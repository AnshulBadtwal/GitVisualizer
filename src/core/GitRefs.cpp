#include "core/GitRefs.h"
#include <algorithm>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <utility>

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

// Custom Quicksort implementation (Lomuto partition scheme with median-of-three pivot)
template <typename T, typename Compare>
std::size_t partition(std::vector<T> &vec, std::size_t low, std::size_t high, Compare comp) {
    // Median-of-three pivot selection to avoid worst-case O(N^2) on already-sorted data
    const std::size_t mid = low + (high - low) / 2;
    if (comp(vec[mid], vec[low])) {
        std::swap(vec[low], vec[mid]);
    }
    if (comp(vec[high], vec[low])) {
        std::swap(vec[low], vec[high]);
    }
    if (comp(vec[mid], vec[high])) {
        std::swap(vec[mid], vec[high]);
    }

    const T &pivot = vec[high];
    std::size_t i = low;
    for (std::size_t j = low; j < high; ++j) {
        if (comp(vec[j], pivot)) {
            std::swap(vec[i], vec[j]);
            ++i;
        }
    }
    std::swap(vec[i], vec[high]);
    return i;
}

template <typename T, typename Compare>
void quickSortImpl(std::vector<T> &vec, std::size_t low, std::size_t high, Compare comp) {
    if (low >= high) {
        return;
    }
    const std::size_t pivotIndex = partition(vec, low, high, comp);
    if (pivotIndex > 0) {
        quickSortImpl(vec, low, pivotIndex - 1, comp);
    }
    quickSortImpl(vec, pivotIndex + 1, high, comp);
}

template <typename T, typename Compare = std::less<T>>
void quickSort(std::vector<T> &vec, Compare comp = Compare{}) {
    if (vec.size() < 2) {
        return;
    }
    quickSortImpl(vec, 0, vec.size() - 1, comp);
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
    quickSort(touched);
    touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
    for (const size_t index : touched) {
        CommitNode &node = nodes[index];
        const std::string &current = node.checkedOutBranch;

        quickSort(node.branchNames,
                  [&current](const std::string &a, const std::string &b) {
                      const bool aCurrent = !current.empty() && a == current;
                      const bool bCurrent = !current.empty() && b == current;
                      if (aCurrent != bCurrent) {
                          return aCurrent;
                      }
                      return a < b;
                  });
        quickSort(node.remoteBranchNames);
        quickSort(node.tagNames);
    }
}

} // namespace Git