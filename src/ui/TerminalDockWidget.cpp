#include "ui/TerminalDockWidget.h"
#include <QFontDatabase>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QProcessEnvironment>
#include <QDir>
#include <QStringLiteral>
#include <cstdlib>

namespace Git {

TerminalDockWidget::TerminalDockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    setupUi();
    startShellSession();
}

TerminalDockWidget::~TerminalDockWidget() {
    if (m_shellProcess.state() != QProcess::NotRunning) {
        m_shellProcess.terminate();
        if (!m_shellProcess.waitForFinished(1000)) {
            m_shellProcess.kill();
        }
    }
}

void TerminalDockWidget::setupUi() {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Terminal Output Display
    m_outputView = new QTextEdit(container);
    m_outputView->setReadOnly(true);
    m_outputView->setUndoRedoEnabled(false);
    m_outputView->setStyleSheet(QStringLiteral("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }"));

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    m_outputView->setFont(font);

    // Command Line Input
    m_inputLine = new QLineEdit(container);
    m_inputLine->setFont(font);
    m_inputLine->setPlaceholderText(QStringLiteral("Enter shell command (e.g., git status)..."));
    m_inputLine->setStyleSheet(QStringLiteral("QLineEdit { background-color: #252526; color: #cccccc; padding: 4px; border: 1px solid #3c3c3c; }"));

    connect(m_inputLine, &QLineEdit::returnPressed, this, &TerminalDockWidget::handleCommandSubmitted);

    layout->addWidget(m_outputView);
    layout->addWidget(m_inputLine);

    setWidget(container);

    connect(&m_shellProcess, &QProcess::readyReadStandardOutput, this, &TerminalDockWidget::handleReadyReadStandardOutput);
    connect(&m_shellProcess, &QProcess::readyReadStandardError, this, &TerminalDockWidget::handleReadyReadStandardError);
    connect(&m_shellProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalDockWidget::handleProcessFinished);
}

void TerminalDockWidget::startShellSession() {
    if (m_shellProcess.state() != QProcess::NotRunning) {
        return;
    }

    // Determine system default shell on Linux/Pop!_OS
    const char *envShell = std::getenv("SHELL");
    QString shellCmd = envShell ? QString::fromUtf8(envShell) : QStringLiteral("/bin/bash");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    m_shellProcess.setProcessEnvironment(env);

    if (!m_workingDir.isEmpty()) {
        m_shellProcess.setWorkingDirectory(m_workingDir);
    }

    // Launch shell in interactive mode
    m_shellProcess.start(shellCmd, QStringList() << QStringLiteral("-i"));
}

void TerminalDockWidget::setWorkingDirectory(const QString &dir) {
    m_workingDir = dir;
    if (m_shellProcess.state() == QProcess::Running) {
        executeCommand(QStringLiteral("cd ") + QStringLiteral("\"") + dir + QStringLiteral("\""));
    } else {
        startShellSession();
    }
}

void TerminalDockWidget::executeCommand(const QString &command) {
    if (command.trimmed().isEmpty()) {
        return;
    }

    if (m_shellProcess.state() != QProcess::Running) {
        startShellSession();
    }

    appendFormattedText(QStringLiteral("$ ") + command + QStringLiteral("\n"), QColor(80, 160, 255));

    QByteArray bytes = command.toUtf8() + "\n";
    m_shellProcess.write(bytes);

    if (command.trimmed().startsWith(QStringLiteral("git"))) {
        emit commandExecuted();
    }
}

void TerminalDockWidget::handleCommandSubmitted() {
    QString cmd = m_inputLine->text();
    m_inputLine->clear();
    executeCommand(cmd);
}

void TerminalDockWidget::handleReadyReadStandardOutput() {
    QByteArray data = m_shellProcess.readAllStandardOutput();
    appendFormattedText(QString::fromUtf8(data), QColor(212, 212, 212));
}

void TerminalDockWidget::handleReadyReadStandardError() {
    QByteArray data = m_shellProcess.readAllStandardError();
    appendFormattedText(QString::fromUtf8(data), QColor(231, 76, 60));
}

void TerminalDockWidget::handleProcessFinished(int exitCode, QProcess::ExitStatus) {
    appendFormattedText(QStringLiteral("\n[Process Terminated with exit code ") + QString::number(exitCode) + QStringLiteral("]\n"), QColor(241, 196, 15));
}

void TerminalDockWidget::clearTerminal() {
    m_outputView->clear();
}

void TerminalDockWidget::appendFormattedText(const QString &text, const QColor &color) {
    QTextCursor cursor(m_outputView->document());
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    format.setForeground(color);
    cursor.setCharFormat(format);
    cursor.insertText(text);

    m_outputView->setTextCursor(cursor);
    m_outputView->ensureCursorVisible();
}

} // namespace Git
