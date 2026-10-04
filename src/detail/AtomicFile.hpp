#pragma once

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <dirent.h>
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
inline constexpr std::size_t atomicTempSweepEntryBudget = 32;
inline constexpr std::size_t atomicTempTrackedParents = 16;
inline constexpr std::size_t atomicTempTrackedAliases = 3;

struct AtomicTempDirectoryCloser {
    void operator()(DIR* directory) const noexcept {
        if (directory) ::closedir(directory);
    }
};

struct AtomicTempSweepSlot {
    std::filesystem::path parent;
    std::array<std::filesystem::path, atomicTempTrackedAliases> aliases{};
    std::size_t nextAlias{0};
    unsigned long long operationsSinceSweep{0};
    std::unique_ptr<DIR, AtomicTempDirectoryCloser> traversal;
};

inline std::array<AtomicTempSweepSlot, atomicTempTrackedParents> atomicTempSweepSlots{};
inline std::mutex atomicTempSweepMutex;
inline std::size_t atomicTempSweepNextSlot{0};

inline bool atomicTempSweepSlotMatches(
    const AtomicTempSweepSlot& slot,
    const std::filesystem::path& lexicalKey) {
    if (slot.parent == lexicalKey) return true;
    for (const auto& alias : slot.aliases) {
        if (alias == lexicalKey) return true;
    }
    return false;
}

inline void rememberAtomicTempSweepAlias(
    AtomicTempSweepSlot& slot,
    const std::filesystem::path& lexicalKey) {
    if (slot.parent == lexicalKey) return;
    for (auto& alias : slot.aliases) {
        if (alias == lexicalKey) return;
        if (alias.empty()) {
            alias = lexicalKey;
            return;
        }
    }
    slot.aliases[slot.nextAlias] = lexicalKey;
    slot.nextAlias = (slot.nextAlias + 1) % slot.aliases.size();
}

