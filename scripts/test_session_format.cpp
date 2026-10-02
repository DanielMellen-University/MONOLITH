#include "../src/window/SessionFormat.hpp"
#include "../src/detail/BoundedLineReader.hpp"

#include <iostream>
#include <sstream>
#include <string>

using monolith::window::session::readPath;
using monolith::window::session::writePath;

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

    const std::string original = "/home/monolith/my sketches/plan\\draft\"1.modr";
    std::ostringstream encoded;
    writePath(encoded, original);
    check(encoded.str() == "\"/home/monolith/my sketches/plan\\\\draft\\\"1.modr\"",
          "quoted path preserves special characters");

    std::string decoded;
    std::istringstream roundTrip(encoded.str());
    check(readPath(roundTrip, decoded) && decoded == original,
          "quoted path round-trips");

    std::istringstream legacy("/home/monolith/notes.txt");
    check(readPath(legacy, decoded) && decoded == "/home/monolith/notes.txt",
          "legacy unquoted path remains readable");

    std::ostringstream emptyEncoded;
    writePath(emptyEncoded, "");
    check(emptyEncoded.str() == "\"-\"", "empty path uses quoted sentinel");
    std::istringstream emptyInput(emptyEncoded.str());
    check(readPath(emptyInput, decoded) && decoded.empty(),
          "empty path sentinel decodes to empty");

    std::istringstream legacyEmpty("-");
    check(readPath(legacyEmpty, decoded) && decoded.empty(),
          "legacy empty path sentinel remains readable");

    const std::string maxLine(monolith::detail::kMaxPersistedLineBytes, 'x');
    std::istringstream boundaryInput(maxLine + "\nnext");
    check(monolith::detail::readBoundedLine(
              boundaryInput, decoded) == monolith::detail::BoundedLineResult::Line
              && decoded == maxLine,
          "session record at the byte limit is accepted");
    check(monolith::detail::readBoundedLine(
              boundaryInput, decoded) == monolith::detail::BoundedLineResult::Line
              && decoded == "next",
          "bounded reader continues after an exact-limit record");

    std::istringstream boundedInput(maxLine + "x");
    check(monolith::detail::readBoundedLine(
              boundedInput, decoded) == monolith::detail::BoundedLineResult::TooLong
              && decoded.empty(),
          "overlong session records are rejected without retaining their contents");

    if (failures == 0) {
        std::cout << "ALL SESSION FORMAT TESTS PASSED\n";
        return 0;
    }
    return 1;
}
