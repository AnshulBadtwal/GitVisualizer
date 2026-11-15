#ifndef GITTYPES_H
#define GITTYPES_H

#include <string>
#include <vector>
#include <cstdint>
#include <QMetaType>
#include <QColor>
#include <QtCore/qnamespace.h>
#include "core/DiffText.h"

namespace Git {

enum class ConnectionPathType {
    Active,
    Merge,
    Fork
};

// A line leaving a commit towards one of its parents.
//  - targetLane: the lane the line occupies where it leaves this row (bottom edge).
//                When several branches converge on one parent, the parent's own lane
//                can differ; the parent row then lists a LaneSegment that joins them.
//  - targetRow:  row index of the parent commit (informational, not needed to draw).
struct Connection {
    int targetLane{0};
    int targetRow{0};
    ConnectionPathType pathType{ConnectionPathType::Active};
    QColor color;
};

// A line that arrives at a commit from the row above (top edge of the row).
//  - fromLane == toLane : the commit's own branch continuing straight down into it.
//  - fromLane != toLane : another branch joining here (pathType Fork = branch point).
struct LaneSegment {
    int fromLane{0};
    int toLane{0};
    ConnectionPathType pathType{ConnectionPathType::Active};
    QColor color;
};

struct CommitNode {
    std::string oid;
    std::string shortHash;
    std::string message;
    std::string author;
    std::string email;
    int64_t timestamp{0};
    std::vector<std::string> parentOids;
    
    // Graph layout calculations (filled in by GraphLayoutEngine::layout)
    int lane{0};
    int maxLanes{1};
    std::vector<Connection> connections;      // bottom half: lines to parents
    std::vector<LaneSegment> incoming;        // top half: lines arriving from children
    std::vector<int> passThroughLanes;        // lanes drawn as full-height verticals in this row
    bool isHead{false};
    std::string checkedOutBranch;                // set on the HEAD commit only; empty when HEAD is detached
    std::vector<std::string> branchNames;        // local branches (refs/heads/...)
    std::vector<std::string> remoteBranchNames;  // remote-tracking branches (refs/remotes/...)
    std::vector<std::string> tagNames;
};

// Everything the details panel and the diff viewer show about one commit. Filled by
// GitRepository::loadCommitDetails on a worker thread (see DetailsWorker).
struct CommitDetails {
    std::string oid;
    std::string fullMessage;        // subject and body, as written
    std::string authorName;
    std::string authorEmail;
    int64_t authorTime{0};
    std::string committerName;
    std::string committerEmail;
    int64_t committerTime{0};
    std::vector<std::string> parentOids;

    std::string patch;              // unified diff against the first parent (empty for no changes)
    bool patchTruncated{false};     // the patch was cut at the size limit
    std::string patchError;         // why there is no patch, e.g. the parent is missing (shallow clone)
    ParsedPatch parsed;             // parsePatch(patch): line kinds and the list of changed files

    std::string error;              // the commit itself could not be read; everything else is empty
    [[nodiscard]] bool ok() const { return error.empty(); }
};

enum GraphColumn {
    Graph = 0,
    Commit = 1,
    Author = 2,
    Date = 3,
    Hash = 4,
    ColumnCount = 5
};

enum CustomRoles {
    CommitRole = Qt::UserRole + 1,   // QVariant holding a copy of the CommitNode
    CommitGraphNodeRole,             // QVariant holding a `const CommitNode *` (no copy; valid until the model changes)
    OidRole
};

} // namespace Git

Q_DECLARE_METATYPE(Git::CommitNode)
Q_DECLARE_METATYPE(Git::CommitDetails)
Q_DECLARE_METATYPE(std::vector<Git::CommitNode>)

#endif // GITTYPES_H
