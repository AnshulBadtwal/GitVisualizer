#ifndef GITTYPES_H
#define GITTYPES_H

#include <string>
#include <vector>
#include <cstdint>
#include <QMetaType>
#include <QColor>

namespace Git {

enum class ConnectionPathType {
    Active,
    Merge,
    Fork
};

struct Connection {
    int targetLane{0};
    int targetRow{0};
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
    
    // Graph layout calculations
    int lane{0};
    int maxLanes{1};
    std::vector<Connection> connections;
    bool isHead{false};
    std::vector<std::string> branchNames;
    std::vector<std::string> tagNames;
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
    CommitRole = Qt::UserRole + 1,
    CommitGraphNodeRole,
    OidRole
};

} // namespace Git

Q_DECLARE_METATYPE(Git::CommitNode)
Q_DECLARE_METATYPE(std::vector<Git::CommitNode>)

#endif // GITTYPES_H
