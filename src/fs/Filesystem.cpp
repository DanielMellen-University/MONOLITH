#include "Filesystem.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace stdfs = std::filesystem;

namespace monolith::fs {

Filesystem::Filesystem(const std::string& hostRootPath)
    : m_hostRoot(hostRootPath)
{
}

bool Filesystem::initialize() {
    try {
        if (!stdfs::exists(m_hostRoot)) {
            stdfs::create_directories(m_hostRoot);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Filesystem::initialize failed: " << e.what() << std::endl;
        return false;
    }
}

std::string Filesystem::hostRoot() const {
    return m_hostRoot;
}

std::string Filesystem::toHostPath(const std::string& virtualPath) const {
    std::string normalized = normalize(virtualPath);
    // Remove leading slash so it becomes relative to root
    if (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }
    return (stdfs::path(m_hostRoot) / normalized).string();
}

std::string Filesystem::normalize(const std::string& path) const {
    if (path.empty()) return "/";

    // Simple normalization (can be improved later)
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;

    while (std::getline(iss, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
            continue;
        }
        parts.push_back(part);
    }

    std::string result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "/";
        result += parts[i];
    }
    return result;
}

bool Filesystem::exists(const std::string& virtualPath) const {
    try {
        return stdfs::exists(toHostPath(virtualPath));
    } catch (...) {
        return false;
    }
}

bool Filesystem::isFile(const std::string& virtualPath) const {
    try {
        return stdfs::is_regular_file(toHostPath(virtualPath));
    } catch (...) {
        return false;
    }
}

bool Filesystem::isDirectory(const std::string& virtualPath) const {
    try {
        return stdfs::is_directory(toHostPath(virtualPath));
    } catch (...) {
        return false;
    }
}

bool Filesystem::createDirectory(const std::string& virtualPath) {
    try {
        return stdfs::create_directories(toHostPath(virtualPath));
    } catch (const std::exception& e) {
        std::cerr << "createDirectory failed: " << e.what() << std::endl;
        return false;
    }
}

bool Filesystem::remove(const std::string& virtualPath) {
    try {
        return stdfs::remove(toHostPath(virtualPath));
    } catch (...) {
        return false;
    }
}

bool Filesystem::writeFile(const std::string& virtualPath, const std::string& content) {
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        stdfs::create_directories(hostPath.parent_path());

        std::ofstream file(hostPath, std::ios::binary | std::ios::trunc);
        if (!file) return false;

        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return file.good();
    } catch (const std::exception& e) {
        std::cerr << "writeFile failed: " << e.what() << std::endl;
        return false;
    }
}

std::string Filesystem::readFile(const std::string& virtualPath) const {
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_regular_file(hostPath)) return "";

        std::ifstream file(hostPath, std::ios::binary | std::ios::ate);
        if (!file) return "";

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string buffer(size, '\0');
        if (!file.read(&buffer[0], size)) return "";

        return buffer;
    } catch (...) {
        return "";
    }
}

std::vector<std::string> Filesystem::list(const std::string& virtualPath) const {
    std::vector<std::string> entries;
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_directory(hostPath)) return entries;

        for (const auto& entry : stdfs::directory_iterator(hostPath)) {
            entries.push_back(entry.path().filename().string());
        }
    } catch (...) {
        // Return empty on error
    }
    return entries;
}

} // namespace monolith::fs
