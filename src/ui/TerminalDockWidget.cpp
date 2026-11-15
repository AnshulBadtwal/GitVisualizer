#include "ui/TerminalDockWidget.h"
#include "core/TerminalCommand.h"
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Git {

namespace {

constexpr int KillGraceMs = 2000;      // how long a command may take to obey Stop before it is forced
constexpr int MaxOutputLines = 5000;   // the panel forgets the oldest lines beyond this

// bash if it is installed, otherwise the POSIX shell that every Linux has.
QString shellProgram() {
    const QString bash = QStandardPaths::findExecutable(QStringLiteral("bash"));
    return bash.isEmpty() ? QStringLiteral("/bin/sh") : bash;
}

// Ends the shell and everything it started. Each command's shell leads its own process group
// (see runInShell), so signalling the group reaches the children too, not only the shell.
void endProcess(QProcess &process, bool force) {
#ifdef Q_OS_UNIX
    const qint64 pid = process.processId();
    if (pid > 0) {
        ::kill(static_cast<pid_t>(-pid), force ? SIGKILL : SIGTERM);
        return;
    }
#endif
    if (force) {
        process.kill();
    } else {
        process.terminate();
    }
}

} // namespace

TerminalDockWidget::TerminalDockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
    , m_workingDir(QDir::currentPath())
{
    setupUi();

    m_killTimer.setSingleShot(true);
    connect(&m_killTimer, &QTimer::timeout, this, &TerminalDockWidget::forceKill);

    connect(&m_process, &QProcess::readyReadStandardOutput, this, &TerminalDockWidget::handleReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, &TerminalDockWidget::handleReadyReadStandardError);
    connect(&m_process, &QProcess::finished, this, &TerminalDockWidget::handleProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &TerminalDockWidget::handleProcessError);
}

TerminalDockWidget::~TerminalDockWidget() {
    // No handler may run from here on: the window that owns this panel is already being taken apart,
    // and a "command finished" signal would reach it too late.
    m_process.disconnect(this);
    m_killTimer.stop();

    if (m_process.state() != QProcess::NotRunning) {
        endProcess(m_process, true);
        m_process.waitForFinished(1000);
    }
}

void TerminalDockWidget::setupUi() {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);

    // The folder the next command runs in
    m_cwdLabel = new QLabel(container);
    m_cwdLabel->setFont(font);
    m_cwdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // Terminal Output Display
    m_outputView = new QTextEdit(container);
    m_outputView->setReadOnly(true);
    m_outputView->setUndoRedoEnabled(false);
    m_outputView->document()->setMaximumBlockCount(MaxOutputLines);
    m_outputView->setStyleSheet(QStringLiteral("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }"));
    m_outputView->setFont(font);

    // Command Line Input and the Stop button
    m_inputLine = new QLineEdit(container);
    m_inputLine->setFont(font);
    m_inputLine->setPlaceholderText(QStringLiteral("Enter a command (e.g., git status)..."));
    m_inputLine->setStyleSheet(QStringLiteral("QLineEdit { background-color: #252526; color: #cccccc; padding: 4px; border: 1px solid #3c3c3c; }"));
    connect(m_inputLine, &QLineEdit::returnPressed, this, &TerminalDockWidget::handleCommandSubmitted);

    m_stopButton = new QToolButton(container);
    m_stopButton->setText(QStringLiteral("Stop"));
    m_stopButton->setToolTip(QStringLiteral("Stop the running command"));
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QToolButton::clicked, this, &TerminalDockWidget::stopCommand);

    auto *inputRow = new QHBoxLayout();
    inputRow->addWidget(m_inputLine, 1);
    inputRow->addWidget(m_stopButton);

    layout->addWidget(m_cwdLabel);
    layout->addWidget(m_outputView, 1);
    layout->addLayout(inputRow);

    setWidget(container);
    updateWorkingDirectoryLabel();
}

void TerminalDockWidget::updateWorkingDirectoryLabel() {
    QString shown = QDir::toNativeSeparators(m_workingDir);
    const QString home = QDir::toNativeSeparators(QDir::homePath());
    if (shown == home || shown.startsWith(home + QDir::separator())) {
        shown = QStringLiteral("~") + shown.mid(home.length());
    }
    m_cwdLabel->setText(shown);
    m_cwdLabel->setToolTip(QDir::toNativeSeparators(m_workingDir));
}

void TerminalDockWidget::changeDirectory(const QString &target) {
    if (target != m_workingDir) {
        m_previousDir = m_workingDir;
        m_workingDir = target;
    }
    updateWorkingDirectoryLabel();
}

void TerminalDockWidget::setWorkingDirectory(const QString &dir) {
    if (!dir.isEmpty()) {
        changeDirectory(QDir::cleanPath(dir));
    }
}

void TerminalDockWidget::executeCommand(const QString &command) {
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (m_process.state() != QProcess::NotRunning) {
        appendFormattedText(QStringLiteral("[A command is still running. Press Stop to end it.]\n"), QColor(241, 196, 15));
        return;
    }

    const TerminalCommand parsed = classifyTerminalCommand(trimmed.toStdString());
    if (parsed.kind == TerminalCommandKind::Clear) {
        clearTerminal();
        return;
    }

    appendFormattedText(QStringLiteral("$ ") + command + QLatin1Char('\n'), QColor(80, 160, 255));

    if (parsed.kind == TerminalCommandKind::ChangeDirectory) {
        handleChangeDirectory(QString::fromStdString(parsed.argument));
        return;
    }
    runInShell(command);
}

