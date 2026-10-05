#include "Filesystem.hpp"

#include "../detail/AtomicFile.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <linux/fs.h>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <sys/syscall.h>
#include <unordered_set>
#include <unistd.h>

namespace stdfs = std::filesystem;

namespace monolith::fs {

namespace {

std::string lowercaseAscii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

int compareCaseInsensitive(std::string_view left, std::string_view right) {
    const size_t sharedLength = std::min(left.size(), right.size());
    for (size_t i = 0; i < sharedLength; ++i) {
        const int leftLower = std::tolower(static_cast<unsigned char>(left[i]));
        const int rightLower = std::tolower(static_cast<unsigned char>(right[i]));
        if (leftLower != rightLower) return leftLower < rightLower ? -1 : 1;
    }
    if (left.size() == right.size()) return 0;
    return left.size() < right.size() ? -1 : 1;
}

bool containsCaseInsensitive(std::string_view name, std::string_view lowercaseQuery) {
    if (lowercaseQuery.empty()) return true;
    if (lowercaseQuery.size() > name.size()) return false;

    const size_t lastStart = name.size() - lowercaseQuery.size();
    for (size_t start = 0; start <= lastStart; ++start) {
        size_t offset = 0;
        while (offset < lowercaseQuery.size()
               && std::tolower(static_cast<unsigned char>(name[start + offset]))
                   == static_cast<unsigned char>(lowercaseQuery[offset])) {
            ++offset;
        }
        if (offset == lowercaseQuery.size()) return true;
    }
    return false;
}

bool isDescriptorLimitError(const std::error_code& error) {
    return error.value() == EMFILE || error.value() == ENFILE;
}

bool entryNameLess(const Filesystem::DirEntry& left, const Filesystem::DirEntry& right) {
    const int foldedOrder = compareCaseInsensitive(left.name, right.name);
    if (foldedOrder != 0) return foldedOrder < 0;
    return left.name < right.name;
}

bool isSymlinkPath(const stdfs::path& path) {
    std::error_code ec;
    const auto status = stdfs::symlink_status(path, ec);
    return !ec && stdfs::is_symlink(status);
}

bool hostEntryExists(const stdfs::path& path) {
    std::error_code ec;
    const auto status = stdfs::symlink_status(path, ec);
    return !ec && status.type() != stdfs::file_type::not_found;
}

monolith::fs::FileStamp fileStampFromStat(const struct stat& info) {
    monolith::fs::FileStamp stamp;
    stamp.device = static_cast<std::uint64_t>(info.st_dev);
    stamp.inode = static_cast<std::uint64_t>(info.st_ino);
    stamp.size = static_cast<std::uint64_t>(info.st_size);
    stamp.modifiedSeconds = static_cast<std::int64_t>(info.st_mtim.tv_sec);
    stamp.modifiedNanoseconds = static_cast<std::int64_t>(info.st_mtim.tv_nsec);
    stamp.changedSeconds = static_cast<std::int64_t>(info.st_ctim.tv_sec);
    stamp.changedNanoseconds = static_cast<std::int64_t>(info.st_ctim.tv_nsec);
    return stamp;
}

enum class StampCheck { Match, Conflict, Error };

StampCheck checkHostPathStamp(const stdfs::path& path,
                              const std::optional<monolith::fs::FileStamp>& expected) {
    struct stat info {};
    if (::stat(path.c_str(), &info) == 0) {
        if (!S_ISREG(info.st_mode)) return StampCheck::Conflict;
        return expected && *expected == fileStampFromStat(info)
            ? StampCheck::Match
            : StampCheck::Conflict;
    }
    if (errno == ENOENT) {
        return expected ? StampCheck::Conflict : StampCheck::Match;
    }
    return StampCheck::Error;
}

std::error_code renameWithoutReplacing(const stdfs::path& source,
                                       const stdfs::path& destination) {
#if defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
    if (::syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD,
                  destination.c_str(), RENAME_NOREPLACE) == 0) {
        return {};
    }
    return {errno, std::generic_category()};
#else
    return std::make_error_code(std::errc::operation_not_supported);
#endif
}

std::atomic_uint64_t copyRollbackSequence{0};

struct CopyRollbackBackup {
    stdfs::path target;
    stdfs::path backup;
    stdfs::file_time_type modifiedTime{};
    stdfs::perms permissions = stdfs::perms::unknown;
    bool isHardLink = false;
    bool active = true;
};

bool createCopyRollbackBackup(const stdfs::path& target, CopyRollbackBackup& outBackup) {
    std::error_code statusError;
    const auto targetStatus = stdfs::status(target, statusError);
    if (statusError || !stdfs::is_regular_file(targetStatus)) return false;

    std::error_code timeError;
    const auto modifiedTime = stdfs::last_write_time(target, timeError);
    if (timeError) return false;

    for (int attempt = 0; attempt < 128; ++attempt) {
        stdfs::path backup = target;
        backup += ".monolith-copy-rollback-"
            + std::to_string(copyRollbackSequence.fetch_add(1, std::memory_order_relaxed));

        std::error_code linkError;
        stdfs::create_hard_link(target, backup, linkError);
        if (!linkError) {
            outBackup = {target, std::move(backup), modifiedTime,
                         targetStatus.permissions(), true, true};
            return true;
        }
        if (linkError == std::errc::file_exists) continue;

        std::error_code copyError;
        const bool copied = stdfs::copy_file(
            target, backup, stdfs::copy_options::none, copyError);
        if (copyError == std::errc::file_exists) continue;
        if (!copied || copyError) return false;

        std::error_code permissionError;
        stdfs::permissions(backup, targetStatus.permissions(),
                           stdfs::perm_options::replace, permissionError);
        if (permissionError) {
            std::error_code cleanupError;
            stdfs::remove(backup, cleanupError);
            return false;
        }
        std::error_code backupTimeError;
        stdfs::last_write_time(backup, modifiedTime, backupTimeError);
        if (backupTimeError) {
            std::error_code cleanupError;
            stdfs::remove(backup, cleanupError);
            return false;
        }

        outBackup = {target, std::move(backup), modifiedTime,
                     targetStatus.permissions(), false, true};
        return true;
    }
    return false;
}

bool restoreCopyRollbackBackup(CopyRollbackBackup& backup) {
    std::error_code renameError;
    stdfs::rename(backup.backup, backup.target, renameError);
    if (renameError) return false;
    backup.active = false;
    if (backup.isHardLink) return true;

    std::error_code permissionError;
    stdfs::permissions(backup.target, backup.permissions,
                       stdfs::perm_options::replace, permissionError);
    std::error_code timeError;
    stdfs::last_write_time(backup.target, backup.modifiedTime, timeError);
    return !permissionError && !timeError;
}

void appendNormalizedPathComponents(std::string& result, std::string_view path) {
    for (std::size_t cursor = 0; cursor < path.size();) {
        while (cursor < path.size() && path[cursor] == '/') ++cursor;
        const std::size_t componentStart = cursor;
        while (cursor < path.size() && path[cursor] != '/') ++cursor;
        const std::size_t componentLength = cursor - componentStart;

        if (componentLength == 0
            || (componentLength == 1 && path[componentStart] == '.')) {
            continue;
        }

        if (componentLength == 2
            && path[componentStart] == '.'
            && path[componentStart + 1] == '.') {
            if (result.size() > 1) {
                const std::size_t lastSeparator = result.find_last_of('/');
                result.resize(lastSeparator == 0 ? 1 : lastSeparator);
            }
            continue;
        }

        if (result.size() > 1) result.push_back('/');
        result.append(path.data() + componentStart, componentLength);
    }
}

bool pathHasPrefix(const stdfs::path& prefix, const stdfs::path& path) {
    auto prefixPart = prefix.begin();
    auto pathPart = path.begin();
    for (; prefixPart != prefix.end(); ++prefixPart, ++pathPart) {
        if (pathPart == path.end() || *prefixPart != *pathPart) return false;
    }
    return true;
}

bool resolveWithinRoot(const stdfs::path& root,
                       const stdfs::path& requested,
                       stdfs::path& resolved) {
    std::error_code ec;
    resolved = stdfs::weakly_canonical(requested, ec);
    return !ec && pathHasPrefix(root, resolved);
}

// Resolve only the parent so an in-root final symlink can be unlinked safely.
bool resolveEntryHostPathWithinRoot(const stdfs::path& hostRoot,
                                     const std::string& normalizedVirtualPath,
                                     stdfs::path& resolvedEntry) {
    std::error_code ec;
    const stdfs::path canonicalRoot = stdfs::weakly_canonical(hostRoot, ec);
    if (ec) return false;

    std::string_view relativePath = normalizedVirtualPath;
    if (!relativePath.empty() && relativePath.front() == '/') {
        relativePath.remove_prefix(1);
    }
    const stdfs::path rawEntry = hostRoot / stdfs::path(relativePath);
    const stdfs::path canonicalParent =
        stdfs::weakly_canonical(rawEntry.parent_path(), ec);
    if (ec || !pathHasPrefix(canonicalRoot, canonicalParent)) return false;

    resolvedEntry = canonicalParent / rawEntry.filename();
    return true;
}

bool inspectVisibleEntry(const stdfs::path& root,
                         const stdfs::directory_entry& entry,
                         bool* outIsDirectory = nullptr) {
    std::error_code statusError;
    const auto linkStatus = entry.symlink_status(statusError);
    if (statusError) return false;

    const bool isSymlink = stdfs::is_symlink(linkStatus);
    if (isSymlink) {
        stdfs::path resolved;
        if (!resolveWithinRoot(root, entry.path(), resolved)) return false;
    }

    bool isDirectory = stdfs::is_directory(linkStatus);
    if (isSymlink) {
        const auto targetStatus = entry.status(statusError);
        if (statusError) {
            if (statusError != std::errc::no_such_file_or_directory) return false;
            statusError.clear();
            isDirectory = false;
        } else {
            isDirectory = stdfs::is_directory(targetStatus);
        }
    } else if (isDirectory) {
        stdfs::path resolved;
        if (!resolveWithinRoot(root, entry.path(), resolved)) return false;
    }
    if (outIsDirectory) *outIsDirectory = isDirectory;
    return true;
}

} // namespace

