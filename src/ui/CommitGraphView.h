#ifndef COMMITGRAPHVIEW_H
#define COMMITGRAPHVIEW_H

#include <QTableView>
#include <QContextMenuEvent>
#include <QMenu>
#include "core/GitTypes.h"
#include "delegates/GraphLaneDelegate.h"

namespace Git {

class CommitGraphView : public QTableView {
    Q_OBJECT

public:
    explicit CommitGraphView(QWidget *parent = nullptr);
    ~CommitGraphView() override = default;

signals:
    void checkoutRequested(const QString &target);
    void createBranchRequested(const QString &commitOid);
    void createTagRequested(const QString &commitOid);
    void resetRequested(const QString &commitOid, const QString &mode);
    void cherryPickRequested(const QString &commitOid);
    void revertRequested(const QString &commitOid);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    GraphLaneDelegate *m_delegate{nullptr};
    void setupView();
};

} // namespace Git

#endif // COMMITGRAPHVIEW_H
