#include <gtest/gtest.h>
#include "core/CommitSearch.h"

using namespace Git;

namespace {

CommitNode makeNode(const char *oid, const char *message, const char *author, const char *email) {
    CommitNode node;
    node.oid = oid;
    node.shortHash = node.oid.substr(0, 7);
    node.message = message;
    node.author = author;
    node.email = email;
    return node;
}

// Row 0: fix crash in parser | Alice | alice@example.com | branch main
// Row 1: Add search bar      | Bob   | bob@example.org     | tag v1.0, remote origin/main
// Row 2: Fix typo            | alice | a.smith@example.com |
std::vector<CommitNode> sampleHistory() {
    std::vector<CommitNode> nodes;
    nodes.push_back(makeNode("abcdef0123456789abcdef0123456789abcdef01", "fix crash in parser", "Alice", "alice@example.com"));
    nodes.push_back(makeNode("1234567890abcdef1234567890abcdef12345678", "Add search bar", "Bob", "bob@example.org"));
    nodes.push_back(makeNode("deadbeef00000000000000000000000000000000", "Fix typo", "alice", "a.smith@example.com"));
    nodes[0].branchNames = {"main"};
    nodes[1].tagNames = {"v1.0"};
    nodes[1].remoteBranchNames = {"origin/main"};
    return nodes;
}

} // namespace

TEST(CommitSearchParse, EmptyAndBlankTextGiveNoTerms) {
    EXPECT_TRUE(parseSearchQuery("").empty());
    EXPECT_TRUE(parseSearchQuery("   \t ").empty());
}

TEST(CommitSearchParse, SplitsWordsAndLowerCasesThem) {
    const SearchQuery query = parseSearchQuery("  Fix   CRASH ");
    ASSERT_EQ(query.terms.size(), 2u);
    EXPECT_EQ(query.terms[0].text, std::string("fix"));
    EXPECT_EQ(query.terms[1].text, std::string("crash"));
    EXPECT_TRUE(query.terms[0].field == SearchField::Any);
}

TEST(CommitSearchParse, RecognizesFieldPrefixes) {
    const SearchQuery query = parseSearchQuery("author:Bob msg:typo message:x hash:ab ref:main");
    ASSERT_EQ(query.terms.size(), 5u);
    EXPECT_TRUE(query.terms[0].field == SearchField::Author);
    EXPECT_EQ(query.terms[0].text, std::string("bob"));
    EXPECT_TRUE(query.terms[1].field == SearchField::Message);
    EXPECT_TRUE(query.terms[2].field == SearchField::Message);
    EXPECT_TRUE(query.terms[3].field == SearchField::Hash);
    EXPECT_TRUE(query.terms[4].field == SearchField::Ref);
}

TEST(CommitSearchParse, PrefixWithNothingAfterItIsSkipped) {
    const SearchQuery query = parseSearchQuery("author: fix");
    ASSERT_EQ(query.terms.size(), 1u);
    EXPECT_EQ(query.terms[0].text, std::string("fix"));
}

TEST(CommitSearchParse, UnknownPrefixIsPlainText) {
    const SearchQuery query = parseSearchQuery("http://x");
    ASSERT_EQ(query.terms.size(), 1u);
    EXPECT_TRUE(query.terms[0].field == SearchField::Any);
    EXPECT_EQ(query.terms[0].text, std::string("http://x"));
}

TEST(CommitSearch, EmptyQueryFindsNothing) {
    EXPECT_EQ(findMatches(sampleHistory(), parseSearchQuery("")).size(), 0u);
}

TEST(CommitSearch, PlainWordMatchesMessageIgnoringCase) {
    const std::vector<int> rows = findMatches(sampleHistory(), parseSearchQuery("FIX"));
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0], 0);
    EXPECT_EQ(rows[1], 2);
}

TEST(CommitSearch, AllWordsMustMatch) {
    const std::vector<int> rows = findMatches(sampleHistory(), parseSearchQuery("fix typo"));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], 2);
}

TEST(CommitSearch, WordsMayMatchDifferentFields) {
    // "bob" is the author of row 1, "search" is in its subject
    const std::vector<int> rows = findMatches(sampleHistory(), parseSearchQuery("bob search"));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], 1);
}

