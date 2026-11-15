#ifndef COMMITSEARCH_H
#define COMMITSEARCH_H

#include <string>
#include <string_view>
#include <vector>
#include "core/GitTypes.h"

// Searching the commits that are loaded in the graph. Pure functions (no Qt widgets, no
// repository), so they can be unit-tested and timed on their own.
//
// A query is a list of words. A commit matches when EVERY word matches (AND). A word is plain
// text, or "field:text" with one of these fields:
//     author:  the author's name or e-mail
//     msg:     the commit subject (also written message:)
//     hash:    the start of the commit id (any length)
//     ref:     a branch, remote branch or tag name
// A plain word matches the subject, author name, e-mail, any ref name, or the start of the
// commit id (the id only when the word is at least 4 hexadecimal characters, so that "a" does
// not match every id). Matching ignores upper/lower case for ASCII letters only.

namespace Git {

enum class SearchField : unsigned char { Any, Message, Author, Hash, Ref };

struct SearchTerm {
    SearchField field{SearchField::Any};
    std::string text;   // already lower-cased (ASCII)
};

struct SearchQuery {
    std::vector<SearchTerm> terms;
    [[nodiscard]] bool empty() const { return terms.empty(); }
};

/// Splits `text` at spaces into words. A recognized "field:" prefix with nothing after it
/// (the person is still typing) is skipped; any other word, even one containing ':', is plain text.
[[nodiscard]] SearchQuery parseSearchQuery(std::string_view text);

/// True if the commit matches every term of the query. An empty query matches everything.
[[nodiscard]] bool commitMatches(const CommitNode &node, const SearchQuery &query);

/// Row indexes (ascending) of all matching commits. An empty query matches no row here,
/// because "no search" must not highlight the whole history.
[[nodiscard]] std::vector<int> findMatches(const std::vector<CommitNode> &nodes, const SearchQuery &query);

/// The match after `fromRow` (or before it when `forward` is false), wrapping around at the ends.
/// `fromRow` need not be a match; -1 means "no current row". Returns -1 only if `rows` is empty.
[[nodiscard]] int stepMatch(const std::vector<int> &rows, int fromRow, bool forward);

/// 1-based position of `row` among the matches, or 0 if `row` is not a match.
[[nodiscard]] int matchPosition(const std::vector<int> &rows, int row);

} // namespace Git

#endif // COMMITSEARCH_H
