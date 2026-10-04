#include "TestTempDir.hpp"

#include <filesystem>
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        std::cout << (ok ? "ok: " : "FAIL: ") << message << '\n';
        if (!ok) ++failures;
    };

    std::filesystem::path firstPath;
    std::filesystem::path secondPath;
    {
        monolith::test::ScopedTempDirectory first("monolith-test-temp");
        check(static_cast<bool>(first) && std::filesystem::is_directory(first.path()),
              "create an isolated temporary directory");
        firstPath = first.path();
        {
            monolith::test::ScopedTempDirectory second("monolith-test-temp");
            check(static_cast<bool>(second) && second.path() != first.path(),
                  "parallel fixtures receive different directories");
            secondPath = second.path();
        }
        check(!std::filesystem::exists(secondPath)
                  && std::filesystem::is_directory(firstPath),
              "scope cleanup removes only its own directory");
    }
    check(!std::filesystem::exists(firstPath),
          "temporary directory is cleaned up at scope exit");

    if (failures == 0) {
        std::cout << "ALL TEMP DIRECTORY TESTS PASSED\n";
        return 0;
    }
    return 1;
}
