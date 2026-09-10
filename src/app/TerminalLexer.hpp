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
 * The token currently being edited for Tab completion.
 * `replacementStart` points after an opening quote when one is active.
 * A cursor immediately after a closed quoted token has no active token.
 */
struct CompletionContext {
    std::size_t replacementStart = 0;
    std::string prefix;
    bool firstWord = true;
    bool hasToken = false;
    char quote = '\0';
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

/** Decode the current token prefix using the same quote rules as the lexer. */
CompletionContext completionContextAt(const std::string& line, std::size_t cursor);

/** Escape a completion for insertion into an unquoted or quoted token. */
std::string escapeCompletion(const std::string& value, char quote);

} // namespace monolith::app
