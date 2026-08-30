#include "workers/CommandRunner.h"
#include <QStringLiteral>

namespace Git {

CommandRunner::CommandRunner(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::finished, this, &CommandRunner::handleProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &CommandRunner::handleProcessError);
}

void CommandRunner::runCommand(const QString &workingDir, const QStringList &args) {
    if (m_process.state() != QProcess::NotRunning) {
        emit commandFailed(-1, QStringLiteral("A Git command is already executing."));
        return;
    }

    m_process.setWorkingDirectory(workingDir);
    m_process.setProgram(QStringLiteral("git"));
    m_process.setArguments(args);

    // Merge system environment variables so SSH agents, GPG keys, and PATH are retained
    m_process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    m_process.start();
}

bool CommandRunner::isRunning() const {
    return m_process.state() != QProcess::NotRunning;
}

void CommandRunner::cancel() {
    if (isRunning()) {
        m_process.kill();
    }
}

void CommandRunner::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString stdoutOutput = QString::fromUtf8(m_process.readAllStandardOutput());
    QString stderrOutput = QString::fromUtf8(m_process.readAllStandardError());

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        emit commandFinished(stdoutOutput);
    } else {
        if (stderrOutput.isEmpty()) {
            stderrOutput = QString::fromUtf8("Process exited with code: ") + QString::number(exitCode);
        }
        emit commandFailed(exitCode, stderrOutput);
    }
}

void CommandRunner::handleProcessError(QProcess::ProcessError error) {
    // Avoid double-emitting if handleProcessFinished captures non-zero exit code
    if (m_process.state() == QProcess::NotRunning && m_process.exitCode() != 0) {
        return;
    }

    QString errorMessage;
    switch (error) {
    case QProcess::FailedToStart:
        errorMessage = QStringLiteral("Failed to execute 'git' binary. Ensure Git is installed and in PATH.");
        break;
    case QProcess::Crashed:
        errorMessage = QStringLiteral("Git process crashed unexpectedly.");
        break;
    case QProcess::Timedout:
        errorMessage = QStringLiteral("Git command timed out.");
        break;
    case QProcess::WriteError:
        errorMessage = QStringLiteral("Failed to write to Git process pipe.");
        break;
    case QProcess::ReadError:
        errorMessage = QStringLiteral("Failed to read from Git process pipe.");
        break;
    default:
        errorMessage = QStringLiteral("An unknown process error occurred.");
        break;
    }

    emit commandFailed(-1, errorMessage);
}

} // namespace Git
