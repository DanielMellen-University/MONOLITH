// Headless test of shipped Filesystem multi-item copy/paste, rename, and listing filter.
// Compiles against src/fs/Filesystem.cpp (no SDL).

#include "../src/fs/Filesystem.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace stdfs = std::filesystem;
using monolith::fs::Filesystem;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << "FAIL: " << msg << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << msg << '\n';
        }
    };

    const stdfs::path hostRoot = stdfs::temp_directory_path()
        / ("monolith-fs-roadmap-" + std::to_string(getpid()));
    std::error_code ec;
    stdfs::remove_all(hostRoot, ec);
    stdfs::create_directories(hostRoot, ec);
    if (ec) {
        std::cerr << "FAIL: could not create temp host root\n";
        return 1;
    }

    Filesystem fs(hostRoot.string());
    check(fs.initialize(), "filesystem initialize");

    const stdfs::path outsideRoot = stdfs::temp_directory_path()
        / ("monolith-fs-outside-" + std::to_string(getpid()));
    stdfs::remove_all(outsideRoot, ec);
    stdfs::create_directories(outsideRoot, ec);
    check(!ec, "create outside symlink target");
    {
        std::ofstream outsideFile(outsideRoot / "secret.txt");
        outsideFile << "outside";
    }
    const stdfs::path escapeLink = hostRoot / "escape";
    stdfs::create_directory_symlink(outsideRoot, escapeLink, ec);
    check(!ec, "create outside symlink");
    if (!ec) {
        check(fs.toHostPath("/escape/secret.txt").empty(),
              "host path rejects symlink target outside root");
        check(!fs.exists("/escape") && !fs.isDirectory("/escape"),
              "outside symlink is not visible as a virtual entry");
        std::string escapedContent;
        check(!fs.readFile("/escape/secret.txt", escapedContent),
              "read rejects outside symlink target");
        check(!fs.writeFile("/escape/new.txt", "blocked"),
              "write rejects outside symlink target");
        const auto rootEntries = fs.list("/");
        check(std::find(rootEntries.begin(), rootEntries.end(), "escape")
                  == rootEntries.end(),
              "directory listing hides outside symlink");
        std::ifstream outsideCheck(outsideRoot / "secret.txt");
        std::string outsideContent;
        std::getline(outsideCheck, outsideContent);
        check(outsideContent == "outside",
              "outside file remains untouched");
        const auto typedRootEntries = fs.listEntries("/");
        check(std::find_if(typedRootEntries.begin(), typedRootEntries.end(), [](const auto& entry) {
                  return entry.name == "escape";
              }) == typedRootEntries.end(),
              "typed directory listing hides outside symlink");
    }

    check(fs.createDirectory("/symlink-target"), "create in-root symlink target");
    check(fs.writeFile("/symlink-target/keep.txt", "keep me"),
          "write in-root symlink target file");
    const stdfs::path internalLink = hostRoot / "internal-link";
    stdfs::create_directory_symlink(hostRoot / "symlink-target", internalLink, ec);
    check(!ec, "create in-root symlink");
    if (!ec) {
        check(!fs.copyRecursive("/internal-link", "/copied-link"),
              "copy rejects a symlink source instead of traversing it");
        check(fs.removeRecursive("/internal-link"),
              "recursive remove deletes the symlink itself");
        check(!fs.exists("/internal-link") && fs.isFile("/symlink-target/keep.txt")
                  && fs.readFile("/symlink-target/keep.txt") == "keep me",
              "recursive symlink removal preserves the target tree");
    }

    check(fs.writeFile("/rename-source.txt", "keep source"),
          "write rename source for dangling-link coverage");
    const stdfs::path danglingDestination = hostRoot / "dangling-destination";
    stdfs::create_symlink(hostRoot / "missing-target", danglingDestination, ec);
    check(!ec, "create dangling rename destination");
    if (!ec) {
        check(!fs.rename("/rename-source.txt", "/dangling-destination"),
              "rename rejects an existing dangling symlink destination");
        check(fs.isFile("/rename-source.txt")
                  && stdfs::is_symlink(stdfs::symlink_status(danglingDestination)),
              "dangling destination and rename source remain intact");
    }

    const stdfs::path fileRoot = stdfs::temp_directory_path()
        / ("monolith-fs-file-root-" + std::to_string(getpid()));
    stdfs::remove_all(fileRoot, ec);
    {
        std::ofstream blocker(fileRoot);
        blocker << "not a directory";
    }
    Filesystem invalidRoot(fileRoot.string());
    check(!invalidRoot.initialize(), "filesystem rejects a file as the host root");
    stdfs::remove(fileRoot, ec);

    check(fs.createDirectory("/src"), "create /src");
    check(fs.createDirectory("/dst"), "create /dst");
    check(fs.createDirectory("/src/folder"), "create /src/folder");
    check(fs.writeFile("/src/a.txt", "alpha"), "write /src/a.txt");
    check(fs.writeFile("/src/b.txt", "bravo"), "write /src/b.txt");
    check(fs.writeFile("/src/notes.txt", "memo"), "write /src/notes.txt");
    check(fs.writeFile("/src/folder/c.txt", "charlie"), "write nested file");
    check(fs.writeFile("/src/Alpha.txt", "upper"), "write /src/Alpha.txt");
    check(fs.writeFile("/src/alpha.txt", "lower"), "write /src/alpha.txt");
    check(fs.writeFile("/src/other.dat", "zzz"), "write /src/other.dat");
    check(fs.writeFile("/src/empty.txt", ""), "write empty file");
    check(fs.isFile("/src/empty.txt") && fs.readFile("/src/empty.txt").empty(),
          "read empty file without failure");
    std::string explicitRead;
    check(fs.readFile("/src/empty.txt", explicitRead) && explicitRead.empty(),
          "explicit read accepts empty file");
    check(fs.readFile("/src/a.txt", explicitRead) && explicitRead == "alpha",
          "explicit read returns file content");
    check(!fs.readFile("/src/missing.txt", explicitRead),
          "explicit read reports missing file");
    std::uint64_t emptySize = 99;
    check(fs.fileSize("/src/empty.txt", emptySize) && emptySize == 0,
          "empty file reports zero bytes");

    // Multi-item copy into a destination folder (paste).
    const int copied = fs.copyItemsInto(
        {"/src/a.txt", "/src/b.txt", "/src/folder"},
        "/dst");
    check(copied == 3, "copyItemsInto copied three sources");
    check(fs.isFile("/dst/a.txt") && fs.readFile("/dst/a.txt") == "alpha",
          "pasted a.txt content");
    check(fs.isFile("/dst/b.txt") && fs.readFile("/dst/b.txt") == "bravo",
          "pasted b.txt content");
    check(fs.isDirectory("/dst/folder"), "pasted folder");
    check(fs.isFile("/dst/folder/c.txt") && fs.readFile("/dst/folder/c.txt") == "charlie",
          "pasted nested file via copyRecursive");
    check(fs.copyRecursive("/src/empty.txt", "/dst/empty.txt"),
          "copy empty file");
    std::uint64_t copiedEmptySize = 99;
    check(fs.fileSize("/dst/empty.txt", copiedEmptySize) && copiedEmptySize == 0,
          "copied empty file remains zero bytes");

    check(fs.writeFile("/src/move.txt", "move me"), "write move source");
    check(fs.writeFile("/src/conflict.txt", "keep source"), "write conflicting source");
    check(fs.writeFile("/dst/conflict.txt", "keep destination"), "write conflicting destination");
    const int moved = fs.moveItemsInto({"/src/move.txt", "/src/conflict.txt"}, "/dst");
    check(moved == 1, "moveItemsInto moves only non-conflicting source");
    check(!fs.exists("/src/move.txt") && fs.readFile("/dst/move.txt") == "move me",
          "moved source leaves destination content");
    check(fs.readFile("/src/conflict.txt") == "keep source"
              && fs.readFile("/dst/conflict.txt") == "keep destination",
          "conflicting source and destination remain intact");

    // Same-folder / existing dest should not overwrite.
    const int copiedAgain = fs.copyItemsInto({"/src/a.txt"}, "/dst");
    check(copiedAgain == 0, "copyItemsInto skips existing dest name");
    check(fs.readFile("/dst/a.txt") == "alpha", "existing dest unchanged");

    // Rename rejects names containing '/'.
    check(!Filesystem::isValidEntryName("foo/bar"), "isValidEntryName rejects slash");
    check(!Filesystem::isValidEntryName(""), "isValidEntryName rejects empty");
    check(Filesystem::isValidEntryName("ok.txt"), "isValidEntryName accepts simple name");
    check(fs.renameEntry("/src", "a.txt", "renamed.txt"), "renameEntry valid name");
    check(fs.isFile("/src/renamed.txt"), "renamed file exists");
    check(!fs.renameEntry("/src", "renamed.txt", "foo/bar"), "renameEntry rejects slash name");
    check(fs.isFile("/src/renamed.txt"), "slash rename left original in place");
    check(!fs.exists("/src/foo"), "slash rename did not create nested path");
    check(!fs.exists("/src/foo/bar"), "slash rename did not write nested file");

    // A directory cannot be moved into its own subtree or replace the virtual root.
    check(!fs.rename("/src", "/src/folder/moved"),
          "rename rejects moving a directory into its descendant");
    check(fs.isDirectory("/src") && fs.isFile("/src/folder/c.txt")
              && !fs.exists("/src/folder/moved"),
          "descendant rename leaves the source tree intact");
    check(!fs.rename("/", "/reparented"), "rename rejects moving the virtual root");

    // Listing filter/search by name.
    auto listed = fs.listEntries("/src");
    auto folderIt = std::find_if(listed.begin(), listed.end(), [](const auto& entry) {
        return entry.name == "folder";
    });
    auto upperIt = std::find_if(listed.begin(), listed.end(), [](const auto& entry) {
        return entry.name == "Alpha.txt";
    });
    auto lowerIt = std::find_if(listed.begin(), listed.end(), [](const auto& entry) {
        return entry.name == "alpha.txt";
    });
    check(folderIt != listed.end() && upperIt != listed.end() && lowerIt != listed.end()
              && folderIt < upperIt && upperIt < lowerIt,
          "listEntries keeps directories first and sorts names case-insensitively");
    auto notes = Filesystem::filterEntries(listed, "note");
    check(notes.size() == 1 && notes.front().name == "notes.txt",
          "filterEntries matches notes.txt");
    auto none = Filesystem::filterEntries(listed, "no-such-name");
    check(none.empty(), "filterEntries empty on miss");
    auto all = Filesystem::filterEntries(listed, "");
    check(all.size() == listed.size(), "empty query returns all entries");
    check(Filesystem::entryNameMatches("Notes.TXT", "note"),
          "entryNameMatches is case-insensitive");
    check(!Filesystem::entryNameMatches("other.dat", "note"),
          "entryNameMatches rejects non-matching name");

    stdfs::remove_all(hostRoot, ec);
    stdfs::remove_all(outsideRoot, ec);

    if (failures == 0) {
        std::cout << "ALL FS ROADMAP TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
