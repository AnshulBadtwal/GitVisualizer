#include "ui/MainWindow.h"
#include <QFileDialog>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QHeaderView>
#include <QStringLiteral>

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
    connect(&m_gitWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::handleGitDirectoryChanged);
    connect(&m_gitWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::handleGitDirectoryChanged);
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

    connect(m_terminalDock, &TerminalDockWidget::commandExecuted, this, &MainWindow::refreshRepository);

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

    connect(this, &MainWindow::requestLogProcessing, m_logWorker, &LogWorker::processLog);
    connect(m_logWorker, &LogWorker::logReady, this, &MainWindow::handleLogReady);
    connect(m_logWorker, &LogWorker::errorOccurred, this, &MainWindow::handleWorkerError);

    m_workerThread.start();
}

void MainWindow::openRepository(const QString &path) {
    m_currentRepoPath = path;
    m_diffViewer->clear();
    m_terminalDock->setWorkingDirectory(path);
    watchGitFolder(path);
    refreshRepository();
}

void MainWindow::refreshRepository() {
    if (m_currentRepoPath.isEmpty()) {
        return;
    }

    m_statusLabel->setText(QStringLiteral("Reading Git log..."));
    m_progressBar->show();
    emit requestLogProcessing(m_currentRepoPath, 0);
}

void MainWindow::handleLogReady(const std::vector<Git::CommitNode> &nodes) {
    m_model->setCommits(nodes);
    
    // Auto-adjust graph column widths
    m_graphView->resizeColumnToContents(GraphColumn::Graph);
    m_graphView->horizontalHeader()->setSectionResizeMode(GraphColumn::Graph, QHeaderView::Interactive);
    m_graphView->horizontalHeader()->setSectionResizeMode(GraphColumn::Commit, QHeaderView::Stretch);

    m_progressBar->hide();
    m_statusLabel->setText(QStringLiteral("Loaded ") + QString::number(nodes.size()) + QStringLiteral(" commits cleanly."));
}

void MainWindow::handleWorkerError(const QString &errorMsg) {
    m_progressBar->hide();
    m_statusLabel->setText(QStringLiteral("Error opening repository."));
    QMessageBox::critical(this, QStringLiteral("Git Error"), errorMsg);
}

void MainWindow::watchGitFolder(const QString &repoPath) {
    if (!m_gitWatcher.directories().isEmpty()) {
        m_gitWatcher.removePaths(m_gitWatcher.directories());
    }
    if (!m_gitWatcher.files().isEmpty()) {
        m_gitWatcher.removePaths(m_gitWatcher.files());
    }

    QString gitDir = repoPath + QStringLiteral("/.git");
    if (QDir(gitDir).exists()) {
        m_gitWatcher.addPath(gitDir);
        m_gitWatcher.addPath(gitDir + QStringLiteral("/HEAD"));
        m_gitWatcher.addPath(gitDir + QStringLiteral("/refs/heads"));
    }
}

void MainWindow::handleGitDirectoryChanged(const QString &) {
    refreshRepository();
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
    m_statusLabel->setText(QStringLiteral("Operation successful."));
    if (!output.trimmed().isEmpty()) {
        statusBar()->showMessage(output.trimmed(), 4000);
    }
    refreshRepository();
}

void MainWindow::handleCommandFailed(int exitCode, const QString &errorMsg) {
    m_statusLabel->setText(QStringLiteral("Command failed (exit code ") + QString::number(exitCode) + QStringLiteral(")"));
    QMessageBox::critical(this, QStringLiteral("Git Action Failed"), errorMsg);
}

} // namespace Git
