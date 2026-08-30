#include "ui/CommitGraphView.h"
#include "models/CommitTableModel.h"
#include <QHeaderView>
#include <QAction>
#include <QInputDialog>

namespace Git {

CommitGraphView::CommitGraphView(QWidget *parent)
    : QTableView(parent)
    , m_delegate(new GraphLaneDelegate(this))
{
    setupView();
}

void CommitGraphView::setupView() {
    setItemDelegateForColumn(GraphColumn::Graph, m_delegate);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSortingEnabled(false);

    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(24);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setStretchLastSection(true);

    setShowGrid(false);
    setAlternatingRowColors(true);
}

void CommitGraphView::contextMenuEvent(QContextMenuEvent *event) {
    QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        return;
    }

    QVariant varData = index.data(CustomRoles::CommitRole);
    if (!varData.canConvert<CommitNode>()) {
        return;
    }

    CommitNode node = qvariant_cast<CommitNode>(varData);
    QString oid = QString::fromStdString(node.oid);
    QString shortHash = QString::fromStdString(node.shortHash);

    QMenu contextMenu(this);

    QAction *actCheckout = contextMenu.addAction(QString::fromUtf8("Checkout ") + shortHash);
    QAction *actCreateBranch = contextMenu.addAction(QString::fromUtf8("Create Branch Here..."));
    QAction *actCreateTag = contextMenu.addAction(QString::fromUtf8("Create Tag Here..."));
    contextMenu.addSeparator();

    QAction *actCherryPick = contextMenu.addAction(QString::fromUtf8("Cherry-Pick Commit"));
    QAction *actRevert = contextMenu.addAction(QString::fromUtf8("Revert Commit"));
    contextMenu.addSeparator();

    QMenu *resetMenu = contextMenu.addMenu(QString::fromUtf8("Reset HEAD to here"));
    QAction *actResetSoft = resetMenu->addAction(QString::fromUtf8("Soft (--soft)"));
    QAction *actResetMixed = resetMenu->addAction(QString::fromUtf8("Mixed (--mixed)"));
    QAction *actResetHard = resetMenu->addAction(QString::fromUtf8("Hard (--hard)"));

    QAction *selectedAction = contextMenu.exec(event->globalPos());
    if (!selectedAction) {
        return;
    }

    if (selectedAction == actCheckout) {
        emit checkoutRequested(oid);
    } else if (selectedAction == actCreateBranch) {
        emit createBranchRequested(oid);
    } else if (selectedAction == actCreateTag) {
        emit createTagRequested(oid);
    } else if (selectedAction == actCherryPick) {
        emit cherryPickRequested(oid);
    } else if (selectedAction == actRevert) {
        emit revertRequested(oid);
    } else if (selectedAction == actResetSoft) {
        emit resetRequested(oid, QStringLiteral("--soft"));
    } else if (selectedAction == actResetMixed) {
        emit resetRequested(oid, QStringLiteral("--mixed"));
    } else if (selectedAction == actResetHard) {
        emit resetRequested(oid, QStringLiteral("--hard"));
    }
}

} // namespace Git