struct Filesystem::CleanupTraversal {
    struct Frame {
        stdfs::directory_iterator current;
    };

    std::vector<Frame> frames;
    std::vector<stdfs::path> deferredDirectories;
    bool finished = true;
};

Filesystem::Filesystem(const std::string& hostRootPath)
    : m_hostRoot(hostRootPath)
{
}

bool Filesystem::initialize() {
    try {
        const stdfs::path root(m_hostRoot);
        if (!stdfs::exists(root)) {
            if (!stdfs::create_directories(root) && !stdfs::is_directory(root)) {
                return false;
            }
        }
        if (!stdfs::is_directory(root)) return false;

        m_cleanupTraversal.reset();
        try {
            auto traversal = std::make_shared<CleanupTraversal>();
            std::error_code iteratorError;
            stdfs::directory_iterator current(
                root, stdfs::directory_options::skip_permission_denied, iteratorError);
            if (!iteratorError && current != stdfs::directory_iterator{}) {
                traversal->frames.push_back({std::move(current)});
                traversal->finished = false;
            } else if (isDescriptorLimitError(iteratorError)) {
                traversal->deferredDirectories.push_back(root);
                traversal->finished = false;
            }
            m_cleanupTraversal = std::move(traversal);
        } catch (...) {
            // Cleanup is best-effort and must not make a usable filesystem fail to initialize.
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Filesystem::initialize failed: " << e.what() << std::endl;
        return false;
    }
}

bool Filesystem::maintenanceStep(std::size_t entryBudget) noexcept {
    const auto traversal = m_cleanupTraversal;
    if (!traversal || traversal->finished) return false;

    try {
        const std::size_t directoryOpenBudget = entryBudget;
        std::size_t deferredDirectoryOpens = 0;
        while (entryBudget > 0
               && (!traversal->frames.empty() || !traversal->deferredDirectories.empty())) {
            if (traversal->frames.empty()) {
                if (deferredDirectoryOpens >= directoryOpenBudget) break;
                ++deferredDirectoryOpens;
                stdfs::path deferredPath = std::move(
                    traversal->deferredDirectories.back());
                traversal->deferredDirectories.pop_back();

                std::error_code deferredError;
                stdfs::directory_iterator deferred(
                    deferredPath, stdfs::directory_options::skip_permission_denied,
                    deferredError);
                if (!deferredError && deferred != stdfs::directory_iterator{}) {
                    traversal->frames.push_back({std::move(deferred)});
                } else if (isDescriptorLimitError(deferredError)) {
                    traversal->deferredDirectories.push_back(std::move(deferredPath));
                    break;
                }
                continue;
            }

            auto& frame = traversal->frames.back();
            if (frame.current == stdfs::directory_iterator{}) {
                traversal->frames.pop_back();
                continue;
            }

            bool isWorkspace = false;
            stdfs::path workspacePath;
            stdfs::path childPath;
            const stdfs::directory_entry& currentEntry = *frame.current;
            const stdfs::path& entryPath = currentEntry.path();
            const std::string_view name = monolith::detail::pathBasenameView(entryPath);
            if (name.starts_with(monolith::detail::atomicTempPrefix)
                || name.starts_with(monolith::detail::atomicTempPreviousPrefix)
                || name.starts_with(monolith::detail::atomicTempOlderPrefix)) {
                workspacePath = entryPath;
                isWorkspace = true;
            } else {
                std::error_code statusError;
                const auto status = currentEntry.symlink_status(statusError);
                if (!statusError && stdfs::is_directory(status)) childPath = entryPath;
            }

            if (isWorkspace) {
                monolith::detail::AtomicTempParentLock parentLock(
                    workspacePath.parent_path(), true);
                if (!parentLock.locked()) {
                    const int lockError = parentLock.error();
                    if (lockError == EWOULDBLOCK || lockError == EAGAIN
                        || lockError == EINTR) {
                        monolith::detail::scheduleAtomicTempSweepRetry(
                            workspacePath.parent_path());
                        return true;
                    }
                    if (isDescriptorLimitError(
                            std::error_code(lockError, std::generic_category()))) {
                        const auto parentPath = workspacePath.parent_path();
                        const bool alreadyDeferred = std::find(
                            traversal->deferredDirectories.begin(),
                            traversal->deferredDirectories.end(), parentPath)
                            != traversal->deferredDirectories.end();
                        if (!alreadyDeferred) {
                            traversal->deferredDirectories.push_back(parentPath);
                        }
                        monolith::detail::scheduleAtomicTempSweepRetry(parentPath);
                    }
                }

                std::error_code iteratorError;
                frame.current.increment(iteratorError);
                --entryBudget;
                if (parentLock.locked()) {
                    monolith::detail::tryScavengeAtomicTempWorkspace(
                        workspacePath, parentLock);
                }
                if (iteratorError) traversal->frames.pop_back();
                continue;
            }

            std::error_code iteratorError;
            frame.current.increment(iteratorError);
            --entryBudget;
            if (iteratorError) {
                traversal->frames.pop_back();
                continue;
            }
            if (childPath.empty()) continue;

            std::error_code childError;
            stdfs::directory_iterator child(
                childPath, stdfs::directory_options::skip_permission_denied, childError);
            if (!childError && child != stdfs::directory_iterator{}) {
                traversal->frames.push_back({std::move(child)});
            } else if (isDescriptorLimitError(childError)) {
                traversal->deferredDirectories.push_back(std::move(childPath));
            }
        }
    } catch (...) {
        traversal->finished = true;
        return false;
    }

    traversal->finished = traversal->frames.empty()
        && traversal->deferredDirectories.empty();
    return !traversal->finished;
}

std::string Filesystem::hostRoot() const {
    return m_hostRoot;
}

bool Filesystem::isWithinHostRoot(const std::string& hostPath) const {
    try {
        std::error_code ec;
        const stdfs::path root = stdfs::weakly_canonical(stdfs::path(m_hostRoot), ec);
        if (ec) return false;
        stdfs::path resolved;
        return resolveWithinRoot(root, stdfs::path(hostPath), resolved);
    } catch (...) {
        return false;
    }
}

std::string Filesystem::toHostPath(const std::string& virtualPath) const {
    std::string normalized = normalize(virtualPath);
    // Remove leading slash so it becomes relative to root
    if (!normalized.empty() && normalized[0] == '/') {
        normalized.erase(0, 1);
    }
    const std::string hostPath = (stdfs::path(m_hostRoot) / normalized).string();
    if (!isWithinHostRoot(hostPath)) return {};
    return hostPath;
}

std::string Filesystem::normalize(const std::string& path) const {
    if (path.empty()) return "/";

    std::string result = "/";
    result.reserve(std::min(path.size(), std::size_t{255}) + 1);
    appendNormalizedPathComponents(result, path);
    return result;
}

bool Filesystem::exists(const std::string& virtualPath) const {
    try {
        const std::string hostPath = toHostPath(virtualPath);
        if (hostPath.empty()) return false;
        return hostEntryExists(stdfs::path(hostPath));
    } catch (...) {
        return false;
    }
}

bool Filesystem::isFile(const std::string& virtualPath) const {
    try {
        return stdfs::is_regular_file(toHostPath(virtualPath));
    } catch (...) {
        return false;
    }
}

bool Filesystem::isDirectory(const std::string& virtualPath) const {
    try {
        return stdfs::is_directory(toHostPath(virtualPath));
    } catch (...) {
        return false;
    }
}

bool Filesystem::createDirectory(const std::string& virtualPath) {
    try {
        const std::string hostPath = toHostPath(virtualPath);
        if (hostPath.empty()) return false;
        return stdfs::create_directories(hostPath);
    } catch (const std::exception& e) {
        std::cerr << "createDirectory failed: " << e.what() << std::endl;
        return false;
    }
}

bool Filesystem::remove(const std::string& virtualPath) {
    try {
        const std::string path = normalize(virtualPath);
        if (path == "/") return false;

        stdfs::path hostEntry;
        if (!resolveEntryHostPathWithinRoot(m_hostRoot, path, hostEntry)) return false;

        std::error_code removeError;
        return stdfs::remove(hostEntry, removeError) && !removeError;
    } catch (...) {
        return false;
    }
}

bool Filesystem::removeRecursive(const std::string& virtualPath) {
    try {
        const std::string path = normalize(virtualPath);
        if (path == "/") return false;

        stdfs::path hostEntry;
        if (!resolveEntryHostPathWithinRoot(m_hostRoot, path, hostEntry)) return false;

        std::error_code statusError;
        const auto entryStatus = stdfs::symlink_status(hostEntry, statusError);
        if (statusError) return false;
        if (stdfs::is_symlink(entryStatus) || stdfs::is_regular_file(entryStatus)) {
            std::error_code removeError;
            return stdfs::remove(hostEntry, removeError) && !removeError;
        }
        if (!stdfs::is_directory(entryStatus)) return false;

        std::error_code removeError;
        const std::uintmax_t removed = stdfs::remove_all(hostEntry, removeError);
        return !removeError && removed != 0;
    } catch (...) {
        return false;
    }
}

bool Filesystem::copyRecursive(const std::string& srcVirtualPath, const std::string& dstVirtualPath) {
    const std::string src = normalize(srcVirtualPath);
    const std::string dst = normalize(dstVirtualPath);

    if (src.empty() || dst.empty()) return false;
    if (isSameOrDescendant(src, dst)) return false;

    std::error_code ec;
    const stdfs::path canonicalRoot = stdfs::weakly_canonical(m_hostRoot, ec);
    if (ec) return false;

    auto resolveVirtualPath = [&](const std::string& virtualPath, HostPath& resolved) {
        std::string relativePath = virtualPath;
        if (!relativePath.empty() && relativePath.front() == '/') {
            relativePath.erase(0, 1);
        }

        const stdfs::path rawPath = stdfs::path(m_hostRoot) / relativePath;
        stdfs::path resolvedPath;
        if (!resolveWithinRoot(canonicalRoot, rawPath, resolvedPath)) return false;
        resolved = {rawPath.string(), resolvedPath.string()};
        return true;
    };

    HostPath source;
    HostPath destination;
    if (!resolveVirtualPath(src, source) || !resolveVirtualPath(dst, destination)) {
        return false;
    }

    std::error_code sourceStatusError;
    const auto sourceStatus = stdfs::symlink_status(source.raw, sourceStatusError);
    if (sourceStatusError || stdfs::is_symlink(sourceStatus)) return false;
    const bool sourceIsDirectory = stdfs::is_directory(sourceStatus);
    if (!sourceIsDirectory && !stdfs::is_regular_file(sourceStatus)) return false;

    return copyRecursiveResolved(
        dst, source, destination, canonicalRoot.string(),
        sourceIsDirectory);
}

bool Filesystem::copyRecursiveResolved(const std::string& dstVirtualPath,
                                       const HostPath& source,
                                       const HostPath& destination,
                                       const std::string& canonicalRootString,
                                       bool sourceIsDirectory) {
    const stdfs::path sourceResolved(source.resolved);
    const stdfs::path destinationResolved(destination.resolved);

    if (pathHasPrefix(sourceResolved, destinationResolved)) return false;

    auto copyFileResolved = [&](const stdfs::path& sourcePath,
                                const std::string& destinationHostPath) {
        std::error_code sizeError;
        const std::uintmax_t expectedBytes = stdfs::file_size(sourcePath, sizeError);
        if (sizeError) return false;

        return writeFileWithProducerAtHostPath(destinationHostPath, [&](std::ostream& out) {
            std::ifstream input(sourcePath, std::ios::binary);
            if (!input) return false;

            std::array<char, 16 * 1024> chunk{};
            std::uintmax_t copiedBytes = 0;
            while (true) {
                input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                const std::streamsize count = input.gcount();
                if (count > 0) {
                    const auto bytes = static_cast<std::uintmax_t>(count);
                    if (copiedBytes > expectedBytes || bytes > expectedBytes - copiedBytes) {
                        return false;
                    }
                    out.write(chunk.data(), count);
                    if (!out) return false;
                    copiedBytes += bytes;
                }
                if (input.eof()) return copiedBytes == expectedBytes;
                if (!input) return false;
            }
        });
    };

    struct DirectoryTimestamp {
        stdfs::path directory;
        stdfs::file_time_type modifiedTime;
    };
    struct DirectoryFrame {
        std::string destinationVirtualPath;
        stdfs::path destinationRaw;
        stdfs::path destinationResolved;
        stdfs::directory_iterator iterator;
        std::error_code iteratorError;
        bool advanceIterator = false;
    };

    std::vector<CopyRollbackBackup> backups;
    std::vector<stdfs::path> createdFiles;
    std::vector<stdfs::path> createdDirectories;
    std::vector<DirectoryTimestamp> directoryTimestamps;
    std::unordered_set<std::string> createdDirectoryKeys;
    std::unordered_set<std::string> timestampedDirectoryKeys;

    auto ensureDestinationDirectory = [&](const stdfs::path& directory) {
        std::vector<stdfs::path> missing;
        stdfs::path existing = directory;
        while (!existing.empty() && !hostEntryExists(existing)) {
            missing.push_back(existing);
            const stdfs::path parent = existing.parent_path();
            if (parent == existing) return false;
            existing = parent;
        }
        if (existing.empty()) return false;

        std::error_code statusError;
        const auto existingStatus = stdfs::status(existing, statusError);
        if (statusError) return false;
        if (!stdfs::is_directory(existingStatus)) return false;

        const std::string existingKey = existing.string();
        if (!createdDirectoryKeys.contains(existingKey)
            && timestampedDirectoryKeys.insert(existingKey).second) {
            std::error_code timeError;
            const auto modifiedTime = stdfs::last_write_time(existing, timeError);
            if (timeError) {
                timestampedDirectoryKeys.erase(existingKey);
                return false;
            }
            directoryTimestamps.push_back({existing, modifiedTime});
        }

        for (auto it = missing.rbegin(); it != missing.rend(); ++it) {
            std::error_code createError;
            const bool created = stdfs::create_directory(*it, createError);
            if (createError) return false;
            if (created) {
                createdDirectories.push_back(*it);
                createdDirectoryKeys.insert(it->string());
            } else {
                std::error_code verifyError;
                if (!stdfs::is_directory(*it, verifyError) || verifyError) return false;
            }
        }

        std::error_code directoryError;
        return stdfs::is_directory(directory, directoryError) && !directoryError;
    };

    auto copyFileWithRollback = [&](const stdfs::path& sourcePath,
                                    const stdfs::path& destinationPath) {
        if (hostEntryExists(destinationPath)) {
            std::error_code statusError;
            const auto status = stdfs::status(destinationPath, statusError);
            if (statusError) return false;
            if (stdfs::is_regular_file(status)) {
                CopyRollbackBackup backup;
                if (!createCopyRollbackBackup(destinationPath, backup)) return false;
                backups.push_back(std::move(backup));
            }
        } else if (!createdDirectoryKeys.contains(destinationPath.parent_path().string())) {
            createdFiles.push_back(destinationPath);
        }

        return copyFileResolved(sourcePath, destinationPath.string());
    };

    auto rollback = [&] {
        for (auto it = createdFiles.rbegin(); it != createdFiles.rend(); ++it) {
            std::error_code removeError;
            stdfs::remove(*it, removeError);
            if (removeError) {
                std::cerr << "copy rollback could not remove " << it->string()
                          << ": " << removeError.message() << '\n';
            }
        }
        for (auto it = backups.rbegin(); it != backups.rend(); ++it) {
            if (!restoreCopyRollbackBackup(*it)) {
                std::cerr << "copy rollback could not restore " << it->target.string()
                          << " from " << it->backup.string() << '\n';
            }
        }
        for (auto it = createdDirectories.rbegin(); it != createdDirectories.rend(); ++it) {
            std::error_code removeError;
            stdfs::remove_all(*it, removeError);
            if (removeError) {
                std::cerr << "copy rollback could not remove " << it->string()
                          << ": " << removeError.message() << '\n';
            }
        }
        for (auto it = directoryTimestamps.rbegin(); it != directoryTimestamps.rend(); ++it) {
            std::error_code timeError;
            stdfs::last_write_time(it->directory, it->modifiedTime, timeError);
            if (timeError) {
                std::cerr << "copy rollback could not restore directory time for "
                          << it->directory.string() << ": " << timeError.message() << '\n';
            }
        }
    };

    auto discardBackups = [&] {
        for (const auto& backup : backups) {
            if (!backup.active) continue;
            std::error_code removeError;
            stdfs::remove(backup.backup, removeError);
            if (removeError) {
                std::cerr << "copy could not remove rollback file "
                          << backup.backup.string() << ": " << removeError.message() << '\n';
            }
        }
    };

    auto failCopy = [&] {
        rollback();
        return false;
    };

    if (!sourceIsDirectory) {
        if (!ensureDestinationDirectory(destinationResolved.parent_path())
            || !copyFileWithRollback(sourceResolved, destinationResolved)) {
            return failCopy();
        }
        discardBackups();
        return true;
    }

    const stdfs::path canonicalRoot(canonicalRootString);
    std::vector<DirectoryFrame> frames;
    if (!ensureDestinationDirectory(destinationResolved)) return failCopy();

    std::error_code rootIteratorError;
    stdfs::directory_iterator rootIterator(sourceResolved, rootIteratorError);
    if (rootIteratorError) return failCopy();
    frames.push_back({
        dstVirtualPath, stdfs::path(destination.raw), destinationResolved,
        std::move(rootIterator), {}, false});

    const stdfs::directory_iterator end;
    while (!frames.empty()) {
        DirectoryFrame& frame = frames.back();
        if (frame.advanceIterator) {
            frame.advanceIterator = false;
            frame.iterator.increment(frame.iteratorError);
        }
        if (frame.iteratorError) return failCopy();
        if (frame.iterator == end) {
            frames.pop_back();
            continue;
        }

        const stdfs::path entryPath = frame.iterator->path();
        std::error_code statusEc;
        const auto entryStatus = frame.iterator->symlink_status(statusEc);
        if (statusEc) return failCopy();

        const std::string childName = entryPath.filename().string();
        const std::string childDestinationVirtual =
            join(frame.destinationVirtualPath, childName);
        if (stdfs::is_symlink(entryStatus)) {
            stdfs::path resolvedLink;
            // Outside-root links stay omitted; links into the virtual tree are
            // rejected rather than copied through to another location.
            if (!resolveWithinRoot(canonicalRoot, entryPath, resolvedLink)) {
                frame.advanceIterator = true;
                continue;
            }
            return failCopy();
        }

        const bool childIsDirectory = stdfs::is_directory(entryStatus);
        if (!childIsDirectory && !stdfs::is_regular_file(entryStatus)) return failCopy();

        const stdfs::path rawDestinationChild = frame.destinationRaw / childName;
        std::error_code destinationStatusError;
        const auto destinationStatus =
            stdfs::symlink_status(rawDestinationChild, destinationStatusError);
        if (destinationStatusError
            && destinationStatusError != std::errc::no_such_file_or_directory) {
            return failCopy();
        }
        const bool childDestinationExists = !destinationStatusError
            && destinationStatus.type() != stdfs::file_type::not_found;

        stdfs::path resolvedDestinationChild;
        if (childDestinationExists && stdfs::is_symlink(destinationStatus)) {
            if (!resolveWithinRoot(
                    canonicalRoot, rawDestinationChild, resolvedDestinationChild)) {
                return failCopy();
            }
        } else {
            resolvedDestinationChild = frame.destinationResolved / childName;
            if (!pathHasPrefix(canonicalRoot, resolvedDestinationChild)) return failCopy();
        }
        if (pathHasPrefix(entryPath, resolvedDestinationChild)) return failCopy();

        if (childIsDirectory) {
            if (!ensureDestinationDirectory(resolvedDestinationChild)) return failCopy();

            std::error_code childIteratorError;
            stdfs::directory_iterator childIterator(entryPath, childIteratorError);
            if (childIteratorError) return failCopy();

            frame.advanceIterator = true;
            frames.push_back({
                childDestinationVirtual, rawDestinationChild, resolvedDestinationChild,
                std::move(childIterator), {}, false});
        } else {
            if (!copyFileWithRollback(entryPath, resolvedDestinationChild)) return failCopy();
            frame.advanceIterator = true;
        }
    }

    discardBackups();
    return true;
}

bool Filesystem::rename(const std::string& oldVirtualPath, const std::string& newVirtualPath) {
    try {
        const std::string oldPath = normalize(oldVirtualPath);
        const std::string newPath = normalize(newVirtualPath);

        // A directory cannot be moved into itself or one of its children.
        // Reject the virtual root too, so a host-root move can never reparent
        // the entire Monolith filesystem.
        if (oldPath == "/" || isSameOrDescendant(oldPath, newPath)) {
            return false;
        }

        stdfs::path oldHost;
        stdfs::path newHost;
        if (!resolveEntryHostPathWithinRoot(m_hostRoot, oldPath, oldHost)
            || !resolveEntryHostPathWithinRoot(m_hostRoot, newPath, newHost)
            || !hostEntryExists(oldHost)) {
            return false;
        }

        const std::error_code renameError = renameWithoutReplacing(oldHost, newHost);
        if (!renameError) return true;
        if (renameError == std::errc::file_exists
            || renameError == std::errc::directory_not_empty) {
            return false;
        }
        throw std::system_error(renameError);
    } catch (const std::exception& e) {
        std::cerr << "rename failed: " << e.what() << std::endl;
        return false;
    }
}

bool Filesystem::isValidEntryName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    if (name.find('/') != std::string::npos) return false;
    if (name.find('\0') != std::string::npos) return false;
    return true;
}

