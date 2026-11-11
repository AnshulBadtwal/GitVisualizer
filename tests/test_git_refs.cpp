#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "core/GitRefs.h"

namespace Git {

class GitRefsTest : public ::testing::Test {
protected:
    static CommitNode node(const std::string& oid) {
        CommitNode n;
        n.oid = oid;
        return n;
    }

    using Names = std::vector<std::string>;
};

// --- parseRefName -----------------------------------------------------------

TEST_F(GitRefsTest, ParsesLocalBranchesRemoteBranchesAndTags) {
    const ParsedRef local = parseRefName("refs/heads/feature/login");
    EXPECT_EQ(local.kind, RefKind::LocalBranch);
    EXPECT_EQ(local.shortName, "feature/login");

    const ParsedRef remote = parseRefName("refs/remotes/origin/main");
    EXPECT_EQ(remote.kind, RefKind::RemoteBranch);
    EXPECT_EQ(remote.shortName, "origin/main");

    const ParsedRef tag = parseRefName("refs/tags/v1.0");
    EXPECT_EQ(tag.kind, RefKind::Tag);
    EXPECT_EQ(tag.shortName, "v1.0");
}

TEST_F(GitRefsTest, IgnoresRefsThatAreNeverShown) {
    EXPECT_EQ(parseRefName("refs/stash").kind, RefKind::Other);
    EXPECT_EQ(parseRefName("refs/notes/commits").kind, RefKind::Other);
    EXPECT_EQ(parseRefName("refs/remotes/origin/HEAD").kind, RefKind::Other);
    EXPECT_EQ(parseRefName("refs/remotes/HEAD").kind, RefKind::Other);
    EXPECT_EQ(parseRefName("HEAD").kind, RefKind::Other);
    EXPECT_EQ(parseRefName("").kind, RefKind::Other);
    EXPECT_EQ(parseRefName("refs/heads/").kind, RefKind::Other);
    EXPECT_EQ(parseRefName("refs/tags/").kind, RefKind::Other);
}

// --- attachRefs -------------------------------------------------------------

TEST_F(GitRefsTest, PutsEachNameOnItsOwnCommit) {
    std::vector<CommitNode> nodes = { node("a"), node("b"), node("c") };
    const std::vector<RefEntry> refs = {
        {"refs/heads/main", "a"},
        {"refs/heads/dev", "b"},
        {"refs/remotes/origin/main", "a"},
        {"refs/tags/v1", "c"},
        {"refs/stash", "b"},
    };

    attachRefs(nodes, refs, HeadInfo{"a", "main", false});

    EXPECT_EQ(nodes[0].branchNames, (Names{"main"}));
    EXPECT_EQ(nodes[0].remoteBranchNames, (Names{"origin/main"}));
    EXPECT_EQ(nodes[1].branchNames, (Names{"dev"}));
    EXPECT_TRUE(nodes[1].remoteBranchNames.empty());
    EXPECT_EQ(nodes[2].tagNames, (Names{"v1"}));
}

TEST_F(GitRefsTest, MarksTheHeadCommitAndItsBranch) {
    std::vector<CommitNode> nodes = { node("a"), node("b") };
    attachRefs(nodes, {{"refs/heads/main", "a"}}, HeadInfo{"a", "main", false});

    EXPECT_TRUE(nodes[0].isHead);
    EXPECT_EQ(nodes[0].checkedOutBranch, "main");
    EXPECT_TRUE(!nodes[1].isHead);
    EXPECT_TRUE(nodes[1].checkedOutBranch.empty());
}

TEST_F(GitRefsTest, DetachedHeadIsMarkedWithoutABranch) {
    std::vector<CommitNode> nodes = { node("a"), node("b") };
    attachRefs(nodes, {{"refs/heads/main", "a"}}, HeadInfo{"b", "", true});

    EXPECT_TRUE(nodes[1].isHead);
    EXPECT_TRUE(nodes[1].checkedOutBranch.empty());
    EXPECT_TRUE(!nodes[0].isHead);
    EXPECT_EQ(nodes[0].branchNames, (Names{"main"}));
}

TEST_F(GitRefsTest, RepositoryWithoutCommitsHasNoHead) {
    std::vector<CommitNode> nodes = { node("a") };
    attachRefs(nodes, {}, HeadInfo{});
    EXPECT_TRUE(!nodes[0].isHead);
}

TEST_F(GitRefsTest, CheckedOutBranchComesFirstThenAlphabetical) {
    std::vector<CommitNode> nodes = { node("a") };
    const std::vector<RefEntry> refs = {
        {"refs/heads/zeta", "a"}, {"refs/heads/main", "a"}, {"refs/heads/alpha", "a"},
        {"refs/tags/v2", "a"}, {"refs/tags/v1", "a"},
    };
    attachRefs(nodes, refs, HeadInfo{"a", "main", false});

    EXPECT_EQ(nodes[0].branchNames, (Names{"main", "alpha", "zeta"}));
    EXPECT_EQ(nodes[0].tagNames, (Names{"v1", "v2"}));
}

TEST_F(GitRefsTest, RefsToCommitsOutsideTheListAreIgnored) {
    std::vector<CommitNode> nodes = { node("a") };
    attachRefs(nodes, {{"refs/heads/old", "not-loaded"}}, HeadInfo{"not-loaded", "old", false});

    EXPECT_TRUE(nodes[0].branchNames.empty());
    EXPECT_TRUE(!nodes[0].isHead);
}

TEST_F(GitRefsTest, RunningTwiceReplacesInsteadOfDuplicating) {
    std::vector<CommitNode> nodes = { node("a"), node("b") };
    attachRefs(nodes, {{"refs/heads/main", "a"}, {"refs/tags/v1", "a"}}, HeadInfo{"a", "main", false});
    attachRefs(nodes, {{"refs/heads/main", "b"}}, HeadInfo{"b", "main", false});

    EXPECT_TRUE(nodes[0].branchNames.empty());
    EXPECT_TRUE(nodes[0].tagNames.empty());
    EXPECT_TRUE(!nodes[0].isHead);
    EXPECT_EQ(nodes[1].branchNames, (Names{"main"}));
    EXPECT_TRUE(nodes[1].isHead);
}

TEST_F(GitRefsTest, EmptyNodeListIsANoOp) {
    std::vector<CommitNode> nodes;
    attachRefs(nodes, {{"refs/heads/main", "a"}}, HeadInfo{"a", "main", false});
    EXPECT_TRUE(nodes.empty());
}

} // namespace Git
