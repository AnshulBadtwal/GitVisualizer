#include "ui/MainWindow.h"
#include <QFileDialog>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QHeaderView>
#include <QScrollBar>
#include <QString>

namespace Git {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_model(new CommitTableModel(this))
    , m_graphView(new CommitGraphView(this))
    , m_diffViewer(new DiffViewerWidget(this))
    , m_logWorker(new LogWorker())
    , m_commandRunner(new CommandRunner(this))
{
    setupUi();
    setupWorkerThread();

    // Context menu actions from CommitGraphView
    connect(m_graphView, &CommitGraphView::checkoutRequested, this, &MainWindow::triggerCheckout);
    connect(m_graphView, &CommitGraphView::createBranchRequested, this, &MainWindow::triggerCreateBranch);
    connect(m_graphView, &CommitGraphView::createTagRequested, this, &MainWindow::triggerCreateTag);
    connect(m_graphView, &CommitGraphView::resetRequested, this, &MainWindow::triggerReset);
    connect(m_graphView, &CommitGraphView::cherryPickRequested, this, &MainWindow::triggerCherryPick);
    connect(m_graphView, &CommitGraphView::revertRequested, this, &MainWindow::triggerRevert);

    // Dynamic diff updates on table selection change
    connect(m_graphView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::handleCommitSelectionChanged);

    // CLI Runner signals
    connect(m_commandRunner, &CommandRunner::commandFinished, this, &MainWindow::handleCommandFinished);
    connect(m_commandRunner, &CommandRunner::commandFailed, this, &MainWindow::handleCommandFailed);

    // File System Watcher signals
    // Only folders are watched (Git replaces files, which would drop a file watch), so there is no fileChanged.
    connect(&m_gitWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::handleGitDirectoryChanged);

    // Many events in a row (one git command touches several files) become a single check.
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(RefreshDelayMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &MainWindow::checkForExternalChanges);
}

MainWindow::~MainWindow() {
    m_workerThread.quit();
    m_workerThread.wait();
}

void MainWindow::setupUi() {
    resize(1280, 800);
    setWindowTitle(QStringLiteral("Git Visualizer"));

    m_graphView->setModel(m_model);

    // Main Vertical Splitter: Top = Commit Graph, Bottom = Diff Viewer
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_mainSplitter->addWidget(m_graphView);
    m_mainSplitter->addWidget(m_diffViewer);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 2);

    setCentralWidget(m_mainSplitter);

