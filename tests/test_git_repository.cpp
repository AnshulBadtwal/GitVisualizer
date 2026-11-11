// Integration tests: build small real repositories with the `git` command and read them back
// through GitRepository. They need `git` on the PATH and skip themselves if it is missing.

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>
#include "TempRepoFixture.h"
#include "core/GitRepository.h"

namespace Git {

class GitRepositoryTest : public TempRepoFixture {
protected:
    std::vector<CommitNode> load(std::size_t limit = 0) {
        GitRepository repo;
        EXPECT_TRUE(repo.open(m_dir.string())) << repo.lastError();
        std::vector<CommitNode> nodes;
        EXPECT_TRUE(repo.fetchCommitHistory(nodes, limit)) << repo.lastError();
        return nodes;
    }

    static const CommitNode* find(const std::vector<CommitNode>& nodes, const std::string& message) {
        for (const auto& node : nodes) {
            if (node.message == message) return &node;
        }
        return nullptr;
    }
};

TEST_F(GitRepositoryTest, OpeningAFolderThatIsNotARepositoryFails) {
    GitRepository repo;
    EXPECT_TRUE(!repo.open(m_dir.string()));
    EXPECT_TRUE(!repo.isOpen());
    EXPECT_TRUE(!repo.lastError().empty());
}

TEST_F(GitRepositoryTest, FetchingWithoutAnOpenRepositoryReportsAnError) {
    GitRepository repo;
    std::vector<CommitNode> nodes;
    EXPECT_TRUE(!repo.fetchCommitHistory(nodes));
    EXPECT_TRUE(!repo.lastError().empty());
}

TEST_F(GitRepositoryTest, RepositoryWithoutCommitsIsEmptyNotAnError) {
    initRepo();
    EXPECT_TRUE(load().empty());

    GitRepository repo;
    ASSERT_TRUE(repo.open(m_dir.string()));
    EXPECT_TRUE(repo.headInfo().oid.empty());
}

TEST_F(GitRepositoryTest, ReadsCommitFields) {
    initRepo();
    commit("first");
    commit("second");

    const auto nodes = load();
    ASSERT_EQ(nodes.size(), 2u);

    EXPECT_EQ(nodes[0].message, "second");
    EXPECT_EQ(nodes[0].author, "Tester");
    EXPECT_EQ(nodes[0].email, "tester@example.com");
    EXPECT_EQ(nodes[0].oid.size(), 40u);
    EXPECT_EQ(nodes[0].shortHash, nodes[0].oid.substr(0, 7));
    EXPECT_TRUE(nodes[0].timestamp > 0);
    ASSERT_EQ(nodes[0].parentOids.size(), 1u);
    EXPECT_EQ(nodes[0].parentOids[0], nodes[1].oid);
    EXPECT_TRUE(nodes[1].parentOids.empty());
}

TEST_F(GitRepositoryTest, LocalBranchesAndTheCheckedOutBranch) {
    initRepo();
    commit("first");
    commit("second");
    ASSERT_TRUE(git("branch feature HEAD~1"));

    const auto nodes = load();
    ASSERT_EQ(nodes.size(), 2u);

    EXPECT_EQ(nodes[0].branchNames, (Names{"main"}));
    EXPECT_TRUE(nodes[0].isHead);
    EXPECT_EQ(nodes[0].checkedOutBranch, "main");

    EXPECT_EQ(nodes[1].branchNames, (Names{"feature"}));
    EXPECT_TRUE(!nodes[1].isHead);
}

TEST_F(GitRepositoryTest, AnnotatedTagsGetABadgeOnTheirCommit) {
    initRepo();
    commit("first");
    ASSERT_TRUE(git("tag light"));
    ASSERT_TRUE(git("tag -a annotated -m note"));

    const auto nodes = load();
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].tagNames, (Names{"annotated", "light"}));
}

TEST_F(GitRepositoryTest, RemoteBranchesAreKeptApartFromLocalOnes) {
    initRepo();
    commit("first");
    ASSERT_TRUE(git("update-ref refs/remotes/origin/main HEAD"));
    ASSERT_TRUE(git("symbolic-ref refs/remotes/origin/HEAD refs/remotes/origin/main"));

    const auto nodes = load();
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].branchNames, (Names{"main"}));
    EXPECT_EQ(nodes[0].remoteBranchNames, (Names{"origin/main"}));   // no "origin/HEAD"
}

TEST_F(GitRepositoryTest, DetachedHeadIsFoundEvenWithoutABranch) {
    initRepo();
    commit("a");
    commit("b");
    ASSERT_TRUE(git("checkout -q --detach HEAD~1"));
    commit("c");   // exists only through HEAD: no branch points at it

    const auto nodes = load();
    ASSERT_EQ(nodes.size(), 3u);

    const CommitNode* c = find(nodes, "c");
    ASSERT_TRUE(c != nullptr);
    EXPECT_TRUE(c->isHead);
    EXPECT_TRUE(c->checkedOutBranch.empty());

    const CommitNode* b = find(nodes, "b");
    ASSERT_TRUE(b != nullptr);
    EXPECT_TRUE(!b->isHead);
    EXPECT_EQ(b->branchNames, (Names{"main"}));
}

TEST_F(GitRepositoryTest, HeadInfoNamesTheBranchAndCommit) {
    initRepo();
    commit("first");

    GitRepository repo;
    ASSERT_TRUE(repo.open(m_dir.string()));
    const HeadInfo info = repo.headInfo();

    EXPECT_EQ(info.branchName, "main");
    EXPECT_TRUE(!info.detached);
    EXPECT_EQ(info.oid.size(), 40u);
    EXPECT_EQ(info.oid, repo.getHeadOid());
}

TEST_F(GitRepositoryTest, LimitStopsTheWalk) {
    initRepo();
    commit("a");
    commit("b");
    commit("c");

    EXPECT_EQ(load(2).size(), 2u);
    EXPECT_EQ(load(0).size(), 3u);
}

TEST_F(GitRepositoryTest, StashCommitsAreNotShown) {
    initRepo();
    { std::ofstream(m_dir / "f.txt") << "one\n"; }
    ASSERT_TRUE(git("add f.txt"));
    commit("with file");
    { std::ofstream(m_dir / "f.txt", std::ios::app) << "two\n"; }
    ASSERT_TRUE(git("stash -q"));

    const auto nodes = load();
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].message, "with file");
}

} // namespace Git
