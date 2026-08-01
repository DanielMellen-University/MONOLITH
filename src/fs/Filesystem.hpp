#pragma once

#include <cstdint>
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

    /**
     * Removes a file or directory tree (children first).
     * Refuses to remove the virtual root "/".
     */
    bool removeRecursive(const std::string& virtualPath);

    /** Renames or moves a file/directory to a new virtual path. Returns false on failure. */
    bool rename(const std::string& oldVirtualPath, const std::string& newVirtualPath);

    /** Writes (or overwrites) a file with the given content. */
    bool writeFile(const std::string& virtualPath, const std::string& content);

    /** Reads the entire content of a file. Returns empty string on failure. */
    std::string readFile(const std::string& virtualPath) const;

    /**
     * Byte size of a regular file. Returns false if missing or not a regular file.
     */
    bool fileSize(const std::string& virtualPath, std::uint64_t& outBytes) const;

    /**
     * Copies a file or directory tree to a new path.
     * Destination parent directories are created as needed.
     * Returns false if the destination is the same as (or inside) the source tree.
     */
    bool copyRecursive(const std::string& srcVirtualPath, const std::string& dstVirtualPath);

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

    /** Joins a directory and a child name, then normalizes. */
    std::string join(const std::string& dirVirtualPath, const std::string& childName) const;

    /**
     * True if `path` is the same as `ancestor`, or a descendant under it
     * (after normalization). Used to block copy/paste into self.
     */
    bool isSameOrDescendant(const std::string& ancestor, const std::string& path) const;

private:
    std::string m_hostRoot;

    // Converts a virtual path to a real path on the host disk.
    std::string toHostPath(const std::string& virtualPath) const;
};

} // namespace monolith::fs
