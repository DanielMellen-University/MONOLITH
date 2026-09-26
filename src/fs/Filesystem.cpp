#include "Filesystem.hpp"

#include "../detail/AtomicFile.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace stdfs = std::filesystem;

namespace monolith::fs {

namespace {

std::string lowercaseAscii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool entryNameLess(const Filesystem::DirEntry& left, const Filesystem::DirEntry& right) {
    const std::string leftLower = lowercaseAscii(left.name);
    const std::string rightLower = lowercaseAscii(right.name);
    if (leftLower != rightLower) return leftLower < rightLower;
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

} // namespace

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
        return stdfs::is_directory(root);
    } catch (const std::exception& e) {
        std::cerr << "Filesystem::initialize failed: " << e.what() << std::endl;
        return false;
    }
}

std::string Filesystem::hostRoot() const {
    return m_hostRoot;
}

bool Filesystem::isWithinHostRoot(const std::string& hostPath) const {
    try {
        std::error_code ec;
        const stdfs::path root = stdfs::weakly_canonical(stdfs::path(m_hostRoot), ec);
        if (ec) return false;

        ec.clear();
        const stdfs::path resolved = stdfs::weakly_canonical(stdfs::path(hostPath), ec);
        if (ec) return false;

        auto rootPart = root.begin();
        auto resolvedPart = resolved.begin();
        for (; rootPart != root.end(); ++rootPart, ++resolvedPart) {
            if (resolvedPart == resolved.end() || *rootPart != *resolvedPart) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string Filesystem::toHostPath(const std::string& virtualPath) const {
    std::string normalized = normalize(virtualPath);
    // Remove leading slash so it becomes relative to root
    if (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }
    const std::string hostPath = (stdfs::path(m_hostRoot) / normalized).string();
    if (!isWithinHostRoot(hostPath)) return {};
    return hostPath;
}

std::string Filesystem::normalize(const std::string& path) const {
    if (path.empty()) return "/";

    // Simple normalization (can be improved later)
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;

    while (std::getline(iss, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
            continue;
        }
        parts.push_back(part);
    }

    std::string result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "/";
        result += parts[i];
    }
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

        // Removing a symlink must unlink the directory entry, never follow it
        // and delete the target file or directory instead.
        const stdfs::path rawHostPath = stdfs::path(m_hostRoot) / path.substr(1);
        if (isSymlinkPath(rawHostPath)) {
            std::error_code symlinkError;
            return stdfs::remove(rawHostPath, symlinkError) && !symlinkError;
        }

        const std::string hostPath = toHostPath(path);
        if (hostPath.empty()) return false;
        return stdfs::remove(hostPath);
    } catch (...) {
        return false;
    }
}

bool Filesystem::removeRecursive(const std::string& virtualPath) {
    const std::string path = normalize(virtualPath);
    if (path == "/") {
        return false; // never delete the virtual root
    }

    const stdfs::path rawHostPath = stdfs::path(m_hostRoot) / path.substr(1);
    if (isSymlinkPath(rawHostPath)) {
        std::error_code symlinkError;
        return stdfs::remove(rawHostPath, symlinkError) && !symlinkError;
    }

    const std::string hostPathString = toHostPath(path);
    if (hostPathString.empty()) return false;
    const stdfs::path hostPath(hostPathString);

    if (isFile(path)) {
        return remove(path);
    }
    if (!isDirectory(path)) {
        return false;
    }

    // Use the raw host directory entries here instead of listEntries(). The
    // public listing intentionally hides symlinks that resolve outside the
    // host root, but recursive deletion still needs to unlink those entries
    // so their containing directory can be removed safely.
    std::vector<stdfs::path> children;
    std::error_code iteratorEc;
    for (stdfs::directory_iterator it(hostPath, iteratorEc), end;
         it != end;
         it.increment(iteratorEc)) {
        if (iteratorEc) return false;
        children.push_back(it->path());
    }
    if (iteratorEc) return false;

    for (const auto& childHostPath : children) {
        if (isSymlinkPath(childHostPath)) {
            std::error_code removeEc;
            if (!stdfs::remove(childHostPath, removeEc) || removeEc) {
                return false;
            }
            continue;
        }

        if (!removeRecursive(join(path, childHostPath.filename().string()))) {
            return false;
        }
    }
    return remove(path);
}

bool Filesystem::copyRecursive(const std::string& srcVirtualPath, const std::string& dstVirtualPath) {
    const std::string src = normalize(srcVirtualPath);
    const std::string dst = normalize(dstVirtualPath);

    if (src.empty() || dst.empty()) return false;
    if (isSameOrDescendant(src, dst)) return false;

    const std::string sourceHostPathString = toHostPath(src);
    if (sourceHostPathString.empty()
        || isSymlinkPath(stdfs::path(sourceHostPathString))) {
        return false;
    }

    if (isFile(src)) {
        std::uint64_t expectedBytes = 0;
        if (!fileSize(src, expectedBytes)) return false;
        return writeFileWithProducer(dst, [&](std::ostream& out) {
            std::uint64_t copiedBytes = 0;
            bool sourceValid = true;
            const bool readSucceeded = readFileChunks(src, [&](std::string_view chunk) {
                if (copiedBytes > expectedBytes
                    || chunk.size() > expectedBytes - copiedBytes) {
                    sourceValid = false;
                    return false;
                }
                out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                if (!out) {
                    sourceValid = false;
                    return false;
                }
                copiedBytes += static_cast<std::uint64_t>(chunk.size());
                return true;
            });
            return readSucceeded && sourceValid && copiedBytes == expectedBytes;
        });
    }
    if (!isDirectory(src)) {
        return false;
    }

    const bool destinationExisted = exists(dst);
    if (!createDirectory(dst) && !isDirectory(dst)) {
        return false;
    }

    for (const auto& entry : listEntries(src)) {
        if (!copyRecursive(join(src, entry.name), join(dst, entry.name))) {
            // A new destination is owned by this copy operation. Remove it
            // on failure so callers never mistake a partial tree for success.
            if (!destinationExisted) {
                removeRecursive(dst);
            }
            return false;
        }
    }
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

        const stdfs::path oldRawHost = stdfs::path(m_hostRoot) / oldPath.substr(1);
        const stdfs::path newRawHost = stdfs::path(m_hostRoot) / newPath.substr(1);
        const std::string oldHostPath = toHostPath(oldVirtualPath);
        const std::string newHostPath = toHostPath(newVirtualPath);
        if (oldHostPath.empty() || newHostPath.empty()) return false;
        // Validate through resolved paths for containment, but preserve a
        // final symlink as an entry instead of moving its target.
        stdfs::path oldHost = isSymlinkPath(oldRawHost)
            ? oldRawHost
            : stdfs::path(oldHostPath);
        stdfs::path newHost(newHostPath);

        // Prevent overwriting any existing directory entry, including a
        // dangling symlink that std::filesystem::exists would not report.
        if (hostEntryExists(newRawHost)) {
            return false;
        }

        stdfs::rename(oldHost, newHost);
        return true;
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
        if (!exists(src)) continue;

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
    auto lower = [](std::string s) {
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };
    const std::string n = lower(name);
    const std::string q = lower(query);
    return n.find(q) != std::string::npos;
}

std::vector<Filesystem::DirEntry> Filesystem::filterEntries(const std::vector<DirEntry>& entries,
                                                            const std::string& query) {
    if (query.empty()) return entries;
    std::vector<DirEntry> out;
    out.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entryNameMatches(entry.name, query)) {
            out.push_back(entry);
        }
    }
    return out;
}

