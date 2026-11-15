#include "core/TerminalCommand.h"
#include <cstring>
#include <utility>

namespace Git {

namespace {

bool isBlank(char c) {
    return c == ' ' || c == '\t';
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && isBlank(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isBlank(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

// A word the shell would use exactly as typed: no blanks, quotes, escapes, variables,
// redirections, globs or command separators.
bool isPlainWord(std::string_view word) {
    if (word.empty()) {
        return false;
    }
    for (const char c : word) {
        if (isBlank(c) || std::strchr(";&|<>()$`\\\"'*?[]{}!#", c) != nullptr) {
            return false;
        }
    }
    return true;
}

// `text` is one plain word, or one word inside a single pair of matching quotes with nothing
// inside that the shell would interpret. On success `word` holds it without the quotes.
bool singleWord(std::string_view text, std::string &word) {
    if (text.size() >= 2 && (text.front() == '\'' || text.front() == '"') && text.back() == text.front()) {
        const char quote = text.front();
        const std::string_view inner = text.substr(1, text.size() - 2);
        for (const char c : inner) {
            if (c == quote) {
                return false;
            }
            if (quote == '"' && (c == '$' || c == '`' || c == '\\' || c == '!')) {
                return false;
            }
        }
        word = std::string(inner);
        return !word.empty();
    }
    if (isPlainWord(text)) {
        word = std::string(text);
        return true;
    }
    return false;
}

} // namespace

TerminalCommand classifyTerminalCommand(std::string_view line) {
    const std::string_view text = trim(line);
    TerminalCommand result;

    if (text == "clear" || text == "cls") {
        result.kind = TerminalCommandKind::Clear;
        return result;
    }

    if (text == "cd") {
        result.kind = TerminalCommandKind::ChangeDirectory;   // no argument: the home folder
        return result;
    }

    if (text.size() > 3 && text.substr(0, 2) == "cd" && isBlank(text[2])) {
        std::string word;
        if (singleWord(trim(text.substr(3)), word)) {
            result.kind = TerminalCommandKind::ChangeDirectory;
            result.argument = std::move(word);
        }
    }
    return result;
}

} // namespace Git
