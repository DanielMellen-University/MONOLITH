#pragma once

#include <filesystem>
#include <fstream>
#include <ostream>
#include <system_error>
#include <utility>

namespace monolith::detail {

// A temporary sibling is an implementation detail, not a file that callers
// should be able to redirect through a symlink. Existing regular temp files
// may be replaced after an interrupted write; every other existing entry is
// rejected.
inline bool isSafeAtomicTempPath(const std::filesystem::path& tempPath) {
    std::error_code statusError;
    const auto status = std::filesystem::symlink_status(tempPath, statusError);
    if (statusError
        && statusError != std::make_error_code(std::errc::no_such_file_or_directory)) {
        return false;
    }
    return status.type() == std::filesystem::file_type::not_found
        || std::filesystem::is_regular_file(status);
}

// Write text to a sibling temporary file, then replace the target only after
// the complete stream has succeeded.
template <typename Writer>
bool writeTextAtomically(const std::filesystem::path& targetPath,
                         Writer&& writer,
                         bool createParentDirectories = false) {
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

    const std::filesystem::path tempPath = targetPath.string() + ".tmp";
    if (!isSafeAtomicTempPath(tempPath)) return false;

    std::ofstream out(tempPath, std::ios::trunc);
    if (!out) return false;

    if (preservePermissions) {
        std::error_code permissionError;
        std::filesystem::permissions(
            tempPath, existingPermissions, std::filesystem::perm_options::replace,
            permissionError);
        if (permissionError) {
            out.close();
            std::error_code cleanupError;
            std::filesystem::remove(tempPath, cleanupError);
            return false;
        }
    }

    std::forward<Writer>(writer)(out);
    out.flush();
    if (!out) {
        out.close();
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
        return false;
    }
    out.close();
    if (!out) {
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
        return false;
    }

    if (!isSafeAtomicTempPath(tempPath)) {
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
        return false;
    }

    std::error_code renameError;
    std::filesystem::rename(tempPath, targetPath, renameError);
    if (renameError) {
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
        return false;
    }
    return true;
}

} // namespace monolith::detail
