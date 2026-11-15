#include "core/CommitSearch.h"
#include <algorithm>

namespace Git {

namespace {

char lowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// `needle` is already lower-case. Compares without making a lower-case copy of `haystack`,
// so searching 100,000 commits allocates nothing.
bool containsFold(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    if (haystack.size() < needle.size()) {
        return false;
    }
    const std::size_t lastStart = haystack.size() - needle.size();
    for (std::size_t i = 0; i <= lastStart; ++i) {
        std::size_t k = 0;
        while (k < needle.size() && lowerAscii(haystack[i + k]) == needle[k]) {
            ++k;
        }
        if (k == needle.size()) {
            return true;
        }
    }
    return false;
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool isHex(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

bool anyContains(const std::vector<std::string> &names, std::string_view needle) {
    return std::any_of(names.begin(), names.end(),
                       [needle](const std::string &name) { return containsFold(name, needle); });
}

bool refMatches(const CommitNode &node, std::string_view needle) {
    return anyContains(node.branchNames, needle) || anyContains(node.remoteBranchNames, needle)
        || anyContains(node.tagNames, needle);
}

bool termMatches(const CommitNode &node, const SearchTerm &term) {
    const std::string_view text = term.text;
    switch (term.field) {
    case SearchField::Message:
        return containsFold(node.message, text);
    case SearchField::Author:
        return containsFold(node.author, text) || containsFold(node.email, text);
    case SearchField::Hash:
        return startsWith(node.oid, text);
    case SearchField::Ref:
        return refMatches(node, text);
    case SearchField::Any:
        return containsFold(node.message, text) || containsFold(node.author, text)
            || containsFold(node.email, text) || refMatches(node, text)
            || (text.size() >= 4 && isHex(text) && startsWith(node.oid, text));
    }
    return false;
}

struct FieldPrefix {
    std::string_view prefix;
    SearchField field;
};

constexpr FieldPrefix kPrefixes[] = {
    {"author:", SearchField::Author},
    {"message:", SearchField::Message},
    {"msg:", SearchField::Message},
    {"hash:", SearchField::Hash},
    {"ref:", SearchField::Ref},
};

} // namespace

SearchQuery parseSearchQuery(std::string_view text) {
    SearchQuery query;
    std::size_t position = 0;

    while (position < text.size()) {
        // skip spaces, then take one word
        while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) {
            ++position;
        }
        const std::size_t start = position;
        while (position < text.size() && text[position] != ' ' && text[position] != '\t') {
            ++position;
        }
        if (start == position) {
            break;
        }

        std::string word(text.substr(start, position - start));
        for (char &c : word) {
            c = lowerAscii(c);
        }

        SearchTerm term;
        term.field = SearchField::Any;
        term.text = word;
        for (const FieldPrefix &entry : kPrefixes) {
            if (startsWith(word, entry.prefix)) {
                term.field = entry.field;
                term.text = word.substr(entry.prefix.size());
                break;
            }
        }
        if (term.text.empty()) {
            continue;   // "author:" and nothing after it yet
        }
        query.terms.push_back(std::move(term));
    }
    return query;
}

bool commitMatches(const CommitNode &node, const SearchQuery &query) {
    return std::all_of(query.terms.begin(), query.terms.end(),
                       [&node](const SearchTerm &term) { return termMatches(node, term); });
}

std::vector<int> findMatches(const std::vector<CommitNode> &nodes, const SearchQuery &query) {
    std::vector<int> rows;
    if (query.empty()) {
        return rows;
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (commitMatches(nodes[i], query)) {
            rows.push_back(static_cast<int>(i));
        }
    }
    return rows;
}

int stepMatch(const std::vector<int> &rows, int fromRow, bool forward) {
    if (rows.empty()) {
        return -1;
    }
    if (forward) {
        const auto it = std::upper_bound(rows.begin(), rows.end(), fromRow);   // first row > fromRow
        return it != rows.end() ? *it : rows.front();
    }
    const auto it = std::lower_bound(rows.begin(), rows.end(), fromRow);       // first row >= fromRow
    return it != rows.begin() ? *(it - 1) : rows.back();
}

int matchPosition(const std::vector<int> &rows, int row) {
    const auto it = std::lower_bound(rows.begin(), rows.end(), row);
    if (it != rows.end() && *it == row) {
        return static_cast<int>(it - rows.begin()) + 1;
    }
    return 0;
}

} // namespace Git
