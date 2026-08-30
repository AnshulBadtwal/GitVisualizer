#ifndef COMMANDRUNNER_H
#define COMMANDRUNNER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

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

private:
    QProcess m_process;
};

} // namespace Git

#endif // COMMANDRUNNER_H
