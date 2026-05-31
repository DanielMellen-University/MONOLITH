#pragma once

#include <string>
#include <vector>

namespace monolith::fs {

/**
 * Basic host-backed filesystem for Monolith.
 *
 * All paths are virtual (relative to an internal root directory on the host).
 * This gives Monolith its own isolated filesystem that is persisted on disk.
 *
 * Example root on disk: ~/.monolith/fs/
 */
class Filesystem {
public:
    /**
     * Constructs a filesystem rooted at the given host directory.
     * The directory will be created if it doesn't exist when initialize() is called.
     */
    explicit Filesystem(const std::string& hostRootPath);

    /**
     * Ensures the root directory exists on disk.
     * Returns true on success.
     */
    bool initialize();

    /** Returns the host path that corresponds to the virtual root "/". */
    std::string hostRoot() const;

    // === Core Operations (virtual paths, e.g. "/home/user/notes.txt") ===

    bool exists(const std::string& virtualPath) const;
    bool isFile(const std::string& virtualPath) const;
    bool isDirectory(const std::string& virtualPath) const;

    /** Creates a directory (and parents if needed). */
    bool createDirectory(const std::string& virtualPath);

    /** Removes a file or empty directory. Returns false on failure. */
    bool remove(const std::string& virtualPath);

    /** Writes (or overwrites) a file with the given content. */
    bool writeFile(const std::string& virtualPath, const std::string& content);

    /** Reads the entire content of a file. Returns empty string on failure. */
    std::string readFile(const std::string& virtualPath) const;

    /** Lists the names of entries in a directory (not full paths). */
    std::vector<std::string> list(const std::string& virtualPath) const;

    /**
     * Entry with basic type information.
     * Used by the graphical filesystem browser (and anything that wants to avoid N isDirectory calls).
     */
    struct DirEntry {
        std::string name;
        bool isDirectory = false;
    };

    /** Lists entries with type info (directories first, then files, both alpha-sorted). */
    std::vector<DirEntry> listEntries(const std::string& virtualPath) const;

    // === Path utilities ===

    /** Normalizes a virtual path (handles .., ., multiple slashes, etc.) */
    std::string normalize(const std::string& path) const;

private:
    std::string m_hostRoot;

    // Converts a virtual path to a real path on the host disk.
    std::string toHostPath(const std::string& virtualPath) const;
};

} // namespace monolith::fs
