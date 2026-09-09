#include "../src/app/Utf8.hpp"

#include <iostream>
#include <string>

using monolith::app::popLastUtf8Codepoint;
using monolith::app::erasePreviousUtf8Codepoint;
using monolith::app::utf8CodepointByteLen;
using monolith::app::utf8NextCodepointStart;
using monolith::app::utf8PrevCodepointStart;

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

    const std::string value = "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    check(utf8CodepointByteLen(value, 0) == 1, "ASCII codepoint length");
    check(utf8CodepointByteLen(value, 1) == 2, "two-byte codepoint length");
    check(utf8CodepointByteLen(value, 3) == 3, "three-byte codepoint length");
    check(utf8CodepointByteLen(value, 6) == 4, "four-byte codepoint length");
    check(utf8PrevCodepointStart(value, value.size()) == 6,
          "previous codepoint finds four-byte start");

    std::string edited = value;
    popLastUtf8Codepoint(edited);
    check(edited == "A\xC3\xA9\xE2\x82\xAC", "backspace removes one codepoint");
    popLastUtf8Codepoint(edited);
    check(edited == "A\xC3\xA9", "repeated backspace stays codepoint-safe");
    popLastUtf8Codepoint(edited);
    popLastUtf8Codepoint(edited);
    check(edited == "", "backspace removes remaining codepoints");

    const std::string mixed = "A\xC3\xA9\xF0\x9F\x98\x80" "B";
    check(utf8NextCodepointStart(mixed, 0) == 1, "next codepoint skips ASCII");
    check(utf8NextCodepointStart(mixed, 1) == 3, "next codepoint skips two-byte character");
    check(utf8NextCodepointStart(mixed, 3) == 7, "next codepoint skips four-byte character");
    check(utf8PrevCodepointStart(mixed, 7) == 3, "previous codepoint returns to four-byte start");

    std::string cursorEdited = mixed;
    std::size_t cursor = 7;
    erasePreviousUtf8Codepoint(cursorEdited, cursor);
    check(cursor == 3 && cursorEdited == "A\xC3\xA9" "B",
          "cursor erase removes one complete codepoint");

    if (failures == 0) {
        std::cout << "ALL UTF8 TESTS PASSED\n";
        return 0;
    }
    return 1;
}
