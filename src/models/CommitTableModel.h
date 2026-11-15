#ifndef COMMITTABLEMODEL_H
#define COMMITTABLEMODEL_H

#include <QAbstractTableModel>
#include <vector>
#include "core/CommitSearch.h"
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
     * @brief Row of the commit with this id, or -1 if it is not in the model.
     */
    [[nodiscard]] int rowForOid(const std::string &oid) const;

    /**
     * @brief Resets model content.
     */
    void clear();

    /**
     * @brief Marks every commit that matches the query (they get a highlighted background).
     * @return The number of matching commits.
     */
    int applySearch(const SearchQuery &query);

    /**
     * @brief Removes the highlighting. Does nothing if no search is active.
     */
    void clearSearch();

    /// True between applySearch() and clearSearch() (or the next setCommits/clear).
    [[nodiscard]] bool hasSearch() const { return m_searchActive; }

    /// Row indexes of the matching commits, ascending. Empty if no search is active.
    [[nodiscard]] const std::vector<int> &matchRows() const { return m_matchRows; }

private:
    void notifyBackgroundChanged();

    std::vector<CommitNode> m_nodes;

    // Search state. m_isMatch has one flag per row while a search is active, so data() can
    // answer "is this row a match?" in constant time while painting.
    std::vector<int> m_matchRows;
    std::vector<char> m_isMatch;
    bool m_searchActive{false};
};

} // namespace Git

#endif // COMMITTABLEMODEL_H
