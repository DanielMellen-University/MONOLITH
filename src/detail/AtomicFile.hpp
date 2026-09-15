#pragma once

#include <filesystem>
#include <fstream>
#include <ostream>
#include <utility>

namespace monolith::detail {

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

    const std::filesystem::path tempPath = targetPath.string() + ".tmp";
    std::ofstream out(tempPath, std::ios::trunc);
    if (!out) return false;

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
