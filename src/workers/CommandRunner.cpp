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
        m_commandQueue.enqueue({workingDir, args});
        return;
    }

    m_process.setWorkingDirectory(workingDir);
    m_process.setProgram(QStringLiteral("git"));
    m_process.setArguments(args);
    m_process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    m_process.start();
}

bool CommandRunner::isRunning() const {
    return m_process.state() != QProcess::NotRunning;
}

void CommandRunner::cancel() {
    if (isRunning()) {
        m_commandQueue.clear();
        m_process.kill();
    }
}

void CommandRunner::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString stdoutOutput = QString::fromUtf8(m_process.readAllStandardOutput());
    QString stderrOutput = QString::fromUtf8(m_process.readAllStandardError());

    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);

    if (!success) {
        if (stderrOutput.isEmpty()) {
            stderrOutput = QString::fromUtf8("Process exited with code: ") + QString::number(exitCode);
        }
        // Clear pending queue on failure so subsequent chained commands don't run blindly
        m_commandQueue.clear();
        emit commandFailed(exitCode, stderrOutput);
        return;
    }

    // Emit success for the completed command
    emit commandFinished(stdoutOutput);

    // If more commands are waiting in the custom queue, execute the next one
    if (!m_commandQueue.isEmpty()) {
        auto nextTask = m_commandQueue.front();
        m_commandQueue.dequeue();
        runCommand(nextTask.workingDir, nextTask.args);
    }
}

void CommandRunner::handleProcessError(QProcess::ProcessError error) {
    if (m_process.state() == QProcess::NotRunning && m_process.exitCode() != 0) {
        return;
    }

    m_commandQueue.clear();

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
