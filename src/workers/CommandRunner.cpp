#include "workers/CommandRunner.h"
#include <QProcessEnvironment>
#include <QString>

namespace Git {

CommandRunner::CommandRunner(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::finished, this, &CommandRunner::handleProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &CommandRunner::handleProcessError);

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &CommandRunner::handleTimeout);
}

void CommandRunner::runCommand(const QString &workingDir, const QStringList &args) {
    const CommandTask task{workingDir, args};

    if (m_process.state() != QProcess::NotRunning) {
        m_commandQueue.enqueue(task);
        return;
    }

    startProcess(task);
}

void CommandRunner::startProcess(const CommandTask &task) {
    m_currentTask = task;
    m_timedOut = false;
    m_cancelled = false;

    // Git must never wait for a person: no password prompt on a terminal, no editor window.
    // ":" is Git's "do nothing" editor, so a merge or revert keeps its default message.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral(":"));
    env.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"), QStringLiteral(":"));
    env.insert(QStringLiteral("GIT_MERGE_AUTOEDIT"), QStringLiteral("no"));
    m_process.setProcessEnvironment(env);

    m_process.setWorkingDirectory(task.workingDir);
    m_process.setProgram(QStringLiteral("git"));
    m_process.setArguments(task.args);
    m_process.start();

    // Nothing will be typed: reading from input ends instead of waiting.
    m_process.closeWriteChannel();

    if (m_timeoutMs > 0) {
        m_timeoutTimer.start(m_timeoutMs);
    }
}

bool CommandRunner::isRunning() const {
    return m_process.state() != QProcess::NotRunning;
}

void CommandRunner::cancel() {
    if (isRunning()) {
        m_commandQueue.clear();
        m_cancelled = true;
        m_process.kill();
    }
}

void CommandRunner::setTimeoutMs(int milliseconds) {
    m_timeoutMs = milliseconds;
}

QString CommandRunner::lastCommandLine() const {
    return QStringLiteral("git ") + m_currentTask.args.join(QLatin1Char(' '));
}

void CommandRunner::handleTimeout() {
    // kill() ends the process; handleProcessFinished reports it with the right message.
    m_timedOut = true;
    m_process.kill();
}

void CommandRunner::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_timeoutTimer.stop();

    QString stdoutOutput = QString::fromUtf8(m_process.readAllStandardOutput());
    QString stderrOutput = QString::fromUtf8(m_process.readAllStandardError());

    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);

    if (!success) {
        if (m_timedOut) {
            stderrOutput = QStringLiteral("Git did not finish within %1 seconds and was stopped. "
                                          "It was probably waiting for input (for example a password).")
                               .arg(m_timeoutMs / 1000);
        } else if (m_cancelled) {
            stderrOutput = QStringLiteral("The command was cancelled.");
        } else if (exitStatus == QProcess::CrashExit) {
            stderrOutput = QStringLiteral("The Git process crashed unexpectedly.")
                           + (stderrOutput.isEmpty() ? QString() : QStringLiteral("\n") + stderrOutput);
        } else if (stderrOutput.isEmpty()) {
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
    // Only FailedToStart is handled here, because QProcess does not emit finished() for it.
    // A crash is reported by handleProcessFinished (QProcess emits finished() right after the
    // error), so handling it in both places is what used to send commandFailed twice.
    // The other errors (read/write problems) do not end the process.
    if (error != QProcess::FailedToStart) {
        return;
    }

    m_timeoutTimer.stop();
    m_commandQueue.clear();
    emit commandFailed(-1, QStringLiteral("Failed to execute 'git'. Ensure Git is installed and in PATH."));
}

} // namespace Git
