#include "models/CommitTableModel.h"
#include <QDateTime>
#include <QColor>
#include <QStringLiteral>

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
        return QVariant::fromValue(node);
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
    endResetModel();
}

const CommitNode* CommitTableModel::commitAt(int row) const {
    if (row < 0 || row >= static_cast<int>(m_nodes.size())) {
        return nullptr;
    }
    return &m_nodes[static_cast<size_t>(row)];
}

void CommitTableModel::clear() {
    beginResetModel();
    m_nodes.clear();
    endResetModel();
}

} // namespace Git
