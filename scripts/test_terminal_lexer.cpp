// Headless test of Terminal command-line quoting (no SDL).
// Compiles against src/app/TerminalLexer.cpp.

#include "../src/app/TerminalLexer.hpp"

#include <iostream>
#include <string>
#include <vector>

using monolith::app::CommandTokens;
using monolith::app::tokenizeCommandLine;

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

    auto expectArgs = [&](const std::string& line,
                          const std::vector<std::string>& want,
                          const std::string& label) {
        CommandTokens t = tokenizeCommandLine(line);
        const std::string noErr = label + " no error";
        const std::string match = label + " args match";
        check(t.error.empty(), noErr.c_str());
        check(t.args == want, match.c_str());
        if (t.args != want) {
            std::cerr << "  got " << t.args.size() << " args:";
            for (const auto& a : t.args) {
                std::cerr << " [" << a << "]";
            }
            std::cerr << '\n';
        }
    };

    expectArgs("ls", {"ls"}, "bare command");
    expectArgs("  ls  -l  ", {"ls", "-l"}, "whitespace collapse");
    expectArgs("cat /home/monolith/readme.txt",
               {"cat", "/home/monolith/readme.txt"},
               "unquoted path");
    expectArgs("cat \"/home/monolith/my file.txt\"",
               {"cat", "/home/monolith/my file.txt"},
               "double-quoted path with spaces");
    expectArgs("cat '/home/monolith/my file.txt'",
               {"cat", "/home/monolith/my file.txt"},
               "single-quoted path with spaces");
    expectArgs("echo \"hello \\\"world\\\"\"",
               {"echo", "hello \"world\""},
               "escaped quotes inside double quotes");
    expectArgs("cp -r \"src dir\" \"dst dir\"",
               {"cp", "-r", "src dir", "dst dir"},
               "flags plus two quoted operands");
    expectArgs("echo one\\ two", {"echo", "one two"}, "backslash-escaped space");
    expectArgs("touch \"\"", {"touch", ""}, "empty double-quoted arg");

    CommandTokens bad = tokenizeCommandLine("cat \"unterminated");
    check(!bad.error.empty(), "unterminated double quote reports error");
    check(bad.args.empty(), "unterminated double quote clears args");

    CommandTokens bad2 = tokenizeCommandLine("echo 'oops");
    check(!bad2.error.empty(), "unterminated single quote reports error");

    if (failures == 0) {
        std::cout << "ALL TERMINAL LEXER TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
