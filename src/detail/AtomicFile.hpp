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

namespace monolith::detail {

inline std::atomic<unsigned long long> atomicTempSequence{0};

inline bool createAtomicTempDirectory(const std::filesystem::path& targetPath,
                                      std::filesystem::path& outDirectory) {
    if (targetPath.filename().empty()) return false;
    const std::filesystem::path parent = targetPath.has_parent_path()
        ? targetPath.parent_path()
        : std::filesystem::path(".");
    const std::size_t nameHash = std::hash<std::string>{}(targetPath.filename().string());

    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        const auto sequence = atomicTempSequence.fetch_add(1, std::memory_order_relaxed);
        const auto candidate = parent / (".monolith-tmp-" + std::to_string(nameHash)
            + "-" + std::to_string(sequence));
        std::error_code createError;
        if (!std::filesystem::create_directory(candidate, createError)) {
            if (!createError || createError == std::errc::file_exists) continue;
            return false;
        }

        std::error_code permissionError;
        std::filesystem::permissions(candidate, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     permissionError);
        if (permissionError) {
            std::error_code cleanupError;
            std::filesystem::remove(candidate, cleanupError);
            return false;
        }
        outDirectory = candidate;
        return true;
    }
    return false;
}

struct AtomicTempCleanup {
    std::filesystem::path file;
    std::filesystem::path directory;

    AtomicTempCleanup(std::filesystem::path tempFile,
                      std::filesystem::path tempDirectory)
        : file(std::move(tempFile)), directory(std::move(tempDirectory)) {}
    AtomicTempCleanup(const AtomicTempCleanup&) = delete;
    AtomicTempCleanup& operator=(const AtomicTempCleanup&) = delete;

    ~AtomicTempCleanup() {
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        ignored.clear();
        std::filesystem::remove(directory, ignored);
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
    if (!createAtomicTempDirectory(targetPath, tempDirectory)) return false;
    const std::filesystem::path tempPath = tempDirectory / "content";
    AtomicTempCleanup cleanup(tempPath, tempDirectory);

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
