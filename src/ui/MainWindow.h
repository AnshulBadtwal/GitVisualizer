#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QMainWindow>
#include <QThread>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTimer>
#include <memory>
#include <optional>

#include "models/CommitTableModel.h"
#include "ui/CommitGraphView.h"
#include "ui/CommitDetailsWidget.h"
#include "ui/DiffViewerWidget.h"
#include "ui/SearchBar.h"
#include "core/NavigationHistory.h"
#include "ui/TerminalDockWidget.h"
#include "workers/DetailsWorker.h"
#include "workers/LogWorker.h"
#include "workers/CommandRunner.h"
#include "core/GitWatch.h"

namespace Git {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void openRepository(const QString &path);

    /// Reloads the history now, whether or not anything changed (File > Refresh).
    void refreshRepository();

    /// Reloads soon, but only if Git's refs really changed. Many calls in a short time cause one check.
    void scheduleRefresh();

private slots:
    void handleLogReady(const std::vector<Git::CommitNode> &nodes);
    void handleWorkerError(const QString &errorMsg);
    void handleDetailsReady(quint64 requestId, const Git::CommitDetails &details);
    void selectCommit(const QString &oid);

    // Search (Edit menu, Ctrl+F)
    void showSearchBar();
    void hideSearchBar();
    void handleSearchChanged();
    void findNext();
    void findPrevious();

    // Back / forward through the commits looked at (Go menu, Alt+Left / Alt+Right)
    void goBack();
    void goForward();
    void handleCommandFinished(const QString &output);
    void handleCommandFailed(int exitCode, const QString &errorMsg);
    void handleGitDirectoryChanged(const QString &path);
    void checkForExternalChanges();
    void handleCommitSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

    void triggerCheckout(const QString &target);
    void triggerCreateBranch(const QString &commitOid);
    void triggerCreateTag(const QString &commitOid);
    void triggerReset(const QString &commitOid, const QString &mode);
    void triggerCherryPick(const QString &commitOid);
    void triggerRevert(const QString &commitOid);

signals:
    void requestLogProcessing(const QString &path, std::size_t maxCommits);
    void requestCommitDetails(quint64 requestId, const QString &repoPath, const QString &oid);

private:
    QString m_currentRepoPath;

    // Core Components & Models
    CommitTableModel *m_model{nullptr};
    CommitGraphView *m_graphView{nullptr};
    SearchBar *m_searchBar{nullptr};
    DiffViewerWidget *m_diffViewer{nullptr};
    CommitDetailsWidget *m_detailsPanel{nullptr};
    TerminalDockWidget *m_terminalDock{nullptr};
    QSplitter *m_mainSplitter{nullptr};
    QSplitter *m_bottomSplitter{nullptr};   // details panel | diff viewer

    // Workers & Threading
    QThread m_workerThread;
    LogWorker *m_logWorker{nullptr};
    QThread m_detailsThread;               // commit details are read here, apart from the history
    DetailsWorker *m_detailsWorker{nullptr};
    CommandRunner *m_commandRunner{nullptr};

    // Auto-Refresh: folder watcher -> short quiet period -> compare refs -> reload only if they changed
    static constexpr int RefreshDelayMs = 300;
    QFileSystemWatcher m_gitWatcher;
    QTimer m_refreshTimer;
    GitDirs m_gitDirs;
    std::optional<std::size_t> m_lastFingerprint;   // refs as they were when the last load started
    bool m_loading{false};
    bool m_reloadQueued{false};

    // Commit details: only the answer to the newest request is shown.
    quint64 m_detailsRequestId{0};
    QString m_shownOid;   // the commit whose details are on screen or on their way

    // Back / forward: two CustomStack stacks inside NavigationHistory
    NavigationHistory m_history;
    bool m_navigating{false};   // true while Back/Forward/search move the selection: not recorded
    QAction *m_actBack{nullptr};
    QAction *m_actForward{nullptr};

    // Status UI
    QLabel *m_statusLabel{nullptr};
    QProgressBar *m_progressBar{nullptr};

    void setupUi();
    void setupWorkerThread();
    void setupDetailsThread();
    void clearCommitView();
    void selectRowAndReveal(int row);
    int selectedRow() const;   // -1 when nothing is selected
    void refreshSearch(bool jumpToMatch);
    void updateSearchStatus();
    void navigateToRow(int row);
    void updateNavigationActions();
    void rebuildWatcher();
    void reloadHistory();
    void finishLoading();
};

} // namespace Git

#endif // MAINWINDOW_H
