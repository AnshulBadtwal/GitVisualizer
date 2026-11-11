// Tests for the auto-refresh helpers. They use real repositories built with the `git` command.

#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include "TempRepoFixture.h"
#include "core/GitWatch.h"

namespace Git {

class GitWatchTest : public TempRepoFixture {
protected:
    fs::path m_worktree;

    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(m_worktree, ignored);
        TempRepoFixture::TearDown();
    }

    GitDirs dirs() const { return findGitDirs(m_dir.string()); }
    std::size_t fingerprint() const { return refsFingerprint(dirs()); }

    static bool contains(const std::vector<std::string>& list, const fs::path& path) {
        return std::find(list.begin(), list.end(), path.string()) != list.end();
    }

    static void writeFile(const fs::path& path, const std::string& text) {
        std::ofstream(path) << text;
    }
};

// --- finding the Git folders ---------------------------------------------------

TEST_F(GitWatchTest, FindsTheGitFolder) {
    initRepo();
    const GitDirs found = dirs();
    ASSERT_TRUE(found.valid());
    EXPECT_EQ(found.gitDir, (m_dir / ".git").string());
    EXPECT_EQ(found.commonDir, found.gitDir);
}

TEST_F(GitWatchTest, FindsTheRepositoryFromAFolderInsideIt) {
    initRepo();
    fs::create_directories(m_dir / "src" / "deep");
    const GitDirs found = findGitDirs((m_dir / "src" / "deep").string());
    ASSERT_TRUE(found.valid());
    EXPECT_EQ(found.gitDir, (m_dir / ".git").string());
}

TEST_F(GitWatchTest, ToleratesATrailingSlash) {
    initRepo();
    const GitDirs found = findGitDirs(m_dir.string() + "/");
    ASSERT_TRUE(found.valid());
    EXPECT_EQ(found.gitDir, (m_dir / ".git").string());
}

TEST_F(GitWatchTest, RecognisesTheGitFolderItself) {
    initRepo();
    const GitDirs found = findGitDirs((m_dir / ".git").string());
    ASSERT_TRUE(found.valid());
    EXPECT_EQ(found.gitDir, (m_dir / ".git").string());
}

TEST_F(GitWatchTest, RecognisesABareRepository) {
    ASSERT_TRUE(git("init -q --bare -b main"));
    const GitDirs found = dirs();
    ASSERT_TRUE(found.valid());
    EXPECT_EQ(found.gitDir, m_dir.string());
}

TEST_F(GitWatchTest, FollowsTheGitFileOfALinkedWorktree) {
    initRepo();
    commit("a");
    m_worktree = m_dir.string() + "-worktree";
    ASSERT_TRUE(git("worktree add -q -b wt '" + m_worktree.string() + "'"));

    const GitDirs found = findGitDirs(m_worktree.string());
    ASSERT_TRUE(found.valid());
    EXPECT_TRUE(fs::equivalent(found.commonDir, m_dir / ".git"));
    EXPECT_TRUE(found.gitDir != found.commonDir);
    EXPECT_TRUE(fs::exists(fs::path(found.gitDir) / "HEAD"));
}

TEST_F(GitWatchTest, AFolderThatIsNotARepositoryGivesAnInvalidResult) {
    EXPECT_TRUE(!dirs().valid());
    EXPECT_TRUE(!findGitDirs("").valid());
}

// --- which folders to watch ----------------------------------------------------

TEST_F(GitWatchTest, WatchesEveryFolderUnderRefs) {
    initRepo();
    commit("a");
    ASSERT_TRUE(git("branch feature/login"));
    ASSERT_TRUE(git("branch fix/a/b"));
    ASSERT_TRUE(git("tag v1"));
    ASSERT_TRUE(git("update-ref refs/remotes/origin/main HEAD"));
    ASSERT_TRUE(git("update-ref refs/remotes/origin/feature/x HEAD"));

    const auto watched = watchedDirectories(dirs());
    const fs::path git = m_dir / ".git";
    EXPECT_TRUE(contains(watched, git));
    EXPECT_TRUE(contains(watched, git / "refs"));
    EXPECT_TRUE(contains(watched, git / "refs" / "heads"));
    EXPECT_TRUE(contains(watched, git / "refs" / "heads" / "feature"));
    EXPECT_TRUE(contains(watched, git / "refs" / "heads" / "fix"));
    EXPECT_TRUE(contains(watched, git / "refs" / "heads" / "fix" / "a"));
    EXPECT_TRUE(contains(watched, git / "refs" / "tags"));
    EXPECT_TRUE(contains(watched, git / "refs" / "remotes"));
    EXPECT_TRUE(contains(watched, git / "refs" / "remotes" / "origin"));
    EXPECT_TRUE(contains(watched, git / "refs" / "remotes" / "origin" / "feature"));
}

