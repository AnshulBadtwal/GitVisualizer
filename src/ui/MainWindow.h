#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <memory>

#include "models/CommitTableModel.h"
#include "ui/CommitGraphView.h"
#include "ui/DiffViewerWidget.h"
#include "ui/TerminalDockWidget.h"
#include "workers/LogWorker.h"
#include "workers/CommandRunner.h"

namespace Git {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void openRepository(const QString &path);
    void refreshRepository();

private slots:
    void handleLogReady(const std::vector<Git::CommitNode> &nodes);
    void handleWorkerError(const QString &errorMsg);
    void handleCommandFinished(const QString &output);
    void handleCommandFailed(int exitCode, const QString &errorMsg);
    void handleGitDirectoryChanged(const QString &path);
    void handleCommitSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

    void triggerCheckout(const QString &target);
    void triggerCreateBranch(const QString &commitOid);
    void triggerCreateTag(const QString &commitOid);
    void triggerReset(const QString &commitOid, const QString &mode);
    void triggerCherryPick(const QString &commitOid);
    void triggerRevert(const QString &commitOid);

signals:
    void requestLogProcessing(const QString &path, std::size_t maxCommits);

private:
    QString m_currentRepoPath;

    // Core Components & Models
    CommitTableModel *m_model{nullptr};
    CommitGraphView *m_graphView{nullptr};
    DiffViewerWidget *m_diffViewer{nullptr};
    TerminalDockWidget *m_terminalDock{nullptr};
    QSplitter *m_mainSplitter{nullptr};

    // Workers & Threading
    QThread m_workerThread;
    LogWorker *m_logWorker{nullptr};
    CommandRunner *m_commandRunner{nullptr};

    // Auto-Refresh Watcher
    QFileSystemWatcher m_gitWatcher;

    // Status UI
    QLabel *m_statusLabel{nullptr};
    QProgressBar *m_progressBar{nullptr};

    void setupUi();
    void setupWorkerThread();
    void watchGitFolder(const QString &repoPath);
};

} // namespace Git

#endif // MAINWINDOW_H
