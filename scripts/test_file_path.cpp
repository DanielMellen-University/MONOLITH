#include "../src/app/FilePath.hpp"

#include <iostream>

using monolith::app::hasCaseInsensitiveSuffix;

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

    if (failures == 0) {
        std::cout << "ALL FILE PATH TESTS PASSED\n";
        return 0;
    }
    return 1;
}