bool Filesystem::renameEntry(const std::string& dirVirtualPath,
                             const std::string& oldName,
                             const std::string& newName) {
    if (!isValidEntryName(oldName) || !isValidEntryName(newName)) {
        return false;
    }
    const std::string dir = normalize(dirVirtualPath);
    if (!isDirectory(dir)) return false;
    return rename(join(dir, oldName), join(dir, newName));
}

std::string Filesystem::baseName(const std::string& virtualPath) const {
    const std::string normalized = normalize(virtualPath);
    if (normalized.empty() || normalized == "/") return "";
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) return normalized;
    return normalized.substr(slash + 1);
}

int Filesystem::copyItemsInto(const std::vector<std::string>& srcVirtualPaths,
                              const std::string& destDirVirtualPath) {
    const std::string destDir = normalize(destDirVirtualPath);
    if (!isDirectory(destDir)) return 0;

    int copied = 0;
    for (const auto& srcRaw : srcVirtualPaths) {
        const std::string src = normalize(srcRaw);
        if (!exists(src)) continue;
        const std::string name = baseName(src);
        if (!isValidEntryName(name)) continue;
        const std::string dest = join(destDir, name);
        if (src == dest) continue;
        if (isSameOrDescendant(src, dest)) continue;
        if (exists(dest)) continue;
        if (copyRecursive(src, dest)) {
            ++copied;
        }
    }
    return copied;
}

