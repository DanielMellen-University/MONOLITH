// Headless test of shipped Filesystem multi-item copy/paste, rename, and listing filter.
// Compiles against src/fs/Filesystem.cpp (no SDL).

#include "../src/fs/Filesystem.hpp"

#include <cstdint>
#include <filesystem>
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

    check(fs.createDirectory("/src"), "create /src");
    check(fs.createDirectory("/dst"), "create /dst");
    check(fs.createDirectory("/src/folder"), "create /src/folder");
    check(fs.writeFile("/src/a.txt", "alpha"), "write /src/a.txt");
    check(fs.writeFile("/src/b.txt", "bravo"), "write /src/b.txt");
    check(fs.writeFile("/src/notes.txt", "memo"), "write /src/notes.txt");
    check(fs.writeFile("/src/folder/c.txt", "charlie"), "write nested file");
    check(fs.writeFile("/src/other.dat", "zzz"), "write /src/other.dat");

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

    // Listing filter/search by name.
    auto listed = fs.listEntries("/src");
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

    if (failures == 0) {
        std::cout << "ALL FS ROADMAP TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