TEST_F(GitWatchTest, NewBranchFoldersAppearOnTheNextScan) {
    initRepo();
    commit("a");
    const fs::path teamFolder = m_dir / ".git" / "refs" / "heads" / "team";
    EXPECT_TRUE(!contains(watchedDirectories(dirs()), teamFolder));

    ASSERT_TRUE(git("branch team/x"));
    EXPECT_TRUE(contains(watchedDirectories(dirs()), teamFolder));
}

TEST_F(GitWatchTest, AWorktreeWatchesBothItsOwnFolderAndTheMainRefs) {
    initRepo();
    commit("a");
    m_worktree = m_dir.string() + "-worktree";
    ASSERT_TRUE(git("worktree add -q -b wt '" + m_worktree.string() + "'"));

    const GitDirs found = findGitDirs(m_worktree.string());
    const auto watched = watchedDirectories(found);
    EXPECT_TRUE(std::find(watched.begin(), watched.end(), found.gitDir) != watched.end());
    EXPECT_TRUE(std::find(watched.begin(), watched.end(), found.commonDir) != watched.end());
    EXPECT_TRUE(contains(watched, fs::path(found.commonDir) / "refs" / "heads"));
}

TEST_F(GitWatchTest, NothingIsWatchedForAnInvalidResult) {
    EXPECT_TRUE(watchedDirectories(GitDirs{}).empty());
    EXPECT_EQ(refsFingerprint(GitDirs{}), 0u);
}

// --- fingerprint ----------------------------------------------------------------

TEST_F(GitWatchTest, FingerprintStaysTheSameWhenNothingChanges) {
    initRepo();
    commit("a");
    EXPECT_EQ(fingerprint(), fingerprint());
}

TEST_F(GitWatchTest, FingerprintChangesWhenABranchMoves) {
    initRepo();
    commit("a");
    const std::size_t before = fingerprint();
    commit("b");
    EXPECT_TRUE(fingerprint() != before);
}

TEST_F(GitWatchTest, FingerprintChangesForBranchesTagsAndCheckout) {
    initRepo();
    commit("a");
    const std::size_t start = fingerprint();

    ASSERT_TRUE(git("branch dev"));
    const std::size_t afterBranch = fingerprint();
    EXPECT_TRUE(afterBranch != start);

    ASSERT_TRUE(git("tag v1"));
    const std::size_t afterTag = fingerprint();
    EXPECT_TRUE(afterTag != afterBranch);

    ASSERT_TRUE(git("checkout -q dev"));
    EXPECT_TRUE(fingerprint() != afterTag);
}

TEST_F(GitWatchTest, FingerprintChangesForRemoteBranches) {
    initRepo();
    commit("a");
    const std::size_t before = fingerprint();
    ASSERT_TRUE(git("update-ref refs/remotes/origin/main HEAD"));
    EXPECT_TRUE(fingerprint() != before);
}

TEST_F(GitWatchTest, FingerprintIgnoresNoiseInsideTheGitFolder) {
    initRepo();
    commit("a");
    const std::size_t before = fingerprint();

    ASSERT_TRUE(git("status"));
    writeFile(m_dir / ".git" / "index.lock", "");
    writeFile(m_dir / ".git" / "FETCH_HEAD", "abc\n");
    writeFile(m_dir / ".git" / "refs" / "heads" / "main.lock", "0000000000000000000000000000000000000000\n");

    EXPECT_EQ(fingerprint(), before);
}

TEST_F(GitWatchTest, FingerprintIgnoresTheStash) {
    initRepo();
    writeFile(m_dir / "f.txt", "one\n");
    ASSERT_TRUE(git("add f.txt"));
    commit("with file");
    const std::size_t before = fingerprint();

    writeFile(m_dir / "f.txt", "two\n");
    ASSERT_TRUE(git("stash -q"));

    EXPECT_EQ(fingerprint(), before);
}

TEST_F(GitWatchTest, FingerprintStillNoticesNewCommitsAfterPackRefs) {
    initRepo();
    commit("a");
    ASSERT_TRUE(git("pack-refs --all"));
    const std::size_t packed = fingerprint();

    commit("b");
    EXPECT_TRUE(fingerprint() != packed);
}

} // namespace Git