int Filesystem::moveItemsInto(const std::vector<std::string>& srcVirtualPaths,
                              const std::string& destDirVirtualPath) {
    const std::string destDir = normalize(destDirVirtualPath);
    if (!isDirectory(destDir)) return 0;

    int moved = 0;
    for (const auto& srcRaw : srcVirtualPaths) {
        const std::string src = normalize(srcRaw);

        const std::string name = baseName(src);
        if (!isValidEntryName(name)) continue;

        const std::string dest = join(destDir, name);
        if (src == dest || isSameOrDescendant(src, dest) || exists(dest)) continue;
        if (rename(src, dest)) {
            ++moved;
        }
    }
    return moved;
}

bool Filesystem::entryNameMatches(const std::string& name, const std::string& query) {
    if (query.empty()) return true;
    return containsCaseInsensitive(name, lowercaseAscii(query));
}

std::vector<Filesystem::DirEntry> Filesystem::filterEntries(const std::vector<DirEntry>& entries,
                                                            const std::string& query) {
    if (query.empty()) return entries;
    const std::string lowercaseQuery = lowercaseAscii(query);
    std::vector<DirEntry> out;
    out.reserve(std::min(entries.size(), std::size_t{64}));
    for (const auto& entry : entries) {
        if (containsCaseInsensitive(entry.name, lowercaseQuery)) {
            out.push_back(entry);
        }
    }
    return out;
}

