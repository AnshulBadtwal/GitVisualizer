#include "core/GitRefs.h"
#include <algorithm>
#include <string_view>
#include <unordered_map>

namespace Git {

namespace {

constexpr std::string_view kHeadsPrefix = "refs/heads/";
constexpr std::string_view kRemotesPrefix = "refs/remotes/";
constexpr std::string_view kTagsPrefix = "refs/tags/";

bool startsWith(const std::string &text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string &text, std::string_view suffix) {
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

ParsedRef parseRefName(const std::string &fullName) {
    ParsedRef parsed;
    parsed.shortName = fullName;

    if (startsWith(fullName, kHeadsPrefix)) {
        std::string rest = fullName.substr(kHeadsPrefix.size());
        if (!rest.empty()) {
            parsed.kind = RefKind::LocalBranch;
            parsed.shortName = std::move(rest);
        }
    } else if (startsWith(fullName, kRemotesPrefix)) {
        std::string rest = fullName.substr(kRemotesPrefix.size());
        const bool isRemoteHeadAlias = rest == "HEAD" || endsWith(rest, "/HEAD");
        if (!rest.empty() && !isRemoteHeadAlias) {
            parsed.kind = RefKind::RemoteBranch;
            parsed.shortName = std::move(rest);
        }
    } else if (startsWith(fullName, kTagsPrefix)) {
        std::string rest = fullName.substr(kTagsPrefix.size());
        if (!rest.empty()) {
            parsed.kind = RefKind::Tag;
            parsed.shortName = std::move(rest);
        }
    }

    return parsed;
}

void attachRefs(std::vector<CommitNode> &nodes, const std::vector<RefEntry> &refs, const HeadInfo &head) {
    if (nodes.empty()) {
        return;
    }

    // One lookup table for all refs, instead of scanning every commit once per ref.
    std::unordered_map<std::string, size_t> oidToIndex;
    oidToIndex.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        oidToIndex.emplace(nodes[i].oid, i);

        // Start clean so calling this twice does not duplicate names.
        nodes[i].isHead = false;
        nodes[i].checkedOutBranch.clear();
        nodes[i].branchNames.clear();
        nodes[i].remoteBranchNames.clear();
        nodes[i].tagNames.clear();
    }

    std::vector<size_t> touched;

    for (const RefEntry &ref : refs) {
        const ParsedRef parsed = parseRefName(ref.fullName);
        if (parsed.kind == RefKind::Other) {
            continue;
        }

        const auto found = oidToIndex.find(ref.commitOid);
        if (found == oidToIndex.end()) {
            continue;
        }

        CommitNode &node = nodes[found->second];
        switch (parsed.kind) {
        case RefKind::LocalBranch:  node.branchNames.push_back(parsed.shortName); break;
        case RefKind::RemoteBranch: node.remoteBranchNames.push_back(parsed.shortName); break;
        case RefKind::Tag:          node.tagNames.push_back(parsed.shortName); break;
        case RefKind::Other:        break;
        }
        touched.push_back(found->second);
    }

    if (!head.oid.empty()) {
        const auto found = oidToIndex.find(head.oid);
        if (found != oidToIndex.end()) {
            CommitNode &node = nodes[found->second];
            node.isHead = true;
            node.checkedOutBranch = head.detached ? std::string() : head.branchName;
        }
    }

    // Stable display order: checked-out branch first, then alphabetical.
    std::sort(touched.begin(), touched.end());
    touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
    for (const size_t index : touched) {
        CommitNode &node = nodes[index];
        const std::string &current = node.checkedOutBranch;

        std::sort(node.branchNames.begin(), node.branchNames.end(),
                  [&current](const std::string &a, const std::string &b) {
                      const bool aCurrent = !current.empty() && a == current;
                      const bool bCurrent = !current.empty() && b == current;
                      if (aCurrent != bCurrent) {
                          return aCurrent;
                      }
                      return a < b;
                  });
        std::sort(node.remoteBranchNames.begin(), node.remoteBranchNames.end());
        std::sort(node.tagNames.begin(), node.tagNames.end());
    }
}

} // namespace Git
