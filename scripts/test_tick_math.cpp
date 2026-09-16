// Headless coverage for the shared SDL tick timing helpers.

#include "../src/detail/TickMath.hpp"

#include <cmath>
#include <iostream>

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

    check(std::fabs(monolith::detail::tickDeltaSeconds(1025u, 1000u) - 0.025f) < 0.0001f,
          "tick delta converts milliseconds to seconds");
    check(std::fabs(monolith::detail::tickDeltaSeconds(0x00000020u, 0xfffffff0u) - 0.048f) < 0.0001f,
          "tick delta survives the SDL counter wrap");
    check(monolith::detail::tickDeltaSeconds(1000u, 0u) == 0.05f,
          "tick delta clamps long stalls");
    check(monolith::detail::tickDeltaSeconds(1000u, 1000u) == 0.f,
          "tick delta accepts an unchanged counter");

    if (failures == 0) {
        std::cout << "ALL TICK MATH TESTS PASSED\n";
        return 0;
    }
    return 1;
}
