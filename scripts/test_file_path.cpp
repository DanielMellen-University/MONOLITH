#include "../src/app/FilePath.hpp"

#include <iostream>

using monolith::app::hasCaseInsensitiveSuffix;
using monolith::app::expandVirtualHomeShorthand;
using monolith::app::expandVirtualHomeCursorPosition;
using monolith::app::isWallpaperImagePath;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    check(hasCaseInsensitiveSuffix("/home/monolith/sketch.modr", ".modr"),
          "lowercase drawing suffix matches");
    check(hasCaseInsensitiveSuffix("/home/monolith/sketch.MODR", ".modr"),
          "uppercase drawing suffix matches");
    check(hasCaseInsensitiveSuffix("/home/monolith/sketch.MoDr", ".MODR"),
          "mixed-case suffix matches");
    check(!hasCaseInsensitiveSuffix("/home/monolith/sketch.mod", ".modr"),
          "nearby suffix does not match");
    check(!hasCaseInsensitiveSuffix(".mod", ".modr"),
          "short value does not match");

    check(expandVirtualHomeShorthand("~") == "/home/monolith",
          "home shorthand expands by itself");
    check(expandVirtualHomeShorthand("~/documents/note.txt")
              == "/home/monolith/documents/note.txt",
          "home-relative paths expand their leading shorthand");
    check(expandVirtualHomeShorthand("~notes/note.txt") == "~notes/note.txt",
          "tilde-prefixed names are not mistaken for home shorthand");
    check(expandVirtualHomeShorthand("documents/note.txt") == "documents/note.txt",
          "ordinary relative paths remain unchanged");
    check(expandVirtualHomeShorthand("/home/monolith/note.txt")
              == "/home/monolith/note.txt",
          "absolute paths remain unchanged");
    check(expandVirtualHomeCursorPosition("~/documents/note.txt", 0) == 0
              && expandVirtualHomeCursorPosition("~/documents/note.txt", 1) == 14
              && expandVirtualHomeCursorPosition("~/documents/note.txt", 2) == 15,
          "home-prefix expansion preserves caret positions around the shorthand");

    check(isWallpaperImagePath("/Wallpapers/sample.bmp"), "bmp wallpaper path");
    check(isWallpaperImagePath("/Wallpapers/sample.PNG"), "png wallpaper path case-insensitive");
    check(isWallpaperImagePath("/Wallpapers/photo.jpg"), "jpg wallpaper path");
    check(isWallpaperImagePath("/Wallpapers/photo.JPEG"), "jpeg wallpaper path");
    check(!isWallpaperImagePath("/Wallpapers/notes.txt"), "non-image is not wallpaper path");
    check(!isWallpaperImagePath("/Wallpapers/photo.jpeg.bak"), "suffixed non-image rejected");

    if (failures == 0) {
        std::cout << "ALL FILE PATH TESTS PASSED\n";
        return 0;
    }
    return 1;
}
