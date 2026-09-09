#include "Filesystem.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace stdfs = std::filesystem;

namespace monolith::fs {

namespace {

std::string lowercaseAscii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool entryNameLess(const Filesystem::DirEntry& left, const Filesystem::DirEntry& right) {
    const std::string leftLower = lowercaseAscii(left.name);
    const std::string rightLower = lowercaseAscii(right.name);
    if (leftLower != rightLower) return leftLower < rightLower;
    return left.name < right.name;
}

} // namespace

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

bool Filesystem::removeRecursive(const std::string& virtualPath) {
    const std::string path = normalize(virtualPath);
    if (path == "/") {
        return false; // never delete the virtual root
    }

    if (isFile(path)) {
        return remove(path);
    }
    if (!isDirectory(path)) {
        return false;
    }

    for (const auto& entry : listEntries(path)) {
        if (!removeRecursive(join(path, entry.name))) {
            return false;
        }
    }
    return remove(path);
}

bool Filesystem::copyRecursive(const std::string& srcVirtualPath, const std::string& dstVirtualPath) {
    const std::string src = normalize(srcVirtualPath);
    const std::string dst = normalize(dstVirtualPath);

    if (src.empty() || dst.empty()) return false;
    if (isSameOrDescendant(src, dst)) return false;

    if (isFile(src)) {
        std::uint64_t expectedBytes = 0;
        if (!fileSize(src, expectedBytes)) return false;
        const std::string content = readFile(src);
        if (content.size() != expectedBytes) return false;
        return writeFile(dst, content);
    }
    if (!isDirectory(src)) {
        return false;
    }

    if (!createDirectory(dst) && !isDirectory(dst)) {
        return false;
    }

    for (const auto& entry : listEntries(src)) {
        if (!copyRecursive(join(src, entry.name), join(dst, entry.name))) {
            return false;
        }
    }
    return true;
}

bool Filesystem::rename(const std::string& oldVirtualPath, const std::string& newVirtualPath) {
    try {
        stdfs::path oldHost = toHostPath(oldVirtualPath);
        stdfs::path newHost = toHostPath(newVirtualPath);

        // Prevent overwriting existing files/directories
        if (stdfs::exists(newHost)) {
            return false;
        }

        stdfs::rename(oldHost, newHost);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "rename failed: " << e.what() << std::endl;
        return false;
    }
}

bool Filesystem::isValidEntryName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    if (name.find('/') != std::string::npos) return false;
    if (name.find('\0') != std::string::npos) return false;
    return true;
}

bool Filesystem::renameEntry(const std::string& dirVirtualPath,
                             const std::string& oldName,
                             const std::string& newName) {
    if (!isValidEntryName(oldName) || !isValidEntryName(newName)) {
        return false;
    }
    const std::string dir = normalize(dirVirtualPath);
    if (!isDirectory(dir)) return false;
    return rename(join(dir, oldName), join(dir, newName));
}

std::string Filesystem::baseName(const std::string& virtualPath) const {
    const std::string normalized = normalize(virtualPath);
    if (normalized.empty() || normalized == "/") return "";
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) return normalized;
    return normalized.substr(slash + 1);
}

int Filesystem::copyItemsInto(const std::vector<std::string>& srcVirtualPaths,
                              const std::string& destDirVirtualPath) {
    const std::string destDir = normalize(destDirVirtualPath);
    if (!isDirectory(destDir)) return 0;

    int copied = 0;
    for (const auto& srcRaw : srcVirtualPaths) {
        const std::string src = normalize(srcRaw);
        if (!exists(src)) continue;
        const std::string name = baseName(src);
        if (!isValidEntryName(name)) continue;
        const std::string dest = join(destDir, name);
        if (src == dest) continue;
        if (isSameOrDescendant(src, dest)) continue;
        if (exists(dest)) continue;
        if (copyRecursive(src, dest)) {
            ++copied;
        }
    }
    return copied;
}

