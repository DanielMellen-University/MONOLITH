#include "AtomicFile.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <linux/fs.h>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace monolith::detail {

bool isAtomicTempWorkspaceName(std::string_view name) {
    return name.starts_with(atomicTempPrefix)
        || name.starts_with(atomicTempPreviousPrefix)
        || name.starts_with(atomicTempOlderPrefix);
}

namespace {

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

std::array<AtomicTempSweepSlot, atomicTempTrackedParents> atomicTempSweepSlots{};
std::mutex atomicTempSweepMutex;
std::size_t atomicTempSweepNextSlot{0};

bool atomicTempSweepSlotMatches(
    const AtomicTempSweepSlot& slot,
    const std::filesystem::path& lexicalKey) {
    if (slot.parent == lexicalKey) return true;
    for (const auto& alias : slot.aliases) {
        if (alias == lexicalKey) return true;
    }
    return false;
}

void rememberAtomicTempSweepAlias(
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

// These tracker helpers are called only while atomicTempSweepMutex is held.
AtomicTempSweepSlot* findAtomicTempSweepSlotLocked(
    const std::filesystem::path& lexicalKey,
    const std::filesystem::path& resolvedKey) {
    for (auto& slot : atomicTempSweepSlots) {
        if (atomicTempSweepSlotMatches(slot, lexicalKey)) return &slot;
    }
    for (auto& slot : atomicTempSweepSlots) {
        if (slot.parent != resolvedKey) continue;
        rememberAtomicTempSweepAlias(slot, lexicalKey);
        return &slot;
    }
    return nullptr;
}

AtomicTempSweepSlot& registerAtomicTempSweepSlotLocked(
    const std::filesystem::path& lexicalKey,
    const std::filesystem::path& resolvedKey,
    unsigned long long operationsSinceSweep) {
    auto& slot = atomicTempSweepSlots[atomicTempSweepNextSlot];
    atomicTempSweepNextSlot = (atomicTempSweepNextSlot + 1) % atomicTempSweepSlots.size();
    slot.traversal.reset();
    slot.parent = resolvedKey;
    slot.aliases = {};
    slot.nextAlias = 0;
    rememberAtomicTempSweepAlias(slot, lexicalKey);
    slot.operationsSinceSweep = operationsSinceSweep;
    return slot;
}

bool atomicTempPathsReferToSameDirectory(
    const std::filesystem::path& first,
    const std::filesystem::path& second) {
    struct stat firstStatus {};
    struct stat secondStatus {};
    return ::stat(first.c_str(), &firstStatus) == 0
        && ::stat(second.c_str(), &secondStatus) == 0
        && S_ISDIR(firstStatus.st_mode)
        && S_ISDIR(secondStatus.st_mode)
        && firstStatus.st_dev == secondStatus.st_dev
        && firstStatus.st_ino == secondStatus.st_ino;
}

void forgetAtomicTempSweepAliasLocked(
    AtomicTempSweepSlot& slot,
    const std::filesystem::path& lexicalKey) {
    for (auto& alias : slot.aliases) {
        if (alias == lexicalKey) alias.clear();
    }
}

// Returns a tracked slot with the mutex held, repairing aliases that changed targets.
AtomicTempSweepSlot* findCurrentAtomicTempSweepSlotLocked(
    const std::filesystem::path& lexicalKey,
    std::unique_lock<std::mutex>& lock,
    std::filesystem::path& resolvedKey,
    bool& resolved) {
    while (true) {
        auto* slot = findAtomicTempSweepSlotLocked(lexicalKey, lexicalKey);
        if (!slot || slot->parent == lexicalKey) break;

        const auto trackedParent = slot->parent;
        lock.unlock();
        const bool aliasStillMatches =
            atomicTempPathsReferToSameDirectory(lexicalKey, trackedParent);
        lock.lock();

        slot = findAtomicTempSweepSlotLocked(lexicalKey, lexicalKey);
        if (!slot || slot->parent != trackedParent) continue;
        if (aliasStillMatches) return slot;

        forgetAtomicTempSweepAliasLocked(*slot, lexicalKey);
        break;
    }

    if (!resolved) {
        lock.unlock();
        std::error_code pathError;
        const auto canonical = std::filesystem::weakly_canonical(lexicalKey, pathError);
        resolvedKey = pathError ? lexicalKey : canonical;
        resolved = true;
        lock.lock();
    }
    return findAtomicTempSweepSlotLocked(lexicalKey, resolvedKey);
}

} // namespace

AtomicTempParentLock::AtomicTempParentLock(
    const std::filesystem::path& parent, bool nonBlocking) {
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

AtomicTempParentLock::~AtomicTempParentLock() { release(); }

bool AtomicTempParentLock::locked() const { return fd_ >= 0; }
int AtomicTempParentLock::fd() const { return fd_; }
int AtomicTempParentLock::error() const { return error_; }

void AtomicTempParentLock::release() {
    if (fd_ < 0) return;
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
}

bool shouldSweepAtomicTempParent(const std::filesystem::path& parent) {
    const auto lexicalKey = parent.lexically_normal();
    auto countOperation = [](AtomicTempSweepSlot& slot) {
        if (slot.operationsSinceSweep >= atomicTempSweepInterval) {
            slot.operationsSinceSweep = 1;
            return true;
        }
        ++slot.operationsSinceSweep;
        return false;
    };

    std::unique_lock lock(atomicTempSweepMutex);
    std::filesystem::path key = lexicalKey;
    bool resolved = false;
    auto* slot = findCurrentAtomicTempSweepSlotLocked(
        lexicalKey, lock, key, resolved);
    if (!slot) {
        registerAtomicTempSweepSlotLocked(lexicalKey, key, 1);
        return true;
    }
    if (slot->traversal) return true;
    return countOperation(*slot);
}

std::string_view pathBasenameView(const std::filesystem::path& path) {
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

void scheduleAtomicTempSweepRetry(const std::filesystem::path& parent) {
    const auto lexicalKey = parent.lexically_normal();
    std::unique_lock lock(atomicTempSweepMutex);
    std::filesystem::path key = lexicalKey;
    bool resolved = false;
    auto* slot = findCurrentAtomicTempSweepSlotLocked(lexicalKey, lock, key, resolved);
    if (!slot) {
        registerAtomicTempSweepSlotLocked(
            lexicalKey, key, atomicTempSweepInterval);
        return;
    }
    slot->operationsSinceSweep = atomicTempSweepInterval;
}

bool hasAtomicTempOwnerMarker(const std::filesystem::path& directory) {
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

bool hasAtomicTempTokenName(std::string_view name) {
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

bool hasAtomicTempTokenName(const std::filesystem::path& directory) {
    return hasAtomicTempTokenName(pathBasenameView(directory));
}

bool createAtomicTempTokenName(std::string& name) {
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

bool createAtomicTempOwnerMarker(const std::filesystem::path& directory) {
    return ::symlink(atomicTempOwnerMarker,
                     (directory / atomicTempOwnerName).c_str()) == 0;
}

bool removeAtomicTempDirectoryIfEmpty(
    const std::filesystem::path& directory) {
    return ::rmdir(directory.c_str()) == 0;
}

bool removeAtomicTempWorkspace(
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
static bool tryReclaimReadyAtomicTempDirectory(
    const std::filesystem::path& directory) {
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

// Classify each candidate once. V4 setup holds the parent lock until ownership,
// lease, and ready markers exist, so an owned incomplete workspace cannot be active.
bool tryScavengeAtomicTempWorkspace(
    const std::filesystem::path& directory,
    const AtomicTempParentLock& parentLock) {
    if (!parentLock.locked()) return false;

    const std::string_view name = pathBasenameView(directory);
    const bool isV4 = name.starts_with(atomicTempPrefix);
    const bool hasOwnerMarker = isV4 && hasAtomicTempOwnerMarker(directory);
    if (isV4 && !hasOwnerMarker && !hasAtomicTempTokenName(name)) return false;

    std::error_code statusError;
    const auto directoryStatus = std::filesystem::symlink_status(directory, statusError);
    if (statusError || !std::filesystem::is_directory(directoryStatus)) return false;

    const auto readyStatus = std::filesystem::symlink_status(directory / "ready", statusError);
    const bool readyMissing = (!statusError
                               && readyStatus.type()
                                   == std::filesystem::file_type::not_found)
        || statusError == std::errc::no_such_file_or_directory;
    if (!readyMissing && statusError) return false;

    if (readyMissing) {
        if (!isV4) return false;
        if (hasOwnerMarker) return removeAtomicTempWorkspace(directory, false);
        // Before the owner symlink is published, the directory is still empty.
        // rmdir semantics make a concurrent/user-added entry fail closed.
        return removeAtomicTempDirectoryIfEmpty(directory);
    }

    if (!std::filesystem::is_regular_file(readyStatus)
        || (isV4 && !hasOwnerMarker)) {
        return false;
    }
    return tryReclaimReadyAtomicTempDirectory(directory);
}

// Keep the directory cursor between operations, but hold the parent lock only
// while advancing one bounded slice.
bool scavengeAtomicTempDirectoryStepLocked(
    const std::filesystem::path& parent,
    const AtomicTempParentLock& parentLock,
    std::size_t entryBudget) {
    if (!parentLock.locked()) {
        scheduleAtomicTempSweepRetry(parent);
        return false;
    }
    if (entryBudget > atomicTempSweepEntryBudget) {
        entryBudget = atomicTempSweepEntryBudget;
    }

    const auto lexicalKey = parent.lexically_normal();
    std::unique_lock lock(atomicTempSweepMutex);
    auto key = lexicalKey;
    bool resolved = false;
    AtomicTempSweepSlot* slot = findCurrentAtomicTempSweepSlotLocked(
        lexicalKey, lock, key, resolved);
    if (!slot) {
        slot = &registerAtomicTempSweepSlotLocked(lexicalKey, key, 1);
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

    std::array<std::filesystem::path, atomicTempSweepEntryBudget> candidates{};
    std::size_t candidateCount = 0;
    std::size_t entriesRead = 0;
    bool complete = false;
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
            complete = true;
            break;
        }
        ++entriesRead;

        const std::string_view name(entry->d_name);
        if (!isAtomicTempWorkspaceName(name)) continue;

        candidates[candidateCount++] = parent / std::filesystem::path(name);
    }

    // Keep filesystem cleanup off the process-wide cursor/tracker mutex.
    lock.unlock();
    for (std::size_t index = 0; index < candidateCount; ++index) {
        tryScavengeAtomicTempWorkspace(candidates[index], parentLock);
    }
    return complete;
}

void scavengeAtomicTempDirectoriesIfDue(const std::filesystem::path& parent) {
    if (!shouldSweepAtomicTempParent(parent)) return;

    const AtomicTempParentLock parentLock(parent);
    if (!parentLock.locked()) {
        scheduleAtomicTempSweepRetry(parent);
        return;
    }
    scavengeAtomicTempDirectoryStepLocked(parent, parentLock);
}

bool createAtomicTempDirectory(const std::filesystem::path& targetPath,
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

AtomicTempCleanup::AtomicTempCleanup(std::filesystem::path tempDirectory,
                                     int tempLeaseFd)
    : directory(std::move(tempDirectory)), leaseFd(tempLeaseFd) {}

AtomicTempCleanup::~AtomicTempCleanup() {
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

bool setAtomicTempFileMode(int pathFd, mode_t mode) {
    struct stat status {};
    if (::fstat(pathFd, &status) != 0 || !S_ISREG(status.st_mode)) return false;

    int result;
    do {
        result = ::fchmodat(pathFd, "", mode, AT_EMPTY_PATH);
    } while (result != 0 && errno == EINTR);
    if (result == 0) return true;
    if (errno != ENOSYS && errno != EINVAL && errno != ENOENT && errno != ENOTSUP) {
        return false;
    }

    const std::string descriptorPath = "/proc/self/fd/" + std::to_string(pathFd);
    do {
        result = ::chmod(descriptorPath.c_str(), mode);
    } while (result != 0 && errno == EINTR);
    return result == 0;
}

bool syncAtomicTempFile(const std::filesystem::path& path) {
    int pathFd;
    do {
        pathFd = ::open(path.c_str(), O_PATH | O_CLOEXEC | O_NOFOLLOW);
    } while (pathFd < 0 && errno == EINTR);
    if (pathFd < 0) return false;

    struct stat pathStatus {};
    const bool regularFile = ::fstat(pathFd, &pathStatus) == 0
        && S_ISREG(pathStatus.st_mode);
    if (!regularFile) {
        ::close(pathFd);
        return false;
    }

    const mode_t finalMode = pathStatus.st_mode & 07777;
    // A restrictive umask may create mode-000 content; adjust the pinned inode,
    // never the pathname that could have been replaced by a symlink.
    if (!setAtomicTempFileMode(pathFd, finalMode | S_IRUSR | S_IWUSR)) {
        ::close(pathFd);
        return false;
    }

    int fd;
    do {
        fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        (void)setAtomicTempFileMode(pathFd, finalMode);
        ::close(pathFd);
        return false;
    }

    struct stat openedStatus {};
    bool sameFile = ::fstat(fd, &openedStatus) == 0
        && S_ISREG(openedStatus.st_mode)
        && openedStatus.st_dev == pathStatus.st_dev
        && openedStatus.st_ino == pathStatus.st_ino;
    if (!sameFile) {
        (void)setAtomicTempFileMode(pathFd, finalMode);
        ::close(fd);
        ::close(pathFd);
        return false;
    }

    int result;
    do {
        result = ::fchmod(fd, finalMode);
    } while (result != 0 && errno == EINTR);
    bool synced = result == 0;
    if (!synced) (void)setAtomicTempFileMode(pathFd, finalMode);
    if (synced) {
        do {
            result = ::fsync(fd);
        } while (result != 0 && errno == EINTR);
        synced = result == 0;
    }
    if (::close(fd) != 0) synced = false;
    if (::close(pathFd) != 0) synced = false;
    return synced;
}

bool syncAtomicParentDirectory(int fd) {
    int result;
    do {
        result = ::fsync(fd);
    } while (result != 0 && errno == EINTR);
    return result == 0;
}

bool publishAtomicTempFile(const std::filesystem::path& source,
                           const std::filesystem::path& target,
                           int parentFd,
                           bool noReplaceTarget,
                           bool* outTargetAlreadyExists) {
    if (noReplaceTarget) {
#if defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
        if (::syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD,
                      target.c_str(), RENAME_NOREPLACE) == 0) {
            (void)syncAtomicParentDirectory(parentFd);
            return true;
        }
        if (errno == EEXIST && outTargetAlreadyExists) {
            *outTargetAlreadyExists = true;
        }
#endif
        return false;
    }

    std::error_code renameError;
    std::filesystem::rename(source, target, renameError);
    if (renameError) return false;
    (void)syncAtomicParentDirectory(parentFd);
    return true;
}

} // namespace monolith::detail
