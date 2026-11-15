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
#include <QStyle>
#include <QVBoxLayout>
#include "core/CommitSearch.h"

namespace Git {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_model(new CommitTableModel(this))
    , m_graphView(new CommitGraphView(this))
    , m_searchBar(new SearchBar(this))
    , m_diffViewer(new DiffViewerWidget(this))
    , m_detailsPanel(new CommitDetailsWidget(this))
    , m_logWorker(new LogWorker())
    , m_detailsWorker(new DetailsWorker())
    , m_commandRunner(new CommandRunner(this))
{
    setupUi();
    setupWorkerThread();
    setupDetailsThread();

    // Context menu actions from CommitGraphView
    connect(m_graphView, &CommitGraphView::checkoutRequested, this, &MainWindow::triggerCheckout);
    connect(m_graphView, &CommitGraphView::createBranchRequested, this, &MainWindow::triggerCreateBranch);
    connect(m_graphView, &CommitGraphView::createTagRequested, this, &MainWindow::triggerCreateTag);
    connect(m_graphView, &CommitGraphView::resetRequested, this, &MainWindow::triggerReset);
    connect(m_graphView, &CommitGraphView::cherryPickRequested, this, &MainWindow::triggerCherryPick);
    connect(m_graphView, &CommitGraphView::revertRequested, this, &MainWindow::triggerRevert);

    // Details panel: a parent link selects that commit, a file in the list scrolls the diff to it
    connect(m_detailsPanel, &CommitDetailsWidget::parentActivated, this, &MainWindow::selectCommit);
    connect(m_detailsPanel, &CommitDetailsWidget::fileActivated, m_diffViewer, &DiffViewerWidget::scrollToFile);

    // Find bar
    connect(m_searchBar, &SearchBar::queryChanged, this, &MainWindow::handleSearchChanged);
    connect(m_searchBar, &SearchBar::nextRequested, this, &MainWindow::findNext);
    connect(m_searchBar, &SearchBar::previousRequested, this, &MainWindow::findPrevious);
    connect(m_searchBar, &SearchBar::closeRequested, this, &MainWindow::hideSearchBar);

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
    m_detailsThread.quit();
    m_workerThread.wait();
    m_detailsThread.wait();
}

void MainWindow::setupUi() {
    resize(1280, 800);
    setWindowTitle(QStringLiteral("Git Visualizer"));

    m_graphView->setModel(m_model);

    // Main Vertical Splitter: Top = Commit Graph, Bottom = Diff Viewer
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    // Top half: the find bar (hidden until Ctrl+F) above the commit graph
    auto *graphPane = new QWidget(this);
    auto *graphLayout = new QVBoxLayout(graphPane);
    graphLayout->setContentsMargins(0, 0, 0, 0);
    graphLayout->setSpacing(0);
    graphLayout->addWidget(m_searchBar);
    graphLayout->addWidget(m_graphView, 1);
    m_searchBar->hide();
    m_mainSplitter->addWidget(graphPane);
    // Bottom half: details panel on the left, diff on the right
    m_bottomSplitter = new QSplitter(Qt::Horizontal, this);
    m_bottomSplitter->addWidget(m_detailsPanel);
    m_bottomSplitter->addWidget(m_diffViewer);
    m_bottomSplitter->setStretchFactor(0, 2);
    m_bottomSplitter->setStretchFactor(1, 3);
    m_mainSplitter->addWidget(m_bottomSplitter);
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

    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    QAction *actFind = editMenu->addAction(QStringLiteral("&Find..."));
    actFind->setShortcut(QKeySequence::Find);
    connect(actFind, &QAction::triggered, this, &MainWindow::showSearchBar);
    QAction *actFindNext = editMenu->addAction(QStringLiteral("Find &Next"));
    actFindNext->setShortcut(QKeySequence::FindNext);
    connect(actFindNext, &QAction::triggered, this, &MainWindow::findNext);
    QAction *actFindPrevious = editMenu->addAction(QStringLiteral("Find &Previous"));
    actFindPrevious->setShortcut(QKeySequence::FindPrevious);
    connect(actFindPrevious, &QAction::triggered, this, &MainWindow::findPrevious);

    QMenu *goMenu = menuBar()->addMenu(QStringLiteral("&Go"));
    m_actBack = goMenu->addAction(style()->standardIcon(QStyle::SP_ArrowBack), QStringLiteral("&Back"));
    m_actBack->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    m_actBack->setToolTip(QStringLiteral("Back to the previous commit (Alt+Left)"));
    m_actBack->setEnabled(false);
    connect(m_actBack, &QAction::triggered, this, &MainWindow::goBack);
    m_actForward = goMenu->addAction(style()->standardIcon(QStyle::SP_ArrowForward), QStringLiteral("&Forward"));
    m_actForward->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    m_actForward->setToolTip(QStringLiteral("Forward to the next commit (Alt+Right)"));
    m_actForward->setEnabled(false);
    connect(m_actForward, &QAction::triggered, this, &MainWindow::goForward);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(m_terminalDock->toggleViewAction());

    // Tool Bar
    QToolBar *toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->addAction(actOpen);
    toolBar->addAction(actRefresh);
    toolBar->addSeparator();
    toolBar->addAction(m_actBack);
    toolBar->addAction(m_actForward);

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

void MainWindow::setupDetailsThread() {
    m_detailsWorker->moveToThread(&m_detailsThread);
    // Same pattern as the log worker: it has no parent, so it is deleted when its thread ends.
    connect(&m_detailsThread, &QThread::finished, m_detailsWorker, &QObject::deleteLater);

    connect(this, &MainWindow::requestCommitDetails, m_detailsWorker, &DetailsWorker::loadDetails);
    connect(m_detailsWorker, &DetailsWorker::detailsReady, this, &MainWindow::handleDetailsReady);

    m_detailsThread.start();
}

void MainWindow::clearCommitView() {
    // A new id makes the worker skip, and handleDetailsReady ignore, anything still on its way.
    ++m_detailsRequestId;
    m_detailsWorker->setLatestRequest(m_detailsRequestId);
    m_shownOid.clear();
    m_detailsPanel->clear();
    m_diffViewer->clear();
}

void MainWindow::handleDetailsReady(quint64 requestId, const Git::CommitDetails &details) {
    if (requestId != m_detailsRequestId) {
        return;   // the answer to an older selection
    }
    if (!details.ok()) {
        m_shownOid.clear();   // selecting the commit again tries again
    }
    m_detailsPanel->setDetails(details);
    m_diffViewer->setDetails(details);
}

void MainWindow::selectCommit(const QString &oid) {
    const int row = m_model->rowForOid(oid.toStdString());
    if (row < 0) {
        m_statusLabel->setText(QStringLiteral("That commit is not in the loaded history."));
        return;
    }
    selectRowAndReveal(row);
}

void MainWindow::selectRowAndReveal(int row) {
    m_graphView->selectRow(row);
    m_graphView->scrollTo(m_model->index(row, 0), QAbstractItemView::PositionAtCenter);
}

int MainWindow::selectedRow() const {
    const QModelIndexList rows = m_graphView->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void MainWindow::goBack() {
    // A commit that a reload removed is skipped, not shown as an error.
    const auto oid = m_history.back([this](const std::string &id) { return m_model->rowForOid(id) >= 0; });
    updateNavigationActions();
    if (oid) {
        navigateToRow(m_model->rowForOid(*oid));
    }
}

void MainWindow::goForward() {
    const auto oid = m_history.forward([this](const std::string &id) { return m_model->rowForOid(id) >= 0; });
    updateNavigationActions();
    if (oid) {
        navigateToRow(m_model->rowForOid(*oid));
    }
}

void MainWindow::navigateToRow(int row) {
    if (row < 0) {
        return;
    }
    m_navigating = true;   // moving through the history must not add to it
    selectRowAndReveal(row);
    m_navigating = false;
}

void MainWindow::updateNavigationActions() {
    m_actBack->setEnabled(m_history.canGoBack());
    m_actForward->setEnabled(m_history.canGoForward());
}

void MainWindow::showSearchBar() {
    m_searchBar->show();
    m_searchBar->focusInput();
    refreshSearch(false);   // the bar may still hold a query from before; highlight it again
}

void MainWindow::hideSearchBar() {
    m_searchBar->hide();
    m_model->clearSearch();
    m_graphView->setFocus();
}

void MainWindow::handleSearchChanged() {
    refreshSearch(true);
}

void MainWindow::findNext() {
    if (m_searchBar->isHidden() || !m_model->hasSearch()) {
        showSearchBar();
        return;
    }
    const int row = stepMatch(m_model->matchRows(), selectedRow(), true);
    if (row >= 0) {
        selectRowAndReveal(row);
    }
}

void MainWindow::findPrevious() {
    if (m_searchBar->isHidden() || !m_model->hasSearch()) {
        showSearchBar();
        return;
    }
    const int row = stepMatch(m_model->matchRows(), selectedRow(), false);
    if (row >= 0) {
        selectRowAndReveal(row);
    }
}

void MainWindow::refreshSearch(bool jumpToMatch) {
    const SearchQuery query = parseSearchQuery(m_searchBar->text().toStdString());
    if (query.empty()) {
        m_model->clearSearch();
        updateSearchStatus();
        return;
    }

    m_model->applySearch(query);

    if (jumpToMatch) {
        // Refining a query should stay near where the person is looking: go to the first match
        // at or after the selected row (wrapping to the first match if there is none after it).
        const int current = selectedRow();
        const int row = stepMatch(m_model->matchRows(), current > 0 ? current - 1 : -1, true);
        if (row >= 0) {
            navigateToRow(row);   // refining a query is not a visit, so it stays out of the history
        }
    }
    updateSearchStatus();
}

void MainWindow::updateSearchStatus() {
    if (!m_model->hasSearch()) {
        m_searchBar->setStatus(QString());
        m_searchBar->setNoMatch(false);
        return;
    }

    const std::vector<int> &rows = m_model->matchRows();
    if (rows.empty()) {
        m_searchBar->setStatus(QStringLiteral("No matches"));
        m_searchBar->setNoMatch(true);
        return;
    }

    m_searchBar->setNoMatch(false);
    const int position = matchPosition(rows, selectedRow());
    const int total = static_cast<int>(rows.size());
    m_searchBar->setStatus(position > 0 ? QStringLiteral("%1 of %2").arg(position).arg(total)
                                        : QStringLiteral("%1 matches").arg(total));
}

void MainWindow::openRepository(const QString &path) {
    m_currentRepoPath = path;
    m_lastFingerprint.reset();
    m_refreshTimer.stop();
    clearCommitView();
    m_history.clear();
    updateNavigationActions();
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
            clearCommitView();   // that commit is no longer in the history
        }
    }

    if (!m_searchBar->isHidden()) {
        refreshSearch(false);
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
    updateSearchStatus();   // "3 of 17" follows the selected row

    if (selected.indexes().isEmpty()) {
        clearCommitView();
        return;
    }

    const CommitNode *node = m_model->commitAt(selected.indexes().first().row());
    if (!node) {
        clearCommitView();
        return;
    }

    // Instant: what the graph row already knows. After a reload this also refreshes the
    // branch and tag names of the commit that stays selected.
    m_detailsPanel->setCommit(*node);

    if (!m_navigating) {
        m_history.visit(node->oid);
        updateNavigationActions();
    }

    const QString oid = QString::fromStdString(node->oid);
    if (oid == m_shownOid) {
        return;   // the same commit again (for example after an automatic refresh): keep what is shown
    }
    m_shownOid = oid;

    // The rest (full message, changed files, diff) is read on the details thread.
    m_diffViewer->showLoading(oid.left(7));
    ++m_detailsRequestId;
    m_detailsWorker->setLatestRequest(m_detailsRequestId);
    emit requestCommitDetails(m_detailsRequestId, m_currentRepoPath, oid);
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