// `cd` has to be done here: a command runs in a process of its own, so a `cd` inside it
// would be forgotten as soon as that process ends.
void TerminalDockWidget::handleChangeDirectory(const QString &argument) {
    QString target;
    if (argument.isEmpty() || argument == QLatin1String("~")) {
        target = QDir::homePath();
    } else if (argument == QLatin1String("-")) {
        target = m_previousDir;
        if (target.isEmpty()) {
            appendFormattedText(QStringLiteral("cd: there is no previous folder\n"), QColor(231, 76, 60));
            return;
        }
    } else if (argument.startsWith(QLatin1String("~/"))) {
        target = QDir::homePath() + argument.mid(1);
    } else {
        target = argument;
    }

    if (QDir::isRelativePath(target)) {
        target = QDir(m_workingDir).absoluteFilePath(target);
    }
    target = QDir::cleanPath(target);

    if (!QFileInfo(target).isDir()) {
        appendFormattedText(QStringLiteral("cd: no such folder: ") + argument + QLatin1Char('\n'), QColor(231, 76, 60));
        return;
    }

    changeDirectory(target);
    if (argument == QLatin1String("-")) {
        appendFormattedText(target + QLatin1Char('\n'), QColor(212, 212, 212));   // the shell prints it too
    }
}

void TerminalDockWidget::runInShell(const QString &command) {
    m_outDecoder.resetState();
    m_errDecoder.resetState();
    m_stopRequested = false;

    // Git must never wait for a person: no password prompt on a terminal, no editor, no pager.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("dumb"));
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral(":"));
    env.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));
    env.insert(QStringLiteral("PAGER"), QStringLiteral("cat"));
    m_process.setProcessEnvironment(env);
    m_process.setWorkingDirectory(m_workingDir);

#ifdef Q_OS_UNIX
    // The shell becomes the leader of a new process group, so Stop can end it together with
    // everything it started. (setsid is safe to call here: it runs between fork and exec.)
    m_process.setChildProcessModifier([]() { ::setsid(); });
#endif

    m_process.setProgram(shellProgram());
    m_process.setArguments({QStringLiteral("-c"), command});

    setRunning(true);
    m_process.start();
    m_process.closeWriteChannel();   // nothing can be typed into a command: reading input ends instead of waiting
}

void TerminalDockWidget::setRunning(bool running) {
    if (running) {
        m_inputHadFocus = m_inputLine->hasFocus();
    }
    m_stopButton->setEnabled(running);
    m_inputLine->setEnabled(!running);
    if (!running && m_inputHadFocus) {
        m_inputLine->setFocus();   // give the focus back only if it was here before
    }
}

void TerminalDockWidget::stopCommand() {
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }
    m_stopRequested = true;
    endProcess(m_process, false);       // polite first (SIGTERM) ...
    m_killTimer.start(KillGraceMs);     // ... and forced if the command ignores it
}

void TerminalDockWidget::forceKill() {
    if (m_process.state() != QProcess::NotRunning) {
        endProcess(m_process, true);
    }
}

void TerminalDockWidget::handleCommandSubmitted() {
    const QString cmd = m_inputLine->text();
    m_inputLine->clear();
    executeCommand(cmd);
}

void TerminalDockWidget::handleReadyReadStandardOutput() {
    const QByteArray data = m_process.readAllStandardOutput();
    const QString text = m_outDecoder(data);
    if (!text.isEmpty()) {
        appendFormattedText(text, QColor(212, 212, 212));
    }
}

void TerminalDockWidget::handleReadyReadStandardError() {
    const QByteArray data = m_process.readAllStandardError();
    const QString text = m_errDecoder(data);
    if (!text.isEmpty()) {
        appendFormattedText(text, QColor(231, 76, 60));
    }
}

void TerminalDockWidget::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_killTimer.stop();

    // Whatever is still unread belongs above the status line.
    handleReadyReadStandardOutput();
    handleReadyReadStandardError();

    if (m_stopRequested) {
        appendFormattedText(QStringLiteral("[Stopped]\n"), QColor(241, 196, 15));
    } else if (exitStatus == QProcess::CrashExit) {
        appendFormattedText(QStringLiteral("[The command was ended by a signal]\n"), QColor(241, 196, 15));
    } else if (exitCode != 0) {
        appendFormattedText(QStringLiteral("[exit code ") + QString::number(exitCode) + QStringLiteral("]\n"), QColor(241, 196, 15));
    }

    m_stopRequested = false;
    setRunning(false);
    emit commandExecuted();
}

void TerminalDockWidget::handleProcessError(QProcess::ProcessError error) {
    // Only FailedToStart is handled here: QProcess sends no finished() for it, while a crash is
    // reported by handleProcessFinished.
    if (error != QProcess::FailedToStart) {
        return;
    }
    m_killTimer.stop();
    appendFormattedText(QStringLiteral("[Could not start the shell: ") + m_process.errorString() + QStringLiteral("]\n"),
                        QColor(231, 76, 60));
    setRunning(false);
}

void TerminalDockWidget::clearTerminal() {
    m_outputView->clear();
}

void TerminalDockWidget::appendLog(const QString &text, bool isError) {
    QString line = text;
    if (!line.endsWith(QLatin1Char('\n'))) {
        line += QLatin1Char('\n');
    }
    appendFormattedText(line, isError ? QColor(231, 76, 60) : QColor(150, 150, 150));
}

void TerminalDockWidget::appendFormattedText(const QString &text, const QColor &color) {
    QScrollBar *scrollBar = m_outputView->verticalScrollBar();
    const bool wasAtBottom = scrollBar->value() >= scrollBar->maximum() - 2;

    // A cursor of our own: it leaves the person's selection alone, unlike setTextCursor().
    QTextCursor cursor(m_outputView->document());
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    format.setForeground(color);
    cursor.insertText(text, format);

    // Follow the output only if the person was already at the end; otherwise they are reading.
    if (wasAtBottom) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

} // namespace Git
