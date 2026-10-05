#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <ios>
#include <ostream>
#include <streambuf>
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
    explicit AtomicTempParentLock(int pinnedParentFd, bool nonBlocking = false);
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
                               std::string& outWorkspaceName,
                               int& outLeaseFd,
                               int& outParentFd,
                               int& outWorkspaceFd);
bool setAtomicTempFileMode(int pathFd, mode_t mode);
int openAtomicTempContent(int workspaceFd,
                          std::ios_base::openmode openMode);
bool syncAtomicTempFile(int workspaceFd,
                        bool preserveMode,
                        mode_t requestedMode = 0);
bool publishAtomicTempFile(int workspaceFd,
                           std::string_view workspaceName,
                           int parentFd,
                           std::string_view targetName,
                           bool noReplaceTarget,
                           bool* outTargetAlreadyExists);

class AtomicTempOutputBuffer final : public std::streambuf {
public:
    explicit AtomicTempOutputBuffer(int fd);
    AtomicTempOutputBuffer(const AtomicTempOutputBuffer&) = delete;
    AtomicTempOutputBuffer& operator=(const AtomicTempOutputBuffer&) = delete;
    ~AtomicTempOutputBuffer() override;

    bool close();

protected:
    int_type overflow(int_type character) override;
    std::streamsize xsputn(const char* data, std::streamsize size) override;
    int sync() override;
    pos_type seekoff(off_type offset,
                     std::ios_base::seekdir direction,
                     std::ios_base::openmode which) override;
    pos_type seekpos(pos_type position,
                     std::ios_base::openmode which) override;

private:
    bool flushBuffer();

    int fd_{-1};
    bool failed_{false};
    std::array<char, 16 * 1024> buffer_{};
};

struct AtomicTempCleanup {
    std::string workspaceName;
    int leaseFd{-1};
    int parentFd{-1};
    int workspaceFd{-1};

    AtomicTempCleanup(std::string tempWorkspaceName,
                      int tempLeaseFd,
                      int tempParentFd,
                      int tempWorkspaceFd);
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

    std::string tempWorkspaceName;
    int tempLeaseFd = -1;
    int tempParentFd = -1;
    int tempWorkspaceFd = -1;
    if (!createAtomicTempDirectory(targetPath, tempWorkspaceName, tempLeaseFd,
                                   tempParentFd, tempWorkspaceFd)) {
        return false;
    }
    AtomicTempCleanup cleanup(std::move(tempWorkspaceName), tempLeaseFd,
                              tempParentFd, tempWorkspaceFd);
    const int outputFd = openAtomicTempContent(cleanup.workspaceFd, openMode);
    if (outputFd < 0) return false;
    AtomicTempOutputBuffer outputBuffer(outputFd);
    std::ostream out(&outputBuffer);

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
    if (!outputBuffer.close()) return false;

    if (!syncAtomicTempFile(
            cleanup.workspaceFd, preservePermissions,
            preservePermissions ? static_cast<mode_t>(existingPermissions) : 0)) {
        return false;
    }

    AtomicTempParentLock publicationLock(cleanup.parentFd);
    if (!publicationLock.locked()) return false;

    // The callback must not reenter an atomic write to this parent directory.
    if (beforeReplace && !beforeReplace()) return false;
    return publishAtomicTempFile(
        cleanup.workspaceFd, "content", publicationLock.fd(),
        pathBasenameView(targetPath), noReplaceTarget, outTargetAlreadyExists);
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
