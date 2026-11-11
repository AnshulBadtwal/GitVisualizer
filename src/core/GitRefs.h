#ifndef GITREFS_H
#define GITREFS_H

#include <string>
#include <vector>
#include "core/GitTypes.h"

// Pure logic for putting branch / tag badges onto commits. It never touches libgit2,
// so it can be tested without a repository (GitRepository gathers the facts, this applies them).

namespace Git {

enum class RefKind {
    LocalBranch,
    RemoteBranch,
    Tag,
    Other        // stash, notes, remote HEAD, ...: never shown
};

struct ParsedRef {
    RefKind kind{RefKind::Other};
    std::string shortName;   // "main", "origin/main", "v1.0"; the full name for RefKind::Other
};

// One reference, already resolved: annotated tags must be peeled to the commit they point at.
struct RefEntry {
    std::string fullName;    // e.g. "refs/heads/main"
    std::string commitOid;
};

struct HeadInfo {
    std::string oid;         // commit HEAD points at; empty for a repository without commits
    std::string branchName;  // checked-out local branch; empty when HEAD is detached
    bool detached{false};
};

/// Splits "refs/heads/x", "refs/remotes/x" and "refs/tags/x" into kind and short name.
/// Remote HEAD aliases ("refs/remotes/origin/HEAD") are RefKind::Other.
[[nodiscard]] ParsedRef parseRefName(const std::string &fullName);

/// Fills branchNames, remoteBranchNames, tagNames, isHead and checkedOutBranch on `nodes`.
/// Anything set by an earlier call is reset first. Refs whose commit is not in `nodes`
/// (for example because the history was limited) are ignored. Names on one commit are
/// sorted alphabetically, except that the checked-out branch comes first.
void attachRefs(std::vector<CommitNode> &nodes, const std::vector<RefEntry> &refs, const HeadInfo &head);

} // namespace Git

#endif // GITREFS_H
