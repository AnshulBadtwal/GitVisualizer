#ifndef GITREPOSITORY_H
#define GITREPOSITORY_H

#include <string>
#include <vector>
#include <memory>
#include <git2.h>
#include "core/GitTypes.h"

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

struct GitReferenceIteratorDeleter {
    void operator()(git_reference_iterator* iter) const {
        if (iter) {
            git_reference_iterator_free(iter);
        }
    }
};

using UniqueGitRepository = std::unique_ptr<git_repository, GitRepositoryDeleter>;
using UniqueGitCommit = std::unique_ptr<git_commit, GitCommitDeleter>;
using UniqueGitRevwalk = std::unique_ptr<git_revwalk, GitRevwalkDeleter>;
using UniqueGitReference = std::unique_ptr<git_reference, GitReferenceDeleter>;
using UniqueGitReferenceIterator = std::unique_ptr<git_reference_iterator, GitReferenceIteratorDeleter>;

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

    bool open(const std::string& path);
    void close();
    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] std::string getHeadOid() const;
    [[nodiscard]] std::vector<CommitNode> fetchCommitHistory(size_t limit = 0);

private:
    UniqueGitRepository m_repo{nullptr};
    std::string m_path;

    static void globalInitialize();
    static void globalShutdown();

    [[nodiscard]] CommitNode parseCommit(const git_oid* oid);
    void loadReferences(std::vector<CommitNode>& nodes);
};

} // namespace Git

#endif // GITREPOSITORY_H