    // Embedded Terminal Dock Panel
    m_terminalDock = new TerminalDockWidget(QStringLiteral("Interactive Terminal"), this);
    m_terminalDock->setObjectName(QStringLiteral("TerminalDockWidget"));
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);

    connect(m_terminalDock, &TerminalDockWidget::commandExecuted, this, &MainWindow::scheduleRefresh);

    // Menu Bar
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction *actOpen = fileMenu->addAction(QStringLiteral("&Open Repository..."));
    actOpen->setShortcut(QKeySequence::Open);
    connect(actOpen, &QAction::triggered, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Open Git Repository"));
        if (!dir.isEmpty()) {
            openRepository(dir);
        }
    });

    QAction *actRefresh = fileMenu->addAction(QStringLiteral("&Refresh"));
    actRefresh->setShortcut(QKeySequence::Refresh);
    connect(actRefresh, &QAction::triggered, this, &MainWindow::refreshRepository);

    fileMenu->addSeparator();
    QAction *actExit = fileMenu->addAction(QStringLiteral("E&xit"));
    connect(actExit, &QAction::triggered, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(m_terminalDock->toggleViewAction());

    // Tool Bar
    QToolBar *toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->addAction(actOpen);
    toolBar->addAction(actRefresh);

    // Status Bar
    m_statusLabel = new QLabel(QStringLiteral("Ready"), this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // Indeterminate loading animation
    m_progressBar->hide();

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::setupWorkerThread() {
    m_logWorker->moveToThread(&m_workerThread);
    // The worker has no parent, so delete it when its thread ends (it is destroyed in that thread).
    connect(&m_workerThread, &QThread::finished, m_logWorker, &QObject::deleteLater);

    connect(this, &MainWindow::requestLogProcessing, m_logWorker, &LogWorker::processLog);
    connect(m_logWorker, &LogWorker::logReady, this, &MainWindow::handleLogReady);
    connect(m_logWorker, &LogWorker::errorOccurred, this, &MainWindow::handleWorkerError);

    m_workerThread.start();
}

void MainWindow::openRepository(const QString &path) {
    m_currentRepoPath = path;
    m_lastFingerprint.reset();
    m_refreshTimer.stop();
    m_diffViewer->clear();
    m_terminalDock->setWorkingDirectory(path);
    m_gitDirs = findGitDirs(path.toStdString());
    rebuildWatcher();
    refreshRepository();
}

void MainWindow::refreshRepository() {
    if (m_currentRepoPath.isEmpty()) {
        return;
    }
    m_refreshTimer.stop();   // an automatic check that was waiting is no longer needed
    reloadHistory();
}

void MainWindow::scheduleRefresh() {
    if (m_currentRepoPath.isEmpty()) {
        return;
    }
    m_refreshTimer.start();   // restarts the quiet period, so a burst of events waits for its end
}

void MainWindow::checkForExternalChanges() {
    if (m_currentRepoPath.isEmpty()) {
        return;
    }

    rebuildWatcher();   // a new branch folder may have appeared, an old one may be gone

    // Many events are noise: index.lock appears and disappears even for `git status`.
    // Reload only if HEAD or a ref is different from what is on screen.
    if (m_gitDirs.valid() && m_lastFingerprint && *m_lastFingerprint == refsFingerprint(m_gitDirs)) {
        return;
    }
    reloadHistory();
}

void MainWindow::reloadHistory() {
    if (m_loading) {
        m_reloadQueued = true;   // run once more as soon as the current load is done
        return;
    }
    m_loading = true;

    // Remember the refs we are about to read. If they change while loading, the next check notices.
    m_lastFingerprint = refsFingerprint(m_gitDirs);

    m_statusLabel->setText(QStringLiteral("Reading Git log..."));
    m_progressBar->show();
    emit requestLogProcessing(m_currentRepoPath, 0);
}

void MainWindow::finishLoading() {
    m_loading = false;
    if (m_reloadQueued) {
        m_reloadQueued = false;
        reloadHistory();
    }
}

void MainWindow::handleLogReady(const std::vector<Git::CommitNode> &nodes) {
    // A reload replaces the whole model. Keep what the person was looking at.
    QString selectedOid;
    const QModelIndexList selectedRows = m_graphView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        selectedOid = selectedRows.first().data(CustomRoles::OidRole).toString();
    }
    const int scrollValue = m_graphView->verticalScrollBar()->value();

    m_model->setCommits(nodes);

    // Auto-adjust graph column widths
    m_graphView->resizeColumnToContents(GraphColumn::Graph);
    m_graphView->horizontalHeader()->setSectionResizeMode(GraphColumn::Graph, QHeaderView::Interactive);
    m_graphView->horizontalHeader()->setSectionResizeMode(GraphColumn::Commit, QHeaderView::Stretch);

    // Lay out now so the scroll bar knows the new size before it is restored.
    m_graphView->doItemsLayout();
    m_graphView->verticalScrollBar()->setValue(scrollValue);
    if (!selectedOid.isEmpty()) {
        const int row = m_model->rowForOid(selectedOid.toStdString());
        if (row >= 0) {
            m_graphView->selectRow(row);
        } else {
            m_diffViewer->clear();   // that commit is no longer in the history
        }
    }

    m_progressBar->hide();
    m_statusLabel->setText(QStringLiteral("Loaded ") + QString::number(nodes.size()) + QStringLiteral(" commits cleanly."));
    finishLoading();
}

void MainWindow::handleWorkerError(const QString &errorMsg) {
    m_lastFingerprint.reset();   // so the next change is tried again
    m_progressBar->hide();
    m_statusLabel->setText(QStringLiteral("Could not read the repository."));
    QMessageBox::critical(this, QStringLiteral("Git Error"), errorMsg);
    finishLoading();
}

void MainWindow::rebuildWatcher() {
    QStringList wanted;
    for (const std::string &dir : watchedDirectories(m_gitDirs)) {
        wanted << QString::fromStdString(dir);
    }

    // Files are not watched: Git replaces HEAD and the refs instead of editing them,
    // and the folder that holds them reports that.
    if (!m_gitWatcher.files().isEmpty()) {
        m_gitWatcher.removePaths(m_gitWatcher.files());
    }

    const QStringList current = m_gitWatcher.directories();

    QStringList stale;
    for (const QString &dir : current) {
        if (!wanted.contains(dir)) {
            stale << dir;
        }
    }
    if (!stale.isEmpty()) {
        m_gitWatcher.removePaths(stale);
    }

    QStringList fresh;
    for (const QString &dir : wanted) {
        if (!current.contains(dir)) {
            fresh << dir;
        }
    }
    if (!fresh.isEmpty()) {
        m_gitWatcher.addPaths(fresh);
    }
}

void MainWindow::handleGitDirectoryChanged(const QString &) {
    scheduleRefresh();
}

void MainWindow::handleCommitSelectionChanged(const QItemSelection &selected, const QItemSelection &) {
    if (selected.indexes().isEmpty()) {
        m_diffViewer->clear();
        return;
    }

    QModelIndex index = selected.indexes().first();
    const CommitNode *node = m_model->commitAt(index.row());
    if (node) {
        m_diffViewer->loadDiffForCommit(m_currentRepoPath, QString::fromStdString(node->oid));
    }
}

void MainWindow::triggerCheckout(const QString &target) {
    m_statusLabel->setText(QStringLiteral("Checking out ") + target + QStringLiteral("..."));
    m_commandRunner->runCommand(m_currentRepoPath, {QStringLiteral("checkout"), target});
}

void MainWindow::triggerCreateBranch(const QString &commitOid) {
    bool ok = false;
    QString branchName = QInputDialog::getText(this, QStringLiteral("New Branch"),
                                                QStringLiteral("Branch Name:"),
                                                QLineEdit::Normal, QString(), &ok);
    if (ok && !branchName.trimmed().isEmpty()) {
        m_commandRunner->runCommand(m_currentRepoPath, {QStringLiteral("branch"), branchName.trimmed(), commitOid});
    }
}

void MainWindow::triggerCreateTag(const QString &commitOid) {
    bool ok = false;
    QString tagName = QInputDialog::getText(this, QStringLiteral("New Tag"),
                                             QStringLiteral("Tag Name:"),
                                             QLineEdit::Normal, QString(), &ok);
    if (ok && !tagName.trimmed().isEmpty()) {
        m_commandRunner->runCommand(m_currentRepoPath, {QStringLiteral("tag"), tagName.trimmed(), commitOid});
    }
}

void MainWindow::triggerReset(const QString &commitOid, const QString &mode) {
    auto res = QMessageBox::warning(this, QStringLiteral("Confirm Reset"),
                                    QStringLiteral("Are you sure you want to reset HEAD to ") + commitOid.left(7) + QStringLiteral("?"),
                                    QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        m_commandRunner->runCommand(m_currentRepoPath, {QStringLiteral("reset"), mode, commitOid});
    }
}

void MainWindow::triggerCherryPick(const QString &commitOid) {
    m_commandRunner->runCommand(m_currentRepoPath, {QStringLiteral("cherry-pick"), commitOid});
}

void MainWindow::triggerRevert(const QString &commitOid) {
    m_commandRunner->runCommand(m_currentRepoPath, {QStringLiteral("revert"), QStringLiteral("--no-edit"), commitOid});
}

void MainWindow::handleCommandFinished(const QString &output) {
    const QString commandLine = m_commandRunner->lastCommandLine();

    m_terminalDock->appendLog(QStringLiteral("$ ") + commandLine, false);
    if (!output.trimmed().isEmpty()) {
        m_terminalDock->appendLog(output.trimmed(), false);
    }

    m_statusLabel->setText(QStringLiteral("Done: ") + commandLine);
    scheduleRefresh();
}

void MainWindow::handleCommandFailed(int exitCode, const QString &errorMsg) {
    const QString commandLine = m_commandRunner->lastCommandLine();

    m_terminalDock->appendLog(QStringLiteral("$ ") + commandLine, false);
    m_terminalDock->appendLog(errorMsg.trimmed(), true);
    m_terminalDock->show();   // make sure the log can be read

    m_statusLabel->setText(QStringLiteral("Command failed (exit code ") + QString::number(exitCode)
                           + QStringLiteral("): ") + commandLine);
    QMessageBox::critical(this, QStringLiteral("Git Action Failed"), commandLine + QStringLiteral("\n\n") + errorMsg);
    scheduleRefresh();
}

} // namespace Git
