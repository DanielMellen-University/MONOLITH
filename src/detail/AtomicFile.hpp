#pragma once

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace monolith::detail {

inline std::atomic<unsigned long long> atomicTempSequence{0};
inline std::atomic<unsigned long long> atomicTempSweepSequence{0};

inline constexpr const char* atomicTempPrefix = ".monolith-tmp-v2-";
inline constexpr unsigned long long atomicTempSweepInterval = 32;

// A ready marker exists only after the writer owns the lease lock.
inline bool tryReclaimAtomicTempDirectory(const std::filesystem::path& directory) {
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

inline void scavengeAtomicTempDirectories(const std::filesystem::path& parent) {
    std::error_code iteratorError;
    std::filesystem::directory_iterator entry(parent, iteratorError);
    const std::filesystem::directory_iterator end;
    std::vector<std::filesystem::path> candidates;
    while (!iteratorError && entry != end) {
        if (entry->path().filename().string().starts_with(atomicTempPrefix)) {
            candidates.push_back(entry->path());
        }
        entry.increment(iteratorError);
    }
    for (const auto& candidate : candidates) {
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
    const std::size_t nameHash = std::hash<std::string>{}(targetPath.filename().string());

    if (atomicTempSweepSequence.fetch_add(1, std::memory_order_relaxed)
        % atomicTempSweepInterval == 0) {
        scavengeAtomicTempDirectories(parent);
    }

    auto prepareNewDirectory = [&](const std::filesystem::path& candidate) {
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
        return true;
    };

    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        const auto sequence = atomicTempSequence.fetch_add(1, std::memory_order_relaxed);
        const auto candidate = parent / (std::string(atomicTempPrefix) + std::to_string(nameHash)
            + "-" + std::to_string(sequence));
        std::error_code createError;
        if (std::filesystem::create_directory(candidate, createError)) {
            return prepareNewDirectory(candidate);
        }
        if (createError && createError != std::errc::file_exists) return false;
        if (!tryReclaimAtomicTempDirectory(candidate)) continue;

        createError.clear();
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

    if (preservePermissions) {
        std::error_code permissionError;
        std::filesystem::permissions(
            tempPath, existingPermissions, std::filesystem::perm_options::replace,
            permissionError);
        if (permissionError) return false;
    }

    try {
        std::forward<Writer>(writer)(out);
    } catch (...) {
        return false;
    }
    out.flush();
    if (!out) return false;
    out.close();
    if (!out) return false;

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
