#include "models/CommitTableModel.h"
#include <QBrush>
#include <QDateTime>
#include <QColor>
#include <QString>

namespace Git {

CommitTableModel::CommitTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int CommitTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_nodes.size());
}

int CommitTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return GraphColumn::ColumnCount;
}

QVariant CommitTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_nodes.size())) {
        return {};
    }

    const auto &node = m_nodes[static_cast<size_t>(index.row())];

    if (role == CustomRoles::CommitRole) {
        return QVariant::fromValue(node);
    }

    if (role == CustomRoles::CommitGraphNodeRole) {
        // Pointer, not a copy: paint() runs for every visible row on every repaint.
        return QVariant::fromValue(&node);
    }

    if (role == CustomRoles::OidRole) {
        return QString::fromStdString(node.oid);
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case GraphColumn::Graph:
            return {}; // Managed by GraphLaneDelegate
        case GraphColumn::Commit:
            return QString::fromUtf8(node.message.c_str());
        case GraphColumn::Author:
            return QString::fromUtf8(node.author.c_str());
        case GraphColumn::Date: {
            QDateTime dt = QDateTime::fromSecsSinceEpoch(node.timestamp);
            return dt.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        }
        case GraphColumn::Hash:
            return QString::fromStdString(node.shortHash);
        default:
            break;
        }
    }

    if (role == Qt::BackgroundRole) {
        // The base delegate paints this behind every column, including the graph column.
        if (m_searchActive && m_isMatch[static_cast<std::size_t>(index.row())] != 0) {
            return QBrush(QColor(255, 213, 79, 70));
        }
        return {};
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == GraphColumn::Hash || index.column() == GraphColumn::Date) {
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return {};
}

QVariant CommitTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case GraphColumn::Graph:
            return QStringLiteral("Graph");
        case GraphColumn::Commit:
            return QStringLiteral("Description");
        case GraphColumn::Author:
            return QStringLiteral("Author");
        case GraphColumn::Date:
            return QStringLiteral("Date");
        case GraphColumn::Hash:
            return QStringLiteral("Commit");
        default:
            break;
        }
    }
    return {};
}

void CommitTableModel::setCommits(std::vector<CommitNode> nodes) {
    beginResetModel();
    m_nodes = std::move(nodes);
    m_matchRows.clear();   // the rows are different now; MainWindow searches again if the find bar is open
    m_isMatch.clear();
    m_searchActive = false;
    endResetModel();
}

const CommitNode* CommitTableModel::commitAt(int row) const {
    if (row < 0 || row >= static_cast<int>(m_nodes.size())) {
        return nullptr;
    }
    return &m_nodes[static_cast<size_t>(row)];
}

int CommitTableModel::rowForOid(const std::string &oid) const {
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].oid == oid) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CommitTableModel::notifyBackgroundChanged() {
    if (m_nodes.empty()) {
        return;
    }
    const int lastColumn = static_cast<int>(GraphColumn::ColumnCount) - 1;
    emit dataChanged(index(0, 0), index(static_cast<int>(m_nodes.size()) - 1, lastColumn), {Qt::BackgroundRole});
}

int CommitTableModel::applySearch(const SearchQuery &query) {
    m_matchRows = findMatches(m_nodes, query);
    m_isMatch.assign(m_nodes.size(), 0);
    for (const int row : m_matchRows) {
        m_isMatch[static_cast<std::size_t>(row)] = 1;
    }
    m_searchActive = true;
    notifyBackgroundChanged();
    return static_cast<int>(m_matchRows.size());
}

void CommitTableModel::clearSearch() {
    if (!m_searchActive) {
        return;
    }
    m_matchRows.clear();
    m_isMatch.clear();
    m_searchActive = false;
    notifyBackgroundChanged();
}

void CommitTableModel::clear() {
    beginResetModel();
    m_nodes.clear();
    m_matchRows.clear();
    m_isMatch.clear();
    m_searchActive = false;
    endResetModel();
}

} // namespace Git
