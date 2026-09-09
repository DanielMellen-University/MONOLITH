#pragma once

#include <string>
#include <vector>

namespace monolith::app {

/** Result of splitting a terminal command line into argv-style tokens. */
struct CommandTokens {
    std::vector<std::string> args;
    /** Non-empty when the line could not be tokenized (e.g. unterminated quote). */
    std::string error;
};

/**
 * Split a command line with shell-style quoting.
 *
 * - Unquoted whitespace separates tokens.
 * - Double quotes keep spaces; \\ and \" are escapes inside them.
 * - Single quotes keep everything literal until the closing quote.
 * - Outside quotes, a backslash escapes the next character (including space).
 */
CommandTokens tokenizeCommandLine(const std::string& line);

} // namespace monolith::app