TEST(CommitSearch, AuthorFieldChecksNameAndEmail) {
    const std::vector<int> byName = findMatches(sampleHistory(), parseSearchQuery("author:alice"));
    ASSERT_EQ(byName.size(), 2u);                       // rows 0 and 2 (name "alice"), row 1 has no "alice"
    const std::vector<int> byEmail = findMatches(sampleHistory(), parseSearchQuery("author:smith"));
    ASSERT_EQ(byEmail.size(), 1u);
    EXPECT_EQ(byEmail[0], 2);
}

TEST(CommitSearch, MessageFieldIgnoresAuthors) {
    // "alice" is only an author, never in a subject
    EXPECT_EQ(findMatches(sampleHistory(), parseSearchQuery("msg:alice")).size(), 0u);
}

TEST(CommitSearch, RefFieldMatchesBranchesRemotesAndTags) {
    EXPECT_EQ(findMatches(sampleHistory(), parseSearchQuery("ref:main")).size(), 2u);   // main, origin/main
    const std::vector<int> tag = findMatches(sampleHistory(), parseSearchQuery("ref:v1"));
    ASSERT_EQ(tag.size(), 1u);
    EXPECT_EQ(tag[0], 1);
}

TEST(CommitSearch, HashFieldMatchesTheStartOfTheId) {
    const std::vector<int> rows = findMatches(sampleHistory(), parseSearchQuery("hash:dead"));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], 2);
    EXPECT_EQ(findMatches(sampleHistory(), parseSearchQuery("hash:cdef")).size(), 0u);   // inside the id, not at its start
}

TEST(CommitSearch, PlainHexWordMatchesIdOnlyFromFourCharacters) {
    // "abc" is too short to count as an id, and no subject, author, e-mail or ref contains it
    EXPECT_EQ(findMatches(sampleHistory(), parseSearchQuery("abc")).size(), 0u);
    const std::vector<int> rows = findMatches(sampleHistory(), parseSearchQuery("abcd"));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], 0);
}

TEST(CommitSearch, NonAsciiTextMatchesExactly) {
    std::vector<CommitNode> nodes;
    nodes.push_back(makeNode("1111111111111111111111111111111111111111", "Fix caf\xC3\xA9 menu", "Jos\xC3\xA9", "j@x.org"));
    EXPECT_EQ(findMatches(nodes, parseSearchQuery("caf\xC3\xA9")).size(), 1u);
    EXPECT_EQ(findMatches(nodes, parseSearchQuery("CAF\xC3\xA9")).size(), 1u);   // ASCII letters still fold
}

TEST(CommitSearchStep, ForwardAndBackwardWrapAround) {
    const std::vector<int> rows = {2, 5, 9};
    EXPECT_EQ(stepMatch(rows, 2, true), 5);
    EXPECT_EQ(stepMatch(rows, 9, true), 2);      // wraps to the first
    EXPECT_EQ(stepMatch(rows, 5, false), 2);
    EXPECT_EQ(stepMatch(rows, 2, false), 9);     // wraps to the last
}

TEST(CommitSearchStep, FromARowThatIsNotAMatch) {
    const std::vector<int> rows = {2, 5, 9};
    EXPECT_EQ(stepMatch(rows, 6, true), 9);
    EXPECT_EQ(stepMatch(rows, 6, false), 5);
}

TEST(CommitSearchStep, NoCurrentRowStartsAtTheEnds) {
    const std::vector<int> rows = {2, 5, 9};
    EXPECT_EQ(stepMatch(rows, -1, true), 2);
    EXPECT_EQ(stepMatch(rows, -1, false), 9);
}

TEST(CommitSearchStep, EmptyListGivesMinusOne) {
    EXPECT_EQ(stepMatch({}, 3, true), -1);
    EXPECT_EQ(stepMatch({}, 3, false), -1);
}

TEST(CommitSearchPosition, IsOneBasedAndZeroForNonMatches) {
    const std::vector<int> rows = {2, 5, 9};
    EXPECT_EQ(matchPosition(rows, 2), 1);
    EXPECT_EQ(matchPosition(rows, 9), 3);
    EXPECT_EQ(matchPosition(rows, 4), 0);
    EXPECT_EQ(matchPosition(rows, -1), 0);
}
