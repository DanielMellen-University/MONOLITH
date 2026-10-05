#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace monolith::fs {

struct FileStamp {
    std::uint64_t device = 0;
    std::uint64_t inode = 0;
    std::uint64_t size = 0;
    std::int64_t modifiedSeconds = 0;
    std::int64_t modifiedNanoseconds = 0;
    std::int64_t changedSeconds = 0;
    std::int64_t changedNanoseconds = 0;

    bool operator==(const FileStamp&) const = default;
};

enum class ConditionalWriteResult { Written, Conflict, Failed };

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
    using FileChunkConsumer = std::function<bool(std::string_view)>;
    using FileContentProducer = std::function<bool(std::ostream&)>;

    /**
     * Constructs a filesystem rooted at the given host directory.
     * The directory will be created if it doesn't exist when initialize() is called.
     */
    explicit Filesystem(const std::string& hostRootPath);

    /**
     * Ensures the root directory exists on disk.
     * Returns false when the configured root cannot be created or is not a directory.
     */
    bool initialize();

    /**
     * Performs bounded background filesystem maintenance. Returns true while
     * startup cleanup still has entries to inspect.
     */
    bool maintenanceStep(std::size_t entryBudget = 32) noexcept;

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

    /**
     * Renames or moves a file/directory to a new virtual path. Returns false on failure,
     * including attempts to move the virtual root or an entry into itself/its descendants.
     */
    bool rename(const std::string& oldVirtualPath, const std::string& newVirtualPath);

    /**
     * True if `name` is a single directory entry: non-empty, not `.` or `..`, and
     * contains no `/`. Used by the Filesystem Browser rename path.
     */
    static bool isValidEntryName(const std::string& name);

    /**
     * Rename `oldName` to `newName` inside `dirVirtualPath`.
     * Rejects invalid names (including those containing `/`) without creating nested paths.
     */
    bool renameEntry(const std::string& dirVirtualPath,
                     const std::string& oldName,
                     const std::string& newName);

    /** Writes (or overwrites) a file with the given content. */
    bool writeFile(const std::string& virtualPath, const std::string& content);

    /**
     * Writes content incrementally through the same safe atomic replacement path
     * as writeFile(). Returning false from the producer aborts and preserves the
     * previous file; the producer must check each stream write it performs.
     */
    bool writeFileWithProducer(const std::string& virtualPath,
                               const FileContentProducer& produceContent);

    /**
     * Checks the target stamp before staging and immediately before atomic replacement;
     * nullopt means the target is expected to be absent. A Conflict means the
     * target changed and no replacement occurred; the final check and rename are
     * not a cross-process compare-and-swap for existing targets. An absent target
     * is published with no-replace rename semantics.
     */
    ConditionalWriteResult writeFileWithProducerIfStampMatches(
        const std::string& virtualPath,
        const FileContentProducer& produceContent,
        const std::optional<FileStamp>& expectedStamp);

    /** Updates the last-write time of an existing regular file without changing its content. */
    bool updateModifiedTime(const std::string& virtualPath);

    /** Reads a regular file's identity, size, and high-resolution change times. */
    bool fileStamp(const std::string& virtualPath, FileStamp& outStamp) const;

    /** Reads the entire content of a file. Returns empty string on failure. */
    std::string readFile(const std::string& virtualPath) const;

    /** Reads a file while preserving the distinction between empty content and failure. */
    bool readFile(const std::string& virtualPath, std::string& outContent) const;

    /**
     * Reads a regular file in bounded 16 KiB chunks. The chunk view is valid only
     * during the callback. Return false from the consumer to stop early; an early
     * stop is successful, while path, open, read, or callback failures return false.
     */
    bool readFileChunks(const std::string& virtualPath,
                        const FileChunkConsumer& consumeChunk) const;

    /**
     * Reads at most the final maxBytes of a regular file in bounded 16 KiB chunks.
     * outPrefixSkipped reports whether earlier bytes were omitted; the first chunk
     * may begin inside a logical record. Return false from the consumer to stop early
     * successfully; path, open, seek, read, or callback failures return false.
     */
    bool readFileTailChunks(const std::string& virtualPath,
                            std::uint64_t maxBytes,
                            const FileChunkConsumer& consumeChunk,
                            bool& outPrefixSkipped) const;

    /**
     * Byte size of a regular file. Returns false if missing or not a regular file.
     */
    bool fileSize(const std::string& virtualPath, std::uint64_t& outBytes) const;

    /**
     * Copies a file or directory tree to a destination path.
     * Destination parent directories are created as needed.
     * A failed copy removes new entries and restores overwritten files and
     * destination-directory modification times.
     * Returns false if the resolved destination aliases the source or is inside its tree.
     */
    bool copyRecursive(const std::string& srcVirtualPath, const std::string& dstVirtualPath);

    /**
     * Copy each source path into `destDirVirtualPath` under its basename, via copyRecursive.
     * Skips missing sources, existing destinations, self-copy, and invalid names.
     * Returns the number of items successfully copied (0 if dest is not a directory).
     */
    int copyItemsInto(const std::vector<std::string>& srcVirtualPaths,
                      const std::string& destDirVirtualPath);

    /**
     * Moves each source path into `destDirVirtualPath` under its basename.
     * Uses non-overwriting renames, so conflicting sources remain untouched.
     * Returns the number of items successfully moved.
     */
    int moveItemsInto(const std::vector<std::string>& srcVirtualPaths,
                      const std::string& destDirVirtualPath);

    /** Last path component after normalize. Empty string for "/". */
    std::string baseName(const std::string& virtualPath) const;

    /**
     * Case-insensitive substring match of `query` against `name`.
     * An empty query matches every name.
     */
    static bool entryNameMatches(const std::string& name, const std::string& query);

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

    /**
     * Filter directory entries by name query (entryNameMatches). Empty query returns all.
     */
    static std::vector<DirEntry> filterEntries(const std::vector<DirEntry>& entries,
                                               const std::string& query);
    /** Matching source indices without copying entry names. */
    static std::vector<std::size_t> filterEntryIndices(
        const std::vector<DirEntry>& entries, const std::string& query);
    /** Narrow an ordered candidate index list in place for a more restrictive query. */
    static void filterEntryIndicesInPlace(
        const std::vector<DirEntry>& entries,
        const std::string& query,
        std::vector<std::size_t>& candidates);

    /** Lists entries with type info (directories first, then case-insensitive alpha-sorted files). */
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

    /**
     * Converts a virtual path to a real path on the host disk under hostRoot().
     * Useful for host APIs (e.g. SDL_LoadBMP) that need a filesystem path.
     * Returns an empty string when an existing symlink would resolve outside
     * the configured host root.
     */
    std::string toHostPath(const std::string& virtualPath) const;

private:
    struct CleanupTraversal;

    struct HostPath {
        std::string raw;
        std::string resolved;
    };

    bool isWithinHostRoot(const std::string& hostPath) const;
    bool copyRecursiveResolved(const std::string& dstVirtualPath,
                               const HostPath& source,
                               const HostPath& destination,
                               const std::string& canonicalRoot,
                               bool sourceIsDirectory);
    bool writeFileWithProducerAtHostPath(
        const std::string& hostPath,
        const FileContentProducer& produceContent,
        const std::optional<FileStamp>* expectedStamp = nullptr,
        bool* outVersionConflict = nullptr);

    std::string m_hostRoot;
    std::shared_ptr<CleanupTraversal> m_cleanupTraversal;
};

} // namespace monolith::fs