bool Filesystem::writeFileWithProducer(
    const std::string& virtualPath,
    const std::function<bool(std::ostream&)>& produceContent) {
    if (!produceContent) return false;

    try {
        const std::string hostPathString = toHostPath(virtualPath);
        if (hostPathString.empty()) return false;

        stdfs::path hostPath(hostPathString);

        // Keep writes to an existing file atomic. Resolve a file symlink first
        // so replacing it updates the target instead of deleting the link.
        stdfs::path writePath = hostPath;
        if (isSymlinkPath(hostPath)) {
            std::error_code resolveEc;
            writePath = stdfs::weakly_canonical(hostPath, resolveEc);
            if (resolveEc || !isWithinHostRoot(writePath.string())) return false;
        }

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

        return monolith::detail::writeAtomically(
            writePath,
            [&produceContent](std::ostream& out) {
                if (!produceContent(out)) {
                    throw std::runtime_error("file content producer failed");
                }
            },
            true,
            std::ios_base::out | std::ios_base::binary);
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

std::vector<std::string> Filesystem::list(const std::string& virtualPath) const {
    std::vector<std::string> entries;
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_directory(hostPath)) return entries;

        for (const auto& entry : stdfs::directory_iterator(hostPath)) {
            if (!isWithinHostRoot(entry.path().string())) continue;
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

        for (const auto& entry : stdfs::directory_iterator(hostPath)) {
            if (!isWithinHostRoot(entry.path().string())) continue;
            DirEntry de;
            de.name = entry.path().filename().string();
            de.isDirectory = entry.is_directory();
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
    if (childName.empty()) {
        return normalize(dirVirtualPath.empty() ? "/" : dirVirtualPath);
    }
    std::string dir = dirVirtualPath.empty() ? "/" : dirVirtualPath;
    if (dir == "/") {
        return normalize("/" + childName);
    }
    if (!dir.empty() && dir.back() != '/') {
        dir += '/';
    }
    return normalize(dir + childName);
}

bool Filesystem::isSameOrDescendant(const std::string& ancestor, const std::string& path) const {
    const std::string a = normalize(ancestor);
    const std::string p = normalize(path);
    if (a.empty() || p.empty()) return false;
    if (a == p) return true;
    if (a == "/") return true; // everything is under root

    std::string prefix = a;
    if (prefix.back() != '/') prefix += '/';
    return p.compare(0, prefix.size(), prefix) == 0;
}

} // namespace monolith::fs
