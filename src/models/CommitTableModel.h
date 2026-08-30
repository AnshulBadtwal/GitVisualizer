#ifndef COMMITTABLEMODEL_H
#define COMMITTABLEMODEL_H

#include <QAbstractTableModel>
#include <vector>
#include "core/GitTypes.h"

namespace Git {

class CommitTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit CommitTableModel(QObject *parent = nullptr);
    ~CommitTableModel() override = default;

    // QAbstractTableModel interface implementation
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief Atomically updates model data from background worker.
     * @param nodes Fully computed vector of commit nodes.
     */
    void setCommits(std::vector<CommitNode> nodes);

    /**
     * @brief Retrives raw commit reference at specific row.
     */
    [[nodiscard]] const CommitNode* commitAt(int row) const;

    /**
     * @brief Resets model content.
     */
    void clear();

private:
    std::vector<CommitNode> m_nodes;
};

} // namespace Git

#endif // COMMITTABLEMODEL_H