std::vector<std::size_t> Filesystem::filterEntryIndices(
    const std::vector<DirEntry>& entries, const std::string& query) {
    std::vector<std::size_t> matches;
    if (query.empty()) return matches;

    const std::string lowercaseQuery = lowercaseAscii(query);
    matches.reserve(std::min(entries.size(), std::size_t{64}));
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (containsCaseInsensitive(entries[index].name, lowercaseQuery)) {
            matches.push_back(index);
        }
    }
    return matches;
}

void Filesystem::filterEntryIndicesInPlace(
    const std::vector<DirEntry>& entries,
    const std::string& query,
    std::vector<std::size_t>& candidates) {
    const std::string lowercaseQuery = lowercaseAscii(query);
    auto output = candidates.begin();
    for (const std::size_t index : candidates) {
        if (index < entries.size()
            && containsCaseInsensitive(entries[index].name, lowercaseQuery)) {
            *output++ = index;
        }
    }
    candidates.erase(output, candidates.end());
}

bool Filesystem::writeFileWithProducer(
    const std::string& virtualPath,
    const FileContentProducer& produceContent) {
    if (!produceContent) return false;

    try {
        const std::string hostPathString = toHostPath(virtualPath);
        if (hostPathString.empty()) return false;
        return writeFileWithProducerAtHostPath(hostPathString, produceContent);
    } catch (const std::exception& e) {
        std::cerr << "writeFileWithProducer failed: " << e.what() << std::endl;
        return false;
    }
}

