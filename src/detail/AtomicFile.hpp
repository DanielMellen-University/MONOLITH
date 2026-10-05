#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include <sys/types.h>

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

bool isAtomicTempWorkspaceName(std::string_view name);
std::string_view pathBasenameView(const std::filesystem::path& path);
bool shouldSweepAtomicTempParent(const std::filesystem::path& parent);
void scheduleAtomicTempSweepRetry(const std::filesystem::path& parent);

bool hasAtomicTempOwnerMarker(const std::filesystem::path& directory);
bool hasAtomicTempTokenName(std::string_view name);
bool hasAtomicTempTokenName(const std::filesystem::path& directory);
bool createAtomicTempTokenName(std::string& name);
bool createAtomicTempOwnerMarker(const std::filesystem::path& directory);
bool removeAtomicTempDirectoryIfEmpty(const std::filesystem::path& directory);
bool removeAtomicTempWorkspace(const std::filesystem::path& directory,
                               bool requireReady);
bool tryScavengeAtomicTempWorkspace(const std::filesystem::path& directory,
                                    const class AtomicTempParentLock& parentLock);
bool scavengeAtomicTempDirectoryStepLocked(
    const std::filesystem::path& parent,
    const class AtomicTempParentLock& parentLock,
    std::size_t entryBudget = atomicTempSweepEntryBudget);
void scavengeAtomicTempDirectoriesIfDue(const std::filesystem::path& parent);

class AtomicTempParentLock {
public:
    explicit AtomicTempParentLock(const std::filesystem::path& parent,
                                  bool nonBlocking = false);
    AtomicTempParentLock(const AtomicTempParentLock&) = delete;
    AtomicTempParentLock& operator=(const AtomicTempParentLock&) = delete;
    ~AtomicTempParentLock();

    bool locked() const;
    int fd() const;
    int error() const;
    void release();

private:
    int fd_{-1};
    int error_ = 0;
};

bool createAtomicTempDirectory(const std::filesystem::path& targetPath,
                               std::filesystem::path& outDirectory,
                               int& outLeaseFd);
bool setAtomicTempFileMode(int pathFd, mode_t mode);
bool syncAtomicTempFile(const std::filesystem::path& path);
bool publishAtomicTempFile(const std::filesystem::path& source,
                           const std::filesystem::path& target,
                           int parentFd,
                           bool noReplaceTarget,
                           bool* outTargetAlreadyExists);

struct AtomicTempCleanup {
    std::filesystem::path directory;
    int leaseFd{-1};

    AtomicTempCleanup(std::filesystem::path tempDirectory, int tempLeaseFd);
    AtomicTempCleanup(const AtomicTempCleanup&) = delete;
    AtomicTempCleanup& operator=(const AtomicTempCleanup&) = delete;
    ~AtomicTempCleanup();
};

// Write inside a uniquely reserved hidden sibling workspace, then replace the
// target only after the complete stream has succeeded.
template <typename Writer>
bool writeAtomically(const std::filesystem::path& targetPath,
                     Writer&& writer,
                     bool createParentDirectories = false,
                     std::ios_base::openmode openMode = std::ios_base::out,
                     const std::function<bool()>& beforeReplace = {},
                     bool noReplaceTarget = false,
                     bool* outTargetAlreadyExists = nullptr) {
    if (outTargetAlreadyExists) *outTargetAlreadyExists = false;
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
        using Result = std::invoke_result_t<Writer, std::ostream&>;
        if constexpr (std::is_void_v<Result>) {
            std::forward<Writer>(writer)(out);
        } else if (!static_cast<bool>(std::forward<Writer>(writer)(out))) {
            return false;
        }
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
    if (!syncAtomicTempFile(tempPath)) return false;

    const std::filesystem::path parentPath = targetPath.has_parent_path()
        ? targetPath.parent_path()
        : std::filesystem::path(".");
    AtomicTempParentLock publicationLock(parentPath);
    if (!publicationLock.locked()) return false;

    // The callback must not reenter an atomic write to this parent directory.
    if (beforeReplace && !beforeReplace()) return false;
    return publishAtomicTempFile(tempPath, targetPath, publicationLock.fd(),
                                 noReplaceTarget, outTargetAlreadyExists);
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
