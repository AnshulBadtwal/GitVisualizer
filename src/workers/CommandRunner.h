#ifndef COMMANDRUNNER_H
#define COMMANDRUNNER_H
#include "core/CustomContainers.h"
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace Git {

class CommandRunner : public QObject {
    Q_OBJECT

public:
    explicit CommandRunner(QObject *parent = nullptr);
    ~CommandRunner() override = default;

    /**
     * @brief Executes a Git CLI command asynchronously in the working directory.
     * @param workingDir Path to the local repository directory.
     * @param args Command line arguments (e.g., {"checkout", "main"}).
     */
    void runCommand(const QString &workingDir, const QStringList &args);

    /**
     * @brief Returns true if a command process is currently running.
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Cancels the currently executing process if running.
     */
    void cancel();

    /**
     * @brief Longest time one command may take before it is stopped (default 2 minutes, 0 = no limit).
     *        This is the safety net for anything that still waits for input, such as an SSH passphrase.
     */
    void setTimeoutMs(int milliseconds);

    /**
     * @brief The command that is running, or that just finished, as "git arg1 arg2 ...".
     *        Inside a commandFinished / commandFailed handler this is the command that handler is about.
     */
    [[nodiscard]] QString lastCommandLine() const;

signals:
    /**
     * @brief Emitted when the CLI operation completes successfully.
     * @param stdoutOutput Standard output text captured from the process.
     */
    void commandFinished(const QString &stdoutOutput);

    /**
     * @brief Emitted if the CLI operation fails (non-zero exit code or process error).
     * @param exitCode Process return code.
     * @param stderrOutput Standard error text captured from the process.
     */
    void commandFailed(int exitCode, const QString &stderrOutput);

private slots:
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessError(QProcess::ProcessError error);
    void handleTimeout();

private:
    struct CommandTask {
        QString workingDir;
        QStringList args;
    };
    CustomQueue<CommandTask> m_commandQueue;
    void startProcess(const CommandTask &task);
    QProcess m_process;
    QTimer m_timeoutTimer;
    CommandTask m_currentTask;
    int m_timeoutMs{DefaultTimeoutMs};
    bool m_timedOut{false};
    bool m_cancelled{false};
    static constexpr int DefaultTimeoutMs = 120000;
};

} // namespace Git

#endif // COMMANDRUNNER_H