ConditionalWriteResult Filesystem::writeFileWithProducerIfStampMatches(
    const std::string& virtualPath,
    const FileContentProducer& produceContent,
    const std::optional<FileStamp>& expectedStamp) {
    if (!produceContent) return ConditionalWriteResult::Failed;

    bool versionConflict = false;
    try {
        const std::string hostPathString = toHostPath(virtualPath);
        if (hostPathString.empty()) return ConditionalWriteResult::Failed;
        const bool written = writeFileWithProducerAtHostPath(
            hostPathString, produceContent, &expectedStamp, &versionConflict);
        if (versionConflict) return ConditionalWriteResult::Conflict;
        return written ? ConditionalWriteResult::Written : ConditionalWriteResult::Failed;
    } catch (const std::exception& e) {
        std::cerr << "conditional write failed: " << e.what() << std::endl;
        return versionConflict ? ConditionalWriteResult::Conflict
                               : ConditionalWriteResult::Failed;
    }
}

bool Filesystem::writeFileWithProducerAtHostPath(
    const std::string& hostPathString,
    const FileContentProducer& produceContent,
    const std::optional<FileStamp>* expectedStamp,
    bool* outVersionConflict) {
    if (!produceContent || hostPathString.empty()) return false;
    if (outVersionConflict) *outVersionConflict = false;

    try {
        stdfs::path hostPath(hostPathString);

        // Keep writes to an existing file atomic. Resolve a file symlink first
        // so replacing it updates the target instead of deleting the link.
        stdfs::path writePath = hostPath;
        const bool hostPathIsSymlink = isSymlinkPath(hostPath);
        if (hostPathIsSymlink) {
            std::error_code resolveEc;
            writePath = stdfs::weakly_canonical(hostPath, resolveEc);
            if (resolveEc || !isWithinHostRoot(writePath.string())) return false;
        }

        auto versionMatches = [&]() {
            if (!expectedStamp) return true;
            if (isSymlinkPath(hostPath) != hostPathIsSymlink) {
                if (outVersionConflict) *outVersionConflict = true;
                return false;
            }
            if (hostPathIsSymlink) {
                std::error_code resolveEc;
                const stdfs::path currentWritePath =
                    stdfs::weakly_canonical(hostPath, resolveEc);
                if (resolveEc) return false;
                if (currentWritePath != writePath) {
                    if (outVersionConflict) *outVersionConflict = true;
                    return false;
                }
            }
            const StampCheck check = checkHostPathStamp(writePath, *expectedStamp);
            if (check == StampCheck::Conflict && outVersionConflict) {
                *outVersionConflict = true;
            }
            return check == StampCheck::Match;
        };
        if (!versionMatches()) return false;

        std::error_code statusEc;
        const auto existingStatus = stdfs::status(writePath, statusEc);
        const bool existing = !statusEc
            && existingStatus.type() != stdfs::file_type::not_found;
        if (existing && !stdfs::is_regular_file(existingStatus)) return false;
        if (existing) {
            const auto writable = existingStatus.permissions() & (
                stdfs::perms::owner_write
                | stdfs::perms::group_write
                | stdfs::perms::others_write);
            if (writable == stdfs::perms::none) return false;
        }

        bool targetAppearedDuringReplace = false;
        const bool written = monolith::detail::writeAtomically(
            writePath,
            [&produceContent](std::ostream& out) {
                if (!produceContent(out)) {
                    throw std::runtime_error("file content producer failed");
                }
            },
            true,
            std::ios_base::out | std::ios_base::binary,
            expectedStamp
                ? std::function<bool()>(versionMatches)
                : std::function<bool()>{},
            expectedStamp && !*expectedStamp,
            &targetAppearedDuringReplace);
        if (targetAppearedDuringReplace && outVersionConflict) {
            *outVersionConflict = true;
        }
        return written;
    } catch (const std::exception& e) {
        std::cerr << "writeFileWithProducer failed: " << e.what() << std::endl;
        return false;
    }
}

