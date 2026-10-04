#pragma once

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

namespace monolith::detail {

inline constexpr const char* atomicTempPrefix = ".monolith-tmp-v4-";
inline constexpr const char* atomicTempPreviousPrefix = ".monolith-tmp-v3-";
inline constexpr const char* atomicTempOlderPrefix = ".monolith-tmp-v2-";
inline constexpr char atomicTempOwnerMarker[] = "monolith atomic workspace v4\n";
inline constexpr const char* atomicTempOwnerName = "owner";
inline constexpr unsigned long long atomicTempSweepInterval = 32;
inline constexpr std::size_t atomicTempTrackedParents = 16;

struct AtomicTempSweepSlot {
    std::filesystem::path parent;
    std::filesystem::path alias;
    unsigned long long writesSinceSweep{0};
};

inline std::array<AtomicTempSweepSlot, atomicTempTrackedParents> atomicTempSweepSlots{};
inline std::mutex atomicTempSweepMutex;
inline std::size_t atomicTempSweepNextSlot{0};

class AtomicTempParentLock {
public:
    explicit AtomicTempParentLock(const std::filesystem::path& parent,
                                  bool nonBlocking = false) {
        fd_ = ::open(parent.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
        if (fd_ < 0) return;

        const int flags = LOCK_EX | (nonBlocking ? LOCK_NB : 0);
        int result;
        do {
            result = ::flock(fd_, flags);
        } while (result != 0 && errno == EINTR && !nonBlocking);
        if (result != 0) release();
    }

    AtomicTempParentLock(const AtomicTempParentLock&) = delete;
    AtomicTempParentLock& operator=(const AtomicTempParentLock&) = delete;
    ~AtomicTempParentLock() { release(); }

    bool locked() const { return fd_ >= 0; }

    void release() {
        if (fd_ < 0) return;
        ::flock(fd_, LOCK_UN);
        ::close(fd_);
        fd_ = -1;
    }

private:
    int fd_{-1};
};

inline bool shouldSweepAtomicTempParent(const std::filesystem::path& parent) {
    const auto lexicalKey = parent.lexically_normal();
    auto countWrite = [](AtomicTempSweepSlot& slot) {
        if (slot.writesSinceSweep >= atomicTempSweepInterval) {
            slot.writesSinceSweep = 1;
            return true;
        }
        ++slot.writesSinceSweep;
        return false;
    };

    {
        std::lock_guard lock(atomicTempSweepMutex);
        for (auto& slot : atomicTempSweepSlots) {
            if (slot.parent == lexicalKey || slot.alias == lexicalKey) {
                return countWrite(slot);
            }
        }
    }

    // Resolve only an unseen spelling, keeping ordinary writes on the lexical hot path.
    std::error_code pathError;
    const auto resolvedKey = std::filesystem::weakly_canonical(parent, pathError);
    const auto key = pathError ? lexicalKey : resolvedKey;

    std::lock_guard lock(atomicTempSweepMutex);
    for (auto& slot : atomicTempSweepSlots) {
        if (slot.parent != key) continue;
        slot.alias = lexicalKey;
        return countWrite(slot);
    }

    auto& slot = atomicTempSweepSlots[atomicTempSweepNextSlot];
    atomicTempSweepNextSlot = (atomicTempSweepNextSlot + 1) % atomicTempSweepSlots.size();
    slot.parent = key;
    slot.alias = key == lexicalKey ? std::filesystem::path{} : lexicalKey;
    slot.writesSinceSweep = 1;
    return true;
}

inline bool hasAtomicTempOwnerMarker(const std::filesystem::path& directory) {
    const auto markerPath = directory / atomicTempOwnerName;
    struct stat pathStatus {};
    if (::lstat(markerPath.c_str(), &pathStatus) != 0) return false;

    if (S_ISLNK(pathStatus.st_mode)) {
        std::array<char, sizeof(atomicTempOwnerMarker)> target{};
        ssize_t targetSize;
        do {
            targetSize = ::readlink(markerPath.c_str(), target.data(), target.size());
        } while (targetSize < 0 && errno == EINTR);
        const std::size_t markerSize = std::strlen(atomicTempOwnerMarker);
        return targetSize == static_cast<ssize_t>(markerSize)
            && std::memcmp(target.data(), atomicTempOwnerMarker, markerSize) == 0;
    }
    if (!S_ISREG(pathStatus.st_mode)) return false;

    // Accept older regular-file markers while rejecting non-regular lookalikes.
    const int markerFd = ::open(markerPath.c_str(),
                                O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (markerFd < 0) return false;

    struct stat markerStatus {};
    bool matches = ::fstat(markerFd, &markerStatus) == 0
        && S_ISREG(markerStatus.st_mode)
        && markerStatus.st_size == static_cast<off_t>(std::strlen(atomicTempOwnerMarker));
    std::array<char, sizeof(atomicTempOwnerMarker) - 1> contents{};
    std::size_t bytesRead = 0;
    while (matches && bytesRead < contents.size()) {
        const ssize_t result = ::read(markerFd, contents.data() + bytesRead,
                                     contents.size() - bytesRead);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) {
            matches = false;
            break;
        }
        bytesRead += static_cast<std::size_t>(result);
    }
    if (matches) {
        matches = std::memcmp(contents.data(), atomicTempOwnerMarker,
                              contents.size()) == 0;
    }
    ::close(markerFd);
    return matches;
}

inline bool hasAtomicTempTokenName(const std::filesystem::path& directory) {
    const std::string name = directory.filename().string();
    if (!name.starts_with(atomicTempPrefix)) return false;
    const std::string_view token(name.data() + std::strlen(atomicTempPrefix),
                                 name.size() - std::strlen(atomicTempPrefix));
    if (token.size() != 32) return false;
    for (const char digit : token) {
        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f'))) {
            return false;
        }
    }
    return true;
}

inline bool createAtomicTempTokenName(std::string& name) {
    std::array<unsigned char, 16> token{};
    std::size_t bytesRead = 0;
    while (bytesRead < token.size()) {
        const ssize_t result = ::getrandom(token.data() + bytesRead,
                                           token.size() - bytesRead, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return false;
        bytesRead += static_cast<std::size_t>(result);
    }

    static constexpr char digits[] = "0123456789abcdef";
    name = atomicTempPrefix;
    name.reserve(name.size() + token.size() * 2);
    for (const unsigned char byte : token) {
        name.push_back(digits[byte >> 4]);
        name.push_back(digits[byte & 0x0f]);
    }
    return true;
}

inline bool createAtomicTempOwnerMarker(const std::filesystem::path& directory) {
    return ::symlink(atomicTempOwnerMarker,
                     (directory / atomicTempOwnerName).c_str()) == 0;
}

// A ready marker exists only after the writer owns the lease lock.
inline bool tryReclaimAtomicTempDirectory(const std::filesystem::path& directory) {
    if (directory.filename().string().starts_with(atomicTempPrefix)
        && !hasAtomicTempOwnerMarker(directory)) {
        return false;
    }

    std::error_code statusError;
    const auto directoryStatus = std::filesystem::symlink_status(directory, statusError);
    if (statusError || !std::filesystem::is_directory(directoryStatus)) return false;

    const auto readyPath = directory / "ready";
    const auto readyStatus = std::filesystem::symlink_status(readyPath, statusError);
    if (statusError || !std::filesystem::is_regular_file(readyStatus)) return false;

    const std::filesystem::path leasePath = directory / "lease";
    const int leaseFd = ::open(leasePath.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (leaseFd < 0) return false;
    if (::flock(leaseFd, LOCK_EX | LOCK_NB) != 0) {
        ::close(leaseFd);
        return false;
    }

    struct stat openedLeaseStatus {};
    struct stat currentLeaseStatus {};
    const bool sameLease = ::fstat(leaseFd, &openedLeaseStatus) == 0
        && ::lstat(leasePath.c_str(), &currentLeaseStatus) == 0
        && S_ISREG(currentLeaseStatus.st_mode)
        && openedLeaseStatus.st_dev == currentLeaseStatus.st_dev
        && openedLeaseStatus.st_ino == currentLeaseStatus.st_ino;
    if (!sameLease) {
        ::flock(leaseFd, LOCK_UN);
        ::close(leaseFd);
        return false;
    }

    statusError.clear();
    const auto currentDirectoryStatus = std::filesystem::symlink_status(directory, statusError);
    if (!statusError && std::filesystem::is_directory(currentDirectoryStatus)) {
        std::filesystem::remove_all(directory, statusError);
    }

    ::flock(leaseFd, LOCK_UN);
    ::close(leaseFd);
    return !statusError;
}

// V4 setup holds the parent lock until ownership, lease, and ready markers exist,
// so an incomplete owned directory cannot belong to an active writer while locked.
inline bool tryReclaimIncompleteAtomicTempDirectory(
    const std::filesystem::path& directory) {
    if (!directory.filename().string().starts_with(atomicTempPrefix)) {
        return false;
    }
    const bool hasOwnerMarker = hasAtomicTempOwnerMarker(directory);
    if (!hasOwnerMarker && !hasAtomicTempTokenName(directory)) return false;

    std::error_code statusError;
    const auto directoryStatus = std::filesystem::symlink_status(directory, statusError);
    if (statusError || !std::filesystem::is_directory(directoryStatus)) return false;

    statusError.clear();
    const auto readyStatus = std::filesystem::symlink_status(directory / "ready", statusError);
    if ((!statusError && readyStatus.type() != std::filesystem::file_type::not_found)
        || (statusError && statusError != std::errc::no_such_file_or_directory)) {
        return false;
    }

    if (!hasOwnerMarker) {
        // Before the owner symlink is published, the directory is still empty.
        // rmdir semantics make a concurrent/user-added entry fail closed.
        statusError.clear();
        return std::filesystem::remove(directory, statusError) && !statusError;
    }

    statusError.clear();
    std::filesystem::remove_all(directory, statusError);
    return !statusError;
}

inline void scavengeAtomicTempDirectories(const std::filesystem::path& parent) {
    AtomicTempParentLock parentLock(parent);
    if (!parentLock.locked()) return;

    std::error_code iteratorError;
    std::filesystem::directory_iterator entry(parent, iteratorError);
    const std::filesystem::directory_iterator end;
    std::vector<std::filesystem::path> candidates;
    while (!iteratorError && entry != end) {
        const std::string name = entry->path().filename().string();
        if (name.starts_with(atomicTempPrefix)
            || name.starts_with(atomicTempPreviousPrefix)
            || name.starts_with(atomicTempOlderPrefix)) {
            candidates.push_back(entry->path());
        }
        entry.increment(iteratorError);
    }
    for (const auto& candidate : candidates) {
        if (tryReclaimIncompleteAtomicTempDirectory(candidate)) continue;
        tryReclaimAtomicTempDirectory(candidate);
    }
}

inline bool createAtomicTempDirectory(const std::filesystem::path& targetPath,
                                      std::filesystem::path& outDirectory,
                                      int& outLeaseFd) {
    if (targetPath.filename().empty()) return false;
    const std::filesystem::path parent = targetPath.has_parent_path()
        ? targetPath.parent_path()
        : std::filesystem::path(".");
    if (shouldSweepAtomicTempParent(parent)) scavengeAtomicTempDirectories(parent);

    AtomicTempParentLock parentLock(parent);
    if (!parentLock.locked()) return false;

    auto prepareNewDirectory = [&](const std::filesystem::path& candidate) {
        if (!createAtomicTempOwnerMarker(candidate)) {
            std::error_code cleanupError;
            std::filesystem::remove_all(candidate, cleanupError);
            return false;
        }

        std::error_code permissionError;
        std::filesystem::permissions(candidate, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     permissionError);
        if (permissionError) {
            std::error_code cleanupError;
            std::filesystem::remove_all(candidate, cleanupError);
            return false;
        }

        const std::filesystem::path leasePath = candidate / "lease";
        const int leaseFd = ::open(leasePath.c_str(),
                                   O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW,
                                   S_IRUSR | S_IWUSR);
        if (leaseFd < 0 || ::flock(leaseFd, LOCK_EX | LOCK_NB) != 0) {
            if (leaseFd >= 0) ::close(leaseFd);
            std::error_code cleanupError;
            std::filesystem::remove_all(candidate, cleanupError);
            return false;
        }

        const std::filesystem::path readyPath = candidate / "ready";
        const int readyFd = ::open(readyPath.c_str(),
                                   O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW,
                                   S_IRUSR | S_IWUSR);
        if (readyFd < 0) {
            ::flock(leaseFd, LOCK_UN);
            ::close(leaseFd);
            std::error_code cleanupError;
            std::filesystem::remove_all(candidate, cleanupError);
            return false;
        }
        if (::close(readyFd) != 0) {
            ::flock(leaseFd, LOCK_UN);
            ::close(leaseFd);
            std::error_code cleanupError;
            std::filesystem::remove_all(candidate, cleanupError);
            return false;
        }

        outDirectory = candidate;
        outLeaseFd = leaseFd;
        parentLock.release();
        return true;
    };

    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        std::string candidateName;
        if (!createAtomicTempTokenName(candidateName)) return false;
        const auto candidate = parent / candidateName;
        std::error_code createError;
        if (std::filesystem::create_directory(candidate, createError)) {
            return prepareNewDirectory(candidate);
        }
        if (createError && createError != std::errc::file_exists) return false;
    }
    return false;
}

struct AtomicTempCleanup {
    std::filesystem::path directory;
    int leaseFd{-1};

    AtomicTempCleanup(std::filesystem::path tempDirectory,
                      int tempLeaseFd)
        : directory(std::move(tempDirectory)),
          leaseFd(tempLeaseFd) {}
    AtomicTempCleanup(const AtomicTempCleanup&) = delete;
    AtomicTempCleanup& operator=(const AtomicTempCleanup&) = delete;

    ~AtomicTempCleanup() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        if (leaseFd >= 0) {
            ::flock(leaseFd, LOCK_UN);
            ::close(leaseFd);
        }
    }
};

// Write inside a uniquely reserved hidden sibling workspace, then replace the
// target only after the complete stream has succeeded.
template <typename Writer>
bool writeAtomically(const std::filesystem::path& targetPath,
                     Writer&& writer,
                     bool createParentDirectories = false,
                     std::ios_base::openmode openMode = std::ios_base::out) {
    if (targetPath.empty()) return false;

    if (createParentDirectories && targetPath.has_parent_path()) {
        std::error_code parentError;
        std::filesystem::create_directories(targetPath.parent_path(), parentError);
        if (parentError) return false;
    }

    std::filesystem::perms existingPermissions = std::filesystem::perms::unknown;
    bool preservePermissions = false;
    std::error_code statusError;
    const auto existingStatus = std::filesystem::status(targetPath, statusError);
    if (!statusError && std::filesystem::is_regular_file(existingStatus)) {
        existingPermissions = existingStatus.permissions();
        preservePermissions = true;
    } else if (statusError
               && statusError != std::make_error_code(std::errc::no_such_file_or_directory)) {
        return false;
    }

    std::filesystem::path tempDirectory;
    int tempLeaseFd = -1;
    if (!createAtomicTempDirectory(targetPath, tempDirectory, tempLeaseFd)) return false;
    const std::filesystem::path tempPath = tempDirectory / "content";
    AtomicTempCleanup cleanup(tempDirectory, tempLeaseFd);

    std::ofstream out(tempPath, openMode | std::ios_base::trunc);
    if (!out) return false;

    try {
        std::forward<Writer>(writer)(out);
    } catch (...) {
        return false;
    }
    out.flush();
    if (!out) return false;
    out.close();
    if (!out) return false;

    if (preservePermissions) {
        std::error_code permissionError;
        std::filesystem::permissions(
            tempPath, existingPermissions, std::filesystem::perm_options::replace,
            permissionError);
        if (permissionError) return false;
    }

    std::error_code renameError;
    std::filesystem::rename(tempPath, targetPath, renameError);
    return !renameError;
}

template <typename Writer>
bool writeTextAtomically(const std::filesystem::path& targetPath,
                         Writer&& writer,
                         bool createParentDirectories = false) {
    return writeAtomically(
        targetPath,
        std::forward<Writer>(writer),
        createParentDirectories,
        std::ios_base::out);
}

} // namespace monolith::detail
