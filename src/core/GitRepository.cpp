#include "core/GitRepository.h"
#include <algorithm>
#include <iostream>
#include <mutex>

namespace Git {

static std::once_flag g_libgit2InitOnce;

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
    int error = git_repository_open_ext(&repoPtr, path.c_str(), 0, nullptr);
    if (error < 0 || !repoPtr) {
        return false;
    }
    m_repo.reset(repoPtr);
    m_path = path;
    return true;
}

void GitRepository::close() {
    m_repo.reset();
    m_path.clear();
}

bool GitRepository::isOpen() const {
    return m_repo != nullptr;
}

std::string GitRepository::getHeadOid() const {
    if (!isOpen()) {
        return {};
    }

    git_oid headOid;
    int error = git_reference_name_to_id(&headOid, m_repo.get(), "HEAD");
    if (error < 0) {
        return {};
    }

    char hexStr[GIT_OID_SHA1_HEXSIZE + 1] = {0};
    git_oid_tostr(hexStr, sizeof(hexStr), &headOid);
    return std::string(hexStr);
}

CommitNode GitRepository::parseCommit(const git_oid* oid) {
    CommitNode node;

    char hexStr[GIT_OID_SHA1_HEXSIZE + 1] = {0};
    git_oid_tostr(hexStr, sizeof(hexStr), oid);
    node.oid = std::string(hexStr);
    node.shortHash = node.oid.substr(0, 7);

    git_commit* commitPtr = nullptr;
    if (git_commit_lookup(&commitPtr, m_repo.get(), oid) < 0 || !commitPtr) {
        return node;
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
            char parentHex[GIT_OID_SHA1_HEXSIZE + 1] = {0};
            git_oid_tostr(parentHex, sizeof(parentHex), parentOid);
            node.parentOids.emplace_back(parentHex);
        }
    }

    return node;
}

std::vector<CommitNode> GitRepository::fetchCommitHistory(size_t limit) {
    std::vector<CommitNode> nodes;
    if (!isOpen()) {
        return nodes;
    }

    git_revwalk* walkPtr = nullptr;
    if (git_revwalk_new(&walkPtr, m_repo.get()) < 0 || !walkPtr) {
        return nodes;
    }
    UniqueGitRevwalk walk(walkPtr);

    // Set sorting mode: Topological + Time ordering for smooth DAG layout
    git_revwalk_sorting(walk.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
    
    // Push all reachable references (HEAD, local branches, tags, remotes)
    if (git_revwalk_push_glob(walk.get(), "refs/*") < 0) {
        // Fallback to HEAD if refs/* glob produces no targets
        if (git_revwalk_push_head(walk.get()) < 0) {
            return nodes;
        }
    }

    git_oid oid;
    size_t count = 0;
    while (git_revwalk_next(&oid, walk.get()) == 0) {
        nodes.push_back(parseCommit(&oid));
        count++;
        if (limit > 0 && count >= limit) {
            break;
        }
    }

    loadReferences(nodes);
    return nodes;
}

void GitRepository::loadReferences(std::vector<CommitNode>& nodes) {
    if (!isOpen() || nodes.empty()) {
        return;
    }

    std::string headOidStr = getHeadOid();

    git_reference_iterator* iterPtr = nullptr;
    if (git_reference_iterator_new(&iterPtr, m_repo.get()) < 0 || !iterPtr) {
        return;
    }
    UniqueGitReferenceIterator iter(iterPtr);

    git_reference* refPtr = nullptr;
    while (git_reference_next(&refPtr, iter.get()) == 0) {
        UniqueGitReference ref(refPtr);

        git_reference* resolvedRefPtr = nullptr;
        if (git_reference_resolve(&resolvedRefPtr, ref.get()) == 0 && resolvedRefPtr) {
            UniqueGitReference resolvedRef(resolvedRefPtr);
            const git_oid* targetOid = git_reference_target(resolvedRef.get());

            if (targetOid) {
                char targetHex[GIT_OID_SHA1_HEXSIZE + 1] = {0};
                git_oid_tostr(targetHex, sizeof(targetHex), targetOid);
                std::string targetOidStr(targetHex);

                const char* refName = git_reference_name(ref.get());
                if (refName) {
                    std::string fullRef(refName);
                    
                    auto it = std::find_if(nodes.begin(), nodes.end(), [&](const CommitNode& n) {
                        return n.oid == targetOidStr;
                    });

                    if (it != nodes.end()) {
                        if (targetOidStr == headOidStr) {
                            it->isHead = true;
                        }

                        if (fullRef.rfind("refs/heads/", 0) == 0) {
                            it->branchNames.push_back(fullRef.substr(11));
                        } else if (fullRef.rfind("refs/tags/", 0) == 0) {
                            it->tagNames.push_back(fullRef.substr(10));
                        } else if (fullRef.rfind("refs/remotes/", 0) == 0) {
                            it->branchNames.push_back(fullRef.substr(13));
                        }
                    }
                }
            }
        }
    }
}

} // namespace Git