bool Filesystem::writeFile(const std::string& virtualPath, const std::string& content) {
    return writeFileWithProducer(virtualPath, [&content](std::ostream& out) {
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return static_cast<bool>(out);
    });
}

bool Filesystem::updateModifiedTime(const std::string& virtualPath) {
    try {
        const std::string hostPath = toHostPath(virtualPath);
        if (hostPath.empty()) return false;

        std::error_code ec;
        const stdfs::path path(hostPath);
        if (!stdfs::is_regular_file(path, ec) || ec) return false;

        stdfs::last_write_time(
            path, stdfs::file_time_type::clock::now(), ec);
        return !ec;
    } catch (...) {
        return false;
    }
}

std::string Filesystem::readFile(const std::string& virtualPath) const {
    std::string content;
    if (!readFile(virtualPath, content)) return "";
    return content;
}

bool Filesystem::readFile(const std::string& virtualPath, std::string& outContent) const {
    outContent.clear();
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_regular_file(hostPath)) return false;

        std::ifstream file(hostPath, std::ios::binary | std::ios::ate);
        if (!file) return false;

        const std::streamsize size = file.tellg();
        if (size < 0) return false;
        file.seekg(0, std::ios::beg);
        if (!file) return false;

        std::string buffer(static_cast<size_t>(size), '\0');
        if (size > 0 && !file.read(buffer.data(), size)) return false;

        outContent = std::move(buffer);
        return true;
    } catch (...) {
        return false;
    }
}

bool Filesystem::readFileChunks(const std::string& virtualPath,
                                const FileChunkConsumer& consumeChunk) const {
    if (!consumeChunk) return false;

    try {
        const stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_regular_file(hostPath)) return false;

        std::ifstream file(hostPath, std::ios::binary);
        if (!file) return false;

        std::array<char, 16 * 1024> chunk{};
        while (true) {
            file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            const std::streamsize count = file.gcount();
            if (count > 0
                && !consumeChunk(std::string_view(
                    chunk.data(), static_cast<std::size_t>(count)))) {
                return true;
            }
            if (file.eof()) return true;
            if (!file) return false;
        }
    } catch (...) {
        return false;
    }
}

