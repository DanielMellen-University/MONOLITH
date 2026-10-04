#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace monolith::test {

class ScopedTempDirectory {
public:
    explicit ScopedTempDirectory(const std::string& prefix) {
        std::string candidate = (std::filesystem::temp_directory_path()
            / (prefix + "-XXXXXX")).string();
        if (::mkdtemp(candidate.data())) path_ = std::move(candidate);
    }

    ScopedTempDirectory(const ScopedTempDirectory&) = delete;
    ScopedTempDirectory& operator=(const ScopedTempDirectory&) = delete;

    ~ScopedTempDirectory() {
        if (path_.empty()) return;
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    explicit operator bool() const { return !path_.empty(); }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace monolith::test