int Filesystem::moveItemsInto(const std::vector<std::string>& srcVirtualPaths,
                              const std::string& destDirVirtualPath) {
    const std::string destDir = normalize(destDirVirtualPath);
    if (!isDirectory(destDir)) return 0;

    int moved = 0;
    for (const auto& srcRaw : srcVirtualPaths) {
        const std::string src = normalize(srcRaw);
        if (!exists(src)) continue;

        const std::string name = baseName(src);
        if (!isValidEntryName(name)) continue;

        const std::string dest = join(destDir, name);
        if (src == dest || isSameOrDescendant(src, dest) || exists(dest)) continue;
        if (rename(src, dest)) {
            ++moved;
        }
    }
    return moved;
}

bool Filesystem::entryNameMatches(const std::string& name, const std::string& query) {
    if (query.empty()) return true;
    auto lower = [](std::string s) {
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };
    const std::string n = lower(name);
    const std::string q = lower(query);
    return n.find(q) != std::string::npos;
}

std::vector<Filesystem::DirEntry> Filesystem::filterEntries(const std::vector<DirEntry>& entries,
                                                            const std::string& query) {
    if (query.empty()) return entries;
    std::vector<DirEntry> out;
    out.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entryNameMatches(entry.name, query)) {
            out.push_back(entry);
        }
    }
    return out;
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

        const std::streamsize size = file.tellg();
        if (size < 0) return "";
        file.seekg(0, std::ios::beg);

        std::string buffer(size, '\0');
        if (size > 0 && !file.read(buffer.data(), size)) return "";

        return buffer;
    } catch (...) {
        return "";
    }
}

bool Filesystem::fileSize(const std::string& virtualPath, std::uint64_t& outBytes) const {
    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_regular_file(hostPath)) return false;
        outBytes = static_cast<std::uint64_t>(stdfs::file_size(hostPath));
        return true;
    } catch (...) {
        return false;
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

std::vector<Filesystem::DirEntry> Filesystem::listEntries(const std::string& virtualPath) const {
    std::vector<DirEntry> dirs;
    std::vector<DirEntry> files;

    try {
        stdfs::path hostPath = toHostPath(virtualPath);
        if (!stdfs::is_directory(hostPath)) return {};

        for (const auto& entry : stdfs::directory_iterator(hostPath)) {
            DirEntry de;
            de.name = entry.path().filename().string();
            de.isDirectory = entry.is_directory();
            if (de.isDirectory) {
                dirs.push_back(std::move(de));
            } else {
                files.push_back(std::move(de));
            }
        }
    } catch (...) {
        return {};
    }

    // Keep listing order aligned with case-insensitive filtering, with a raw-name tie-break.
    std::sort(dirs.begin(), dirs.end(), entryNameLess);
    std::sort(files.begin(), files.end(), entryNameLess);

    // Directories first, then files
    dirs.insert(dirs.end(), std::make_move_iterator(files.begin()), std::make_move_iterator(files.end()));
    return dirs;
}

std::string Filesystem::join(const std::string& dirVirtualPath, const std::string& childName) const {
    if (childName.empty()) {
        return normalize(dirVirtualPath.empty() ? "/" : dirVirtualPath);
    }
    std::string dir = dirVirtualPath.empty() ? "/" : dirVirtualPath;
    if (dir == "/") {
        return normalize("/" + childName);
    }
    if (!dir.empty() && dir.back() != '/') {
        dir += '/';
    }
    return normalize(dir + childName);
}

bool Filesystem::isSameOrDescendant(const std::string& ancestor, const std::string& path) const {
    const std::string a = normalize(ancestor);
    const std::string p = normalize(path);
    if (a.empty() || p.empty()) return false;
    if (a == p) return true;
    if (a == "/") return true; // everything is under root

    std::string prefix = a;
    if (prefix.back() != '/') prefix += '/';
    return p.compare(0, prefix.size(), prefix) == 0;
}

} // namespace monolith::fs
