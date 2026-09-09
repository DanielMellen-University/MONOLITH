#include "../src/app/Utf8.hpp"

#include <iostream>
#include <string>

using monolith::app::popLastUtf8Codepoint;
using monolith::app::utf8CodepointByteLen;
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

    if (failures == 0) {
        std::cout << "ALL UTF8 TESTS PASSED\n";
        return 0;
    }
    return 1;
}
