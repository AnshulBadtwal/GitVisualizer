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

// Rough size of a cached commit in bytes. It only has to be consistent, not exact.
std::size_t commitCost(const CommitNode& n) {
    return sizeof(CommitNode) + 2 * n.oid.size() + n.shortHash.size() + n.message.size()
         + n.author.size() + n.email.size()
         + n.parentOids.size() * (sizeof(std::string) + n.oid.size());
}

// Collects the text of a diff, and stops (non-zero return) once it is big enough.
struct PatchSink {
    std::string text;
    std::size_t limit{0};      // 0 = no limit
    bool truncated{false};
};

int collectPatchLine(const git_diff_delta*, const git_diff_hunk*, const git_diff_line* line, void* payload) {
    auto* sink = static_cast<PatchSink*>(payload);
    if (!line || !sink) {
        return 0;
    }
    if (sink->limit != 0 && sink->text.size() >= sink->limit) {
        sink->truncated = true;
        return 1;
    }
    // libgit2 keeps the leading '+', '-' or ' ' of a line out of its content: it is in `origin`.
    // (This is what libgit2's own diff example does.) Header lines already contain everything.
    if (line->origin == GIT_DIFF_LINE_CONTEXT || line->origin == GIT_DIFF_LINE_ADDITION
        || line->origin == GIT_DIFF_LINE_DELETION) {
        sink->text.push_back(line->origin);
    }
    sink->text.append(line->content, line->content_len);
    return 0;
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
    const std::string oidText = oidToString(oid);

    // A commit id is a hash of the commit, so what was read for it earlier is still correct.
    if (m_commitCache) {
        if (const CommitNode* cached = m_commitCache->find(oidText)) {
            node = *cached;   // refs and layout fields are empty here; later steps fill them in
            return true;
        }
    }

    node = CommitNode{};
    node.oid = oidText;
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

    if (m_commitCache) {
        m_commitCache->put(node.oid, node, commitCost(node));   // only after a successful parse
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

bool GitRepository::loadCommitDetails(const std::string& oidText, CommitDetails& out, std::size_t maxPatchBytes) {
    out = CommitDetails{};
    out.oid = oidText;

    if (!isOpen()) {
        m_lastError = "No repository is open.";
        return false;
    }

    git_oid oid;
    if (git_oid_fromstr(&oid, oidText.c_str()) < 0) {
        m_lastError = describeError("Invalid commit id " + oidText);
        return false;
    }

    git_commit* commitPtr = nullptr;
    if (git_commit_lookup(&commitPtr, m_repo.get(), &oid) < 0 || !commitPtr) {
        m_lastError = describeError("Cannot read commit " + oidText.substr(0, 7));
        return false;
    }
    UniqueGitCommit commit(commitPtr);

    if (const char* message = git_commit_message(commit.get())) {
        out.fullMessage = message;
    }
    if (const git_signature* author = git_commit_author(commit.get())) {
        out.authorName = author->name ? author->name : "";
        out.authorEmail = author->email ? author->email : "";
        out.authorTime = static_cast<int64_t>(author->when.time);
    }
    if (const git_signature* committer = git_commit_committer(commit.get())) {
        out.committerName = committer->name ? committer->name : "";
        out.committerEmail = committer->email ? committer->email : "";
        out.committerTime = static_cast<int64_t>(committer->when.time);
    }

    const unsigned int parentCount = git_commit_parentcount(commit.get());
    out.parentOids.reserve(parentCount);
    for (unsigned int i = 0; i < parentCount; ++i) {
        if (const git_oid* parentOid = git_commit_parent_id(commit.get(), i)) {
            out.parentOids.push_back(oidToString(parentOid));
        }
    }

    // From here on a problem only means "no patch": the metadata above is still worth showing.
    git_tree* treePtr = nullptr;
    if (git_commit_tree(&treePtr, commit.get()) < 0 || !treePtr) {
        out.patchError = describeError("Cannot read the files of this commit");
        m_lastError.clear();
        return true;
    }
    UniqueGitTree tree(treePtr);

    UniqueGitTree parentTree;   // stays empty for a root commit: everything counts as added
    if (parentCount > 0) {
        git_commit* parentPtr = nullptr;
        if (git_commit_parent(&parentPtr, commit.get(), 0) < 0 || !parentPtr) {
            out.patchError = "The parent commit is not available (shallow clone?), so there is no diff to show.";
            m_lastError.clear();
            return true;
        }
        UniqueGitCommit parent(parentPtr);

        git_tree* parentTreePtr = nullptr;
        if (git_commit_tree(&parentTreePtr, parent.get()) < 0 || !parentTreePtr) {
            out.patchError = describeError("Cannot read the files of the parent commit");
            m_lastError.clear();
            return true;
        }
        parentTree.reset(parentTreePtr);
    }

    git_diff* diffPtr = nullptr;
    if (git_diff_tree_to_tree(&diffPtr, m_repo.get(), parentTree.get(), tree.get(), nullptr) < 0 || !diffPtr) {
        out.patchError = describeError("Cannot compare the commit with its parent");
        m_lastError.clear();
        return true;
    }
    UniqueGitDiff diff(diffPtr);

    PatchSink sink;
    sink.limit = maxPatchBytes;
    const int printStatus = git_diff_print(diff.get(), GIT_DIFF_FORMAT_PATCH, collectPatchLine, &sink);
    if (printStatus < 0 && !sink.truncated) {
        out.patchError = describeError("Cannot format the diff");
        m_lastError.clear();
        return true;
    }

    out.patch = std::move(sink.text);
    out.patchTruncated = sink.truncated;
    out.parsed = parsePatch(out.patch);
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