class AtomicTempParentLock {
public:
    explicit AtomicTempParentLock(const std::filesystem::path& parent,
                                  bool nonBlocking = false) {
        fd_ = ::open(parent.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
        if (fd_ < 0) {
            error_ = errno;
            return;
        }

        const int flags = LOCK_EX | (nonBlocking ? LOCK_NB : 0);
        int result;
        do {
            result = ::flock(fd_, flags);
        } while (result != 0 && errno == EINTR);
        if (result != 0) {
            error_ = errno;
            release();
        }
    }

    AtomicTempParentLock(const AtomicTempParentLock&) = delete;
    AtomicTempParentLock& operator=(const AtomicTempParentLock&) = delete;
    ~AtomicTempParentLock() { release(); }

    bool locked() const { return fd_ >= 0; }
    int fd() const { return fd_; }
    int error() const { return error_; }

    void release() {
        if (fd_ < 0) return;
        ::flock(fd_, LOCK_UN);
        ::close(fd_);
        fd_ = -1;
    }

private:
    int fd_{-1};
    int error_ = 0;
};

inline bool shouldSweepAtomicTempParent(const std::filesystem::path& parent) {
    const auto lexicalKey = parent.lexically_normal();
    auto countOperation = [](AtomicTempSweepSlot& slot) {
        if (slot.operationsSinceSweep >= atomicTempSweepInterval) {
            slot.operationsSinceSweep = 1;
            return true;
        }
        ++slot.operationsSinceSweep;
        return false;
    };

    {
        std::lock_guard lock(atomicTempSweepMutex);
        for (auto& slot : atomicTempSweepSlots) {
            if (atomicTempSweepSlotMatches(slot, lexicalKey)) {
                if (slot.traversal) return true;
                return countOperation(slot);
            }
        }
    }

    // Resolve only an unseen spelling, keeping known-parent operations on the lexical hot path.
    std::error_code pathError;
    const auto resolvedKey = std::filesystem::weakly_canonical(parent, pathError);
    const auto key = pathError ? lexicalKey : resolvedKey;

    std::lock_guard lock(atomicTempSweepMutex);
    for (auto& slot : atomicTempSweepSlots) {
        if (slot.parent != key) continue;
        rememberAtomicTempSweepAlias(slot, lexicalKey);
        if (slot.traversal) return true;
        return countOperation(slot);
    }

    auto& slot = atomicTempSweepSlots[atomicTempSweepNextSlot];
    atomicTempSweepNextSlot = (atomicTempSweepNextSlot + 1) % atomicTempSweepSlots.size();
    slot.traversal.reset();
    slot.parent = key;
    slot.aliases = {};
    slot.nextAlias = 0;
    rememberAtomicTempSweepAlias(slot, lexicalKey);
    slot.operationsSinceSweep = 1;
    return true;
}

inline std::string_view pathBasenameView(const std::filesystem::path& path) {
    const auto& native = path.native();
    std::size_t end = native.size();
    while (end > 0 && native[end - 1] == std::filesystem::path::preferred_separator) {
        --end;
    }
    if (end == 0) return {};
    const auto separator = native.rfind(std::filesystem::path::preferred_separator, end - 1);
    const std::size_t begin = separator == std::string::npos ? 0 : separator + 1;
    return std::string_view(native).substr(begin, end - begin);
}

inline void scheduleAtomicTempSweepRetry(const std::filesystem::path& parent) {
    const auto lexicalKey = parent.lexically_normal();
    std::error_code pathError;
    const auto resolvedKey = std::filesystem::weakly_canonical(parent, pathError);
    const auto key = pathError ? lexicalKey : resolvedKey;

    std::lock_guard lock(atomicTempSweepMutex);
    for (auto& slot : atomicTempSweepSlots) {
        if (slot.parent != key && !atomicTempSweepSlotMatches(slot, lexicalKey)) {
            continue;
        }
        slot.operationsSinceSweep = atomicTempSweepInterval;
        return;
    }

    auto& slot = atomicTempSweepSlots[atomicTempSweepNextSlot];
    atomicTempSweepNextSlot = (atomicTempSweepNextSlot + 1) % atomicTempSweepSlots.size();
    slot.traversal.reset();
    slot.parent = key;
    slot.aliases = {};
    slot.nextAlias = 0;
    rememberAtomicTempSweepAlias(slot, lexicalKey);
    slot.operationsSinceSweep = atomicTempSweepInterval;
}

inline bool hasAtomicTempTokenName(std::string_view name);
inline bool hasAtomicTempTokenName(const std::filesystem::path& directory);

inline bool hasAtomicTempOwnerMarker(const std::filesystem::path& directory) {
    const auto markerPath = directory / atomicTempOwnerName;
    struct stat pathStatus {};
    if (::lstat(markerPath.c_str(), &pathStatus) != 0) return false;

    if (S_ISLNK(pathStatus.st_mode)) {
        if (!hasAtomicTempTokenName(directory)) return false;
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

inline bool hasAtomicTempTokenName(std::string_view name) {
    constexpr std::string_view prefix = atomicTempPrefix;
    if (!name.starts_with(prefix)) return false;
    const std::string_view token = name.substr(prefix.size());
    if (token.size() != 32) return false;
    for (const char digit : token) {
        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f'))) {
            return false;
        }
    }
    return true;
}

inline bool hasAtomicTempTokenName(const std::filesystem::path& directory) {
    return hasAtomicTempTokenName(pathBasenameView(directory));
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

inline bool removeAtomicTempDirectoryIfEmpty(
    const std::filesystem::path& directory) {
    return ::rmdir(directory.c_str()) == 0;
}

inline bool removeAtomicTempWorkspace(
    const std::filesystem::path& directory,
    bool requireReady) {
    const bool requireOwner = pathBasenameView(directory).starts_with(atomicTempPrefix);
    std::error_code directoryError;
    const auto directoryStatus = std::filesystem::symlink_status(directory, directoryError);
    if (directoryError || !std::filesystem::is_directory(directoryStatus)) return false;

    std::filesystem::path ownerPath;
    std::filesystem::path leasePath;
    std::filesystem::path readyPath;
    std::filesystem::path contentPath;
    bool ownerPresent = false;
    bool leasePresent = false;
    bool readyPresent = false;
    bool contentPresent = false;

    std::error_code iteratorError;
    std::filesystem::directory_iterator entry(directory, iteratorError);
    const std::filesystem::directory_iterator end;
    while (!iteratorError && entry != end) {
        const auto entryPath = entry->path();
        const std::string_view name = pathBasenameView(entryPath);
        if (name == atomicTempOwnerName) {
            if (!hasAtomicTempOwnerMarker(directory)) return false;
            ownerPath = entryPath;
            ownerPresent = true;
        } else if (name == "lease" || name == "ready" || name == "content") {
            std::error_code entryError;
            const auto entryStatus = std::filesystem::symlink_status(entryPath, entryError);
            if (entryError || !std::filesystem::is_regular_file(entryStatus)) return false;
            if (name == "lease") {
                leasePath = entryPath;
                leasePresent = true;
            } else if (name == "ready") {
                readyPath = entryPath;
                readyPresent = true;
            } else {
                contentPath = entryPath;
                contentPresent = true;
            }
        } else {
            return false;
        }
        entry.increment(iteratorError);
    }
    if (iteratorError || (requireOwner && !ownerPresent)) return false;
    if (requireReady) {
        if (!leasePresent || !readyPresent) return false;
    } else if (readyPresent || contentPresent) {
        return false;
    }

    const std::array<std::filesystem::path, 4> knownEntries{
        contentPath, readyPath, leasePath, ownerPath};
    for (const auto& path : knownEntries) {
        if (path.empty()) continue;
        std::error_code removeError;
        if (!std::filesystem::remove(path, removeError) || removeError) return false;
    }
    return removeAtomicTempDirectoryIfEmpty(directory);
}

// A ready marker exists only after the writer owns the lease lock.
inline bool tryReclaimAtomicTempDirectory(const std::filesystem::path& directory) {
    if (pathBasenameView(directory).starts_with(atomicTempPrefix)
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
    struct stat leasePathStatus {};
    if (::lstat(leasePath.c_str(), &leasePathStatus) != 0
        || !S_ISREG(leasePathStatus.st_mode)) {
        return false;
    }
    constexpr int leaseFlags = O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
    int leaseFd = ::open(leasePath.c_str(), O_RDONLY | leaseFlags);
    if (leaseFd < 0 && errno == EACCES) {
        leaseFd = ::open(leasePath.c_str(), O_WRONLY | leaseFlags);
    }
    if (leaseFd < 0) return false;
    if (::flock(leaseFd, LOCK_EX | LOCK_NB) != 0) {
        ::close(leaseFd);
        return false;
    }

    struct stat openedLeaseStatus {};
    struct stat currentLeaseStatus {};
    const bool sameLease = ::fstat(leaseFd, &openedLeaseStatus) == 0
        && S_ISREG(openedLeaseStatus.st_mode)
        && ::lstat(leasePath.c_str(), &currentLeaseStatus) == 0
        && S_ISREG(currentLeaseStatus.st_mode)
        && openedLeaseStatus.st_dev == currentLeaseStatus.st_dev
        && openedLeaseStatus.st_ino == currentLeaseStatus.st_ino;
    if (!sameLease) {
        ::flock(leaseFd, LOCK_UN);
        ::close(leaseFd);
        return false;
    }

    const bool reclaimed = removeAtomicTempWorkspace(
        directory, true);

    ::flock(leaseFd, LOCK_UN);
    ::close(leaseFd);
    return reclaimed;
}

// V4 setup holds the parent lock until ownership, lease, and ready markers exist,
// so an incomplete owned directory cannot belong to an active writer while locked.
inline bool tryReclaimIncompleteAtomicTempDirectory(
    const std::filesystem::path& directory) {
    const std::string_view name = pathBasenameView(directory);
    if (!name.starts_with(atomicTempPrefix)) {
        return false;
    }
    const bool hasOwnerMarker = hasAtomicTempOwnerMarker(directory);
    if (!hasOwnerMarker && !hasAtomicTempTokenName(name)) return false;

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
        return removeAtomicTempDirectoryIfEmpty(directory);
    }

    return removeAtomicTempWorkspace(directory, false);
}

inline bool tryScavengeAtomicTempWorkspace(
    const std::filesystem::path& directory,
    const AtomicTempParentLock& parentLock) {
    if (!parentLock.locked()) return false;
    if (tryReclaimIncompleteAtomicTempDirectory(directory)) return true;
    return tryReclaimAtomicTempDirectory(directory);
}

// Keep the directory cursor between operations, but hold the parent lock only
// while advancing one bounded slice.
inline bool scavengeAtomicTempDirectoryStepLocked(
    const std::filesystem::path& parent,
    const AtomicTempParentLock& parentLock,
    std::size_t entryBudget = atomicTempSweepEntryBudget) {
    if (!parentLock.locked()) {
        scheduleAtomicTempSweepRetry(parent);
        return false;
    }

    const auto lexicalKey = parent.lexically_normal();
    std::unique_lock lock(atomicTempSweepMutex);
    auto findLexicalSlot = [&]() -> AtomicTempSweepSlot* {
        for (auto& candidate : atomicTempSweepSlots) {
            if (atomicTempSweepSlotMatches(candidate, lexicalKey)) {
                return &candidate;
            }
        }
        return nullptr;
    };

    AtomicTempSweepSlot* slot = findLexicalSlot();
    auto key = lexicalKey;
    if (!slot) {
        lock.unlock();
        std::error_code pathError;
        const auto resolvedKey = std::filesystem::weakly_canonical(parent, pathError);
        key = pathError ? lexicalKey : resolvedKey;
        lock.lock();
        // The lexical spelling may have been registered while resolution ran.
        slot = findLexicalSlot();
    }
    if (!slot) {
        for (auto& candidate : atomicTempSweepSlots) {
            if (candidate.parent != key) continue;
            rememberAtomicTempSweepAlias(candidate, lexicalKey);
            slot = &candidate;
            break;
        }
    }
    if (!slot) {
        slot = &atomicTempSweepSlots[atomicTempSweepNextSlot];
        atomicTempSweepNextSlot = (atomicTempSweepNextSlot + 1)
            % atomicTempSweepSlots.size();
        slot->traversal.reset();
        slot->parent = key;
        slot->aliases = {};
        slot->nextAlias = 0;
        rememberAtomicTempSweepAlias(*slot, lexicalKey);
        slot->operationsSinceSweep = 1;
    }

    if (!slot->traversal) {
        slot->traversal.reset(::opendir(parent.c_str()));
        if (!slot->traversal) {
            slot->operationsSinceSweep = atomicTempSweepInterval;
            return false;
        }
    }

    struct stat scanStatus {};
    struct stat lockedStatus {};
    if (::fstat(::dirfd(slot->traversal.get()), &scanStatus) != 0
        || ::fstat(parentLock.fd(), &lockedStatus) != 0
        || scanStatus.st_dev != lockedStatus.st_dev
        || scanStatus.st_ino != lockedStatus.st_ino) {
        slot->traversal.reset();
        slot->operationsSinceSweep = atomicTempSweepInterval;
        return false;
    }

    std::size_t entriesRead = 0;
    while (entriesRead < entryBudget) {
        errno = 0;
        const dirent* entry = ::readdir(slot->traversal.get());
        if (!entry) {
            if (errno != 0) {
                slot->traversal.reset();
                slot->operationsSinceSweep = atomicTempSweepInterval;
                return false;
            }
            slot->traversal.reset();
            return true;
        }
        ++entriesRead;

        const std::string_view name(entry->d_name);
        const bool isWorkspace = name.starts_with(atomicTempPrefix)
            || name.starts_with(atomicTempPreviousPrefix)
            || name.starts_with(atomicTempOlderPrefix);
        if (!isWorkspace) continue;

        const std::filesystem::path candidate = parent / std::filesystem::path(name);
        if (tryReclaimIncompleteAtomicTempDirectory(candidate)) continue;
        tryReclaimAtomicTempDirectory(candidate);
    }
    return false;
}

inline bool scavengeAtomicTempDirectoriesLocked(
    const std::filesystem::path& parent,
    const AtomicTempParentLock& parentLock) {
    if (!parentLock.locked()) {
        scheduleAtomicTempSweepRetry(parent);
        return false;
    }

    std::unique_ptr<DIR, AtomicTempDirectoryCloser> directory(
        ::opendir(parent.c_str()));
    if (!directory) {
        scheduleAtomicTempSweepRetry(parent);
        return false;
    }

    int readError = 0;
    while (true) {
        // POSIX leaves errno unchanged at end-of-directory.
        errno = 0;
        const dirent* entry = ::readdir(directory.get());
        if (!entry) {
            readError = errno;
            break;
        }
        const std::string_view name(entry->d_name);
        const bool isWorkspace = name.starts_with(atomicTempPrefix)
            || name.starts_with(atomicTempPreviousPrefix)
            || name.starts_with(atomicTempOlderPrefix);
        if (!isWorkspace) continue;
        const std::filesystem::path candidate = parent / std::filesystem::path(name);
        if (tryReclaimIncompleteAtomicTempDirectory(candidate)) continue;
        tryReclaimAtomicTempDirectory(candidate);
    }
    const bool complete = readError == 0;
    if (!complete) scheduleAtomicTempSweepRetry(parent);
    return complete;
}

inline bool scavengeAtomicTempDirectories(const std::filesystem::path& parent) {
    const AtomicTempParentLock parentLock(parent);
    return scavengeAtomicTempDirectoriesLocked(parent, parentLock);
}

inline void scavengeAtomicTempDirectoriesIfDue(const std::filesystem::path& parent) {
    if (!shouldSweepAtomicTempParent(parent)) return;

    const AtomicTempParentLock parentLock(parent);
    if (!parentLock.locked()) {
        scheduleAtomicTempSweepRetry(parent);
        return;
    }
    scavengeAtomicTempDirectoryStepLocked(parent, parentLock);
}

inline bool createAtomicTempDirectory(const std::filesystem::path& targetPath,
                                      std::filesystem::path& outDirectory,
                                      int& outLeaseFd) {
    if (targetPath.filename().empty()) return false;
    const std::filesystem::path parent = targetPath.has_parent_path()
        ? targetPath.parent_path()
        : std::filesystem::path(".");
    const bool sweepDue = shouldSweepAtomicTempParent(parent);

    AtomicTempParentLock parentLock(parent);
    if (!parentLock.locked()) {
        if (sweepDue) scheduleAtomicTempSweepRetry(parent);
        return false;
    }
    if (sweepDue) scavengeAtomicTempDirectoryStepLocked(parent, parentLock);

    auto prepareNewDirectory = [&](const std::filesystem::path& candidate) {
        std::error_code permissionError;
        std::filesystem::permissions(candidate, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     permissionError);
        if (permissionError) {
            removeAtomicTempDirectoryIfEmpty(candidate);
            return false;
        }

        if (!createAtomicTempOwnerMarker(candidate)) {
            removeAtomicTempDirectoryIfEmpty(candidate);
            return false;
        }

        const std::filesystem::path leasePath = candidate / "lease";
        const int leaseFd = ::open(leasePath.c_str(),
                                   O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW,
                                   S_IRUSR | S_IWUSR);
        if (leaseFd < 0) {
            removeAtomicTempWorkspace(candidate, false);
            return false;
        }
        if (::fchmod(leaseFd, S_IRUSR | S_IWUSR) != 0
            || ::flock(leaseFd, LOCK_EX | LOCK_NB) != 0) {
            ::close(leaseFd);
            removeAtomicTempWorkspace(candidate, false);
            return false;
        }

        const std::filesystem::path readyPath = candidate / "ready";
        const int readyFd = ::open(readyPath.c_str(),
                                   O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW,
                                   S_IRUSR | S_IWUSR);
        if (readyFd < 0) {
            ::flock(leaseFd, LOCK_UN);
            ::close(leaseFd);
            removeAtomicTempWorkspace(candidate, false);
            return false;
        }
        if (::close(readyFd) != 0) {
            ::flock(leaseFd, LOCK_UN);
            ::close(leaseFd);
            removeAtomicTempWorkspace(candidate, true);
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
        int createResult;
        do {
            createResult = ::mkdir(candidate.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
        } while (createResult != 0 && errno == EINTR);
        if (createResult == 0) {
            return prepareNewDirectory(candidate);
        }
        if (errno != EEXIST) return false;
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
        try {
            removeAtomicTempWorkspace(directory, true);
        } catch (...) {
            // Cleanup is best-effort; a destructor must not throw.
        }
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
