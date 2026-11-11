#include "core/GitRepository.h"
#include <mutex>

namespace Git {

namespace {

std::once_flag g_libgit2InitOnce;

std::string oidToString(const git_oid* oid) {
    char hex[GIT_OID_SHA1_HEXSIZE + 1] = {0};
    git_oid_tostr(hex, sizeof(hex), oid);
    return std::string(hex);
}

// "<what failed>: <libgit2's explanation>"
std::string describeError(const std::string& what) {
    const git_error* error = git_error_last();
    if (error && error->message) {
        return what + ": " + error->message;
    }
    return what;
}

} // namespace

void GitRepository::globalInitialize() {
    std::call_once(g_libgit2InitOnce, []() {
        git_libgit2_init();
    });
}

void GitRepository::globalShutdown() {
    // libgit2 cleanup optional on process termination,
    // managed centrally if required.
}

GitRepository::GitRepository() {
    globalInitialize();
}

GitRepository::~GitRepository() {
    close();
}

bool GitRepository::open(const std::string& path) {
    close();
    git_repository* repoPtr = nullptr;
    if (git_repository_open_ext(&repoPtr, path.c_str(), 0, nullptr) < 0 || !repoPtr) {
        m_lastError = describeError("Cannot open a Git repository at " + path);
        return false;
    }
    m_repo.reset(repoPtr);
    m_path = path;
    return true;
}

void GitRepository::close() {
    m_repo.reset();
    m_path.clear();
    m_lastError.clear();
}

bool GitRepository::isOpen() const {
    return m_repo != nullptr;
}

const std::string& GitRepository::lastError() const {
    return m_lastError;
}

std::string GitRepository::getHeadOid() const {
    return headInfo().oid;
}

HeadInfo GitRepository::headInfo() const {
    HeadInfo info;
    if (!isOpen()) {
        return info;
    }

    // No commits yet: HEAD names a branch that does not exist.
    if (git_repository_head_unborn(m_repo.get()) == 1) {
        return info;
    }

    git_reference* headPtr = nullptr;
    if (git_repository_head(&headPtr, m_repo.get()) < 0 || !headPtr) {
        return info;
    }
    UniqueGitReference head(headPtr);

    const git_oid* target = git_reference_target(head.get());
    if (target) {
        info.oid = oidToString(target);
    }

    info.detached = git_repository_head_detached(m_repo.get()) == 1;
    if (!info.detached) {
        const char* name = git_reference_name(head.get());
        const ParsedRef parsed = parseRefName(name ? name : "");
        if (parsed.kind == RefKind::LocalBranch) {
            info.branchName = parsed.shortName;
        }
    }

    return info;
}

bool GitRepository::parseCommit(const git_oid* oid, CommitNode& node) {
    node = CommitNode{};
    node.oid = oidToString(oid);
    node.shortHash = node.oid.substr(0, 7);

    git_commit* commitPtr = nullptr;
    if (git_commit_lookup(&commitPtr, m_repo.get(), oid) < 0 || !commitPtr) {
        m_lastError = describeError("Cannot read commit " + node.shortHash);
        return false;
    }
    UniqueGitCommit commit(commitPtr);

    const char* rawSummary = git_commit_summary(commit.get());
    node.message = rawSummary ? std::string(rawSummary) : "";

    const git_signature* authorSig = git_commit_author(commit.get());
    if (authorSig) {
        node.author = authorSig->name ? std::string(authorSig->name) : "";
        node.email = authorSig->email ? std::string(authorSig->email) : "";
        node.timestamp = static_cast<int64_t>(authorSig->when.time);
    }

    unsigned int parentCount = git_commit_parentcount(commit.get());
    node.parentOids.reserve(parentCount);
    for (unsigned int i = 0; i < parentCount; ++i) {
        const git_oid* parentOid = git_commit_parent_id(commit.get(), i);
        if (parentOid) {
            node.parentOids.push_back(oidToString(parentOid));
        }
    }

    return true;
}

bool GitRepository::fetchCommitHistory(std::vector<CommitNode>& out, std::size_t limit) {
    out.clear();
    if (!isOpen()) {
        m_lastError = "No repository is open.";
        return false;
    }

    git_revwalk* walkPtr = nullptr;
    if (git_revwalk_new(&walkPtr, m_repo.get()) < 0 || !walkPtr) {
        m_lastError = describeError("Cannot start reading the history");
        return false;
    }
    UniqueGitRevwalk walk(walkPtr);

    // Set sorting mode: Topological + Time ordering for smooth DAG layout
    git_revwalk_sorting(walk.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    // Branches and tags only. A plain "refs/*" would also pull in refs/stash and notes,
    // whose internal commits do not belong in the graph.
    for (const char* glob : {"refs/heads/*", "refs/remotes/*", "refs/tags/*"}) {
        if (git_revwalk_push_glob(walk.get(), glob) < 0) {
            m_lastError = describeError(std::string("Cannot read references matching ") + glob);
            return false;
        }
    }

    // HEAD as well: when it is detached it may not be reachable from any branch.
    // A repository without commits has no HEAD commit yet, and that is fine.
    if (git_repository_head_unborn(m_repo.get()) != 1 && git_revwalk_push_head(walk.get()) < 0) {
        m_lastError = describeError("Cannot read HEAD");
        return false;
    }

    git_oid oid;
    int status = 0;
    while (limit == 0 || out.size() < limit) {
        status = git_revwalk_next(&oid, walk.get());
        if (status != 0) {
            break;
        }

        CommitNode node;
        if (!parseCommit(&oid, node)) {
            out.clear();
            return false;
        }
        out.push_back(std::move(node));
    }

    if (status < 0 && status != GIT_ITEROVER) {
        m_lastError = describeError("Reading the history failed");
        out.clear();
        return false;
    }

    loadReferences(out);
    m_lastError.clear();
    return true;
}

void GitRepository::loadReferences(std::vector<CommitNode>& nodes) {
    if (!isOpen() || nodes.empty()) {
        return;
    }

    std::vector<RefEntry> refs;

    git_reference_iterator* iterPtr = nullptr;
    if (git_reference_iterator_new(&iterPtr, m_repo.get()) == 0 && iterPtr) {
        UniqueGitReferenceIterator iter(iterPtr);

        git_reference* refPtr = nullptr;
        while (git_reference_next(&refPtr, iter.get()) == 0) {
            UniqueGitReference ref(refPtr);

            const char* name = git_reference_name(ref.get());
            if (!name || parseRefName(name).kind == RefKind::Other) {
                continue;   // stash, notes, remote HEAD alias, ...
            }

            git_reference* resolvedPtr = nullptr;
            if (git_reference_resolve(&resolvedPtr, ref.get()) < 0 || !resolvedPtr) {
                continue;
            }
            UniqueGitReference resolved(resolvedPtr);

            // An annotated tag points at a tag object, not at a commit: peel it to the commit.
            git_object* commitObjectPtr = nullptr;
            if (git_reference_peel(&commitObjectPtr, resolved.get(), GIT_OBJECT_COMMIT) < 0 || !commitObjectPtr) {
                continue;   // e.g. a tag on a tree or blob
            }
            UniqueGitObject commitObject(commitObjectPtr);

            refs.push_back(RefEntry{std::string(name), oidToString(git_object_id(commitObject.get()))});
        }
    }

    attachRefs(nodes, refs, headInfo());
}

} // namespace Git