bool Filesystem::readFileTailChunks(const std::string& virtualPath,
                                    std::uint64_t maxBytes,
                                    const FileChunkConsumer& consumeChunk,
                                    bool& outPrefixSkipped) const {
    outPrefixSkipped = false;
    if (!consumeChunk) return false;

    try {
        const stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_regular_file(hostPath)) return false;

        std::ifstream file(hostPath, std::ios::binary);
        if (!file) return false;
        file.seekg(0, std::ios::end);
        const std::streampos end = file.tellg();
        if (!file || end == std::streampos(-1)) return false;

        const std::streamoff endOffset = static_cast<std::streamoff>(end);
        if (endOffset < 0) return false;
        const std::uint64_t fileBytes = static_cast<std::uint64_t>(endOffset);
        const std::uint64_t startOffset = fileBytes > maxBytes ? fileBytes - maxBytes : 0;
        outPrefixSkipped = startOffset > 0;
        file.seekg(static_cast<std::streamoff>(startOffset), std::ios::beg);
        if (!file) return false;

        std::array<char, 16 * 1024> chunk{};
        std::uint64_t remainingBytes = fileBytes - startOffset;
        while (remainingBytes > 0) {
            const auto requested = static_cast<std::streamsize>(
                std::min<std::uint64_t>(remainingBytes, chunk.size()));
            file.read(chunk.data(), requested);
            const std::streamsize count = file.gcount();
            if (count != requested) return false;
            if (!consumeChunk(std::string_view(
                    chunk.data(), static_cast<std::size_t>(count)))) {
                return true;
            }
            remainingBytes -= static_cast<std::uint64_t>(count);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool Filesystem::fileSize(const std::string& virtualPath, std::uint64_t& outBytes) const {
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_regular_file(hostPath)) return false;
        outBytes = static_cast<std::uint64_t>(stdfs::file_size(hostPath));
        return true;
    } catch (...) {
        return false;
    }
}

bool Filesystem::fileStamp(const std::string& virtualPath, FileStamp& outStamp) const {
    try {
        const std::string hostPath = toHostPath(virtualPath);
        if (hostPath.empty()) return false;

        struct stat info {};
        if (::stat(hostPath.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) return false;

        FileStamp stamp;
        stamp.device = static_cast<std::uint64_t>(info.st_dev);
        stamp.inode = static_cast<std::uint64_t>(info.st_ino);
        stamp.size = static_cast<std::uint64_t>(info.st_size);
        stamp.modifiedSeconds = static_cast<std::int64_t>(info.st_mtim.tv_sec);
        stamp.modifiedNanoseconds = static_cast<std::int64_t>(info.st_mtim.tv_nsec);
        stamp.changedSeconds = static_cast<std::int64_t>(info.st_ctim.tv_sec);
        stamp.changedNanoseconds = static_cast<std::int64_t>(info.st_ctim.tv_nsec);
        outStamp = stamp;
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> Filesystem::list(const std::string& virtualPath) const {
    std::vector<std::string> entries;
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_directory(hostPath)) return entries;
        monolith::detail::scavengeAtomicTempDirectoriesIfDue(hostPath);

        std::error_code rootError;
        const stdfs::path root = stdfs::weakly_canonical(m_hostRoot, rootError);
        if (rootError) return entries;

        for (const auto& entry : stdfs::directory_iterator(hostPath)) {
            if (!inspectVisibleEntry(root, entry)) continue;
            entries.push_back(entry.path().filename().string());
        }
    } catch (...) {
        // Return empty on error
    }
    return entries;
}

std::vector<Filesystem::DirEntry> Filesystem::listEntries(const std::string& virtualPath) const {
    std::vector<DirEntry> dirs;
    std::vector<DirEntry> files;

    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_directory(hostPath)) return {};
        monolith::detail::scavengeAtomicTempDirectoriesIfDue(hostPath);

        std::error_code rootError;
        const stdfs::path root = stdfs::weakly_canonical(m_hostRoot, rootError);
        if (rootError) return {};

        for (const auto& entry : stdfs::directory_iterator(hostPath)) {
            bool isDirectory = false;
            if (!inspectVisibleEntry(root, entry, &isDirectory)) continue;

            DirEntry de;
            de.name = entry.path().filename().string();
            de.isDirectory = isDirectory;
            if (de.isDirectory) {
                dirs.push_back(std::move(de));
            } else {
                files.push_back(std::move(de));
            }
        }
    } catch (...) {
        return {};
    }

    // Keep listing order aligned with case-insensitive filtering, with a raw-name tie-break.
    std::sort(dirs.begin(), dirs.end(), entryNameLess);
    std::sort(files.begin(), files.end(), entryNameLess);

    // Directories first, then files
    dirs.insert(dirs.end(), std::make_move_iterator(files.begin()), std::make_move_iterator(files.end()));
    return dirs;
}

std::string Filesystem::join(const std::string& dirVirtualPath, const std::string& childName) const {
    const std::string_view directory = dirVirtualPath.empty()
        ? std::string_view("/")
        : std::string_view(dirVirtualPath);
    const std::size_t initialPathBytes = std::min(directory.size(), std::size_t{255})
        + std::min(childName.size(), std::size_t{255});

    std::string result = "/";
    result.reserve(std::min(initialPathBytes, std::size_t{255}) + 1);
    appendNormalizedPathComponents(result, directory);
    appendNormalizedPathComponents(result, childName);
    return result;
}

bool Filesystem::isSameOrDescendant(const std::string& ancestor, const std::string& path) const {
    const std::string a = normalize(ancestor);
    const std::string p = normalize(path);
    if (a.empty() || p.empty()) return false;
    if (a == p) return true;
    if (a == "/") return true; // everything is under root
    return p.size() > a.size()
        && p.compare(0, a.size(), a) == 0
        && p[a.size()] == '/';
}

} // namespace monolith::fs
