#ifndef GITREPOSITORY_H
#define GITREPOSITORY_H

#include <string>
#include <vector>
#include <memory>
#include <git2.h>
#include "core/GitTypes.h"
#include "core/GitRefs.h"
#include "core/OidCache.h"

// libgit2 older than 1.7 calls this constant GIT_OID_HEXSZ.
#ifndef GIT_OID_SHA1_HEXSIZE
#define GIT_OID_SHA1_HEXSIZE GIT_OID_HEXSZ
#endif

namespace Git {

// Custom RAII Deleters for libgit2 C handles
struct GitRepositoryDeleter {
    void operator()(git_repository* repo) const {
        if (repo) {
            git_repository_free(repo);
        }
    }
};

struct GitCommitDeleter {
    void operator()(git_commit* commit) const {
        if (commit) {
            git_commit_free(commit);
        }
    }
};

struct GitRevwalkDeleter {
    void operator()(git_revwalk* walk) const {
        if (walk) {
            git_revwalk_free(walk);
        }
    }
};

struct GitReferenceDeleter {
    void operator()(git_reference* ref) const {
        if (ref) {
            git_reference_free(ref);
        }
    }
};

struct GitObjectDeleter {
    void operator()(git_object* object) const {
        if (object) {
            git_object_free(object);
        }
    }
};

struct GitReferenceIteratorDeleter {
    void operator()(git_reference_iterator* iter) const {
        if (iter) {
            git_reference_iterator_free(iter);
        }
    }
};

struct GitTreeDeleter {
    void operator()(git_tree* tree) const {
        if (tree) {
            git_tree_free(tree);
        }
    }
};

struct GitDiffDeleter {
    void operator()(git_diff* diff) const {
        if (diff) {
            git_diff_free(diff);
        }
    }
};

using UniqueGitRepository = std::unique_ptr<git_repository, GitRepositoryDeleter>;
using UniqueGitCommit = std::unique_ptr<git_commit, GitCommitDeleter>;
using UniqueGitRevwalk = std::unique_ptr<git_revwalk, GitRevwalkDeleter>;
using UniqueGitReference = std::unique_ptr<git_reference, GitReferenceDeleter>;
using UniqueGitObject = std::unique_ptr<git_object, GitObjectDeleter>;
using UniqueGitTree = std::unique_ptr<git_tree, GitTreeDeleter>;
using UniqueGitDiff = std::unique_ptr<git_diff, GitDiffDeleter>;
using UniqueGitReferenceIterator = std::unique_ptr<git_reference_iterator, GitReferenceIteratorDeleter>;

// Parsed commits (message, author, parents, ...) by commit id. See GitRepository::setCommitCache.
using CommitCache = OidCache<CommitNode>;

class GitRepository {
public:
    GitRepository();
    ~GitRepository();

    // Prevent copying to maintain strict ownership over C pointers
    GitRepository(const GitRepository&) = delete;
    GitRepository& operator=(const GitRepository&) = delete;

    // Allow move semantics
    GitRepository(GitRepository&&) noexcept = default;
    GitRepository& operator=(GitRepository&&) noexcept = default;

    /// Opens the repository containing `path`. On failure returns false and sets lastError().
    [[nodiscard]] bool open(const std::string& path);
    void close();
    [[nodiscard]] bool isOpen() const;

    /// Human-readable description of the last failure; empty after a successful call.
    [[nodiscard]] const std::string& lastError() const;

    /// Lets fetchCommitHistory skip re-reading commits it has already parsed. The cache is not
    /// owned, may be null, and must only be used from the thread that uses this repository.
    void setCommitCache(CommitCache* cache) { m_commitCache = cache; }

    [[nodiscard]] std::string getHeadOid() const;

    /// Commit and branch that HEAD points at (see HeadInfo).
    [[nodiscard]] HeadInfo headInfo() const;

    /**
     * @brief Reads the history of all local branches, remote branches, tags and HEAD,
     *        newest first, in topological order, with branch/tag badges attached.
     * @param out   Replaced with the commits. Empty (and still a success) for a repository with no commits.
     * @param limit Maximum number of commits; 0 means no limit.
     * @return false on failure, with the reason in lastError(). `out` is empty then.
     */
    [[nodiscard]] bool fetchCommitHistory(std::vector<CommitNode>& out, std::size_t limit = 0);

    /**
     * @brief Reads one commit completely: full message, author and committer, parents, and the
     *        patch against the first parent (the whole tree for a root commit), parsed into files.
     * @param maxPatchBytes The patch is cut after about this many bytes (patchTruncated is set);
     *                      0 means no limit.
     * @return false only if the commit itself cannot be read (reason in lastError()). A missing
     *         parent (shallow clone) is not a failure: the metadata is returned and `patchError`
     *         explains why there is no patch.
     */
    [[nodiscard]] bool loadCommitDetails(const std::string& oid, CommitDetails& out,
                                         std::size_t maxPatchBytes = 0);

private:
    UniqueGitRepository m_repo{nullptr};
    std::string m_path;
    std::string m_lastError;
    CommitCache* m_commitCache{nullptr};

    static void globalInitialize();
    static void globalShutdown();

    [[nodiscard]] bool parseCommit(const git_oid* oid, CommitNode& node);
    void loadReferences(std::vector<CommitNode>& nodes);
};

} // namespace Git

#endif // GITREPOSITORY_H
