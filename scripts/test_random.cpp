// Headless test of the per-app random helper used by the built-in games.

#include "../src/detail/Random.hpp"

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

    monolith::detail::Random first(12345u);
    monolith::detail::Random second(12345u);
    bool sameSeedMatches = true;
    for (int i = 0; i < 32; ++i) {
        sameSeedMatches = sameSeedMatches
            && first.uniformInt(20) == second.uniformInt(20);
    }
    check(sameSeedMatches, "seeded generators produce repeatable sequences");

    monolith::detail::Random values(67890u);
    bool inRange = true;
    for (int i = 0; i < 500; ++i) {
        const int value = values.uniformInt(7);
        inRange = inRange && value >= 0 && value < 7;
    }
    check(inRange, "uniformInt stays inside its upper bound");
    check(values.uniformInt(0) == 0 && values.uniformInt(-1) == 0,
          "non-positive bounds return the safe lower value");

    if (failures == 0) {
        std::cout << "ALL RANDOM TESTS PASSED\n";
        return 0;
    }
    return 1;
}
