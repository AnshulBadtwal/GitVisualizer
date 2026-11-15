#ifndef TERMINALDOCKWIDGET_H
#define TERMINALDOCKWIDGET_H

#include <QDockWidget>
#include <QLineEdit>
#include <QProcess>
#include <QString>
#include <QStringDecoder>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

class QLabel;

namespace Git {

// A small command panel: type a command, see its output.
//
// Every command runs in its own `bash -c` process (or `sh -c` if bash is missing), whatever your
// login shell is (fish, zsh, ...). That keeps the panel predictable: a mistyped quote or a syntax
// error only affects that one command, there is no prompt or job-control noise, the exit code is
// known, and Stop can end a command together with everything it started.
//
// What this means in practice:
//   * `cd folder` and `clear` are handled by the panel itself (see core/TerminalCommand.h), so the
//     folder you cd to is kept for the next command.
//   * Variables you export, aliases and shell functions do NOT carry over to the next command.
//   * A command cannot ask questions: its input is closed, and Git is told not to prompt
//     (GIT_TERMINAL_PROMPT=0) or open an editor, so an HTTPS password or an SSH passphrase prompt fails
//     instead of hanging. Use ssh-agent or a credential helper.
class TerminalDockWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit TerminalDockWidget(const QString &title, QWidget *parent = nullptr);
    ~TerminalDockWidget() override;

    /**
     * @brief Sets the folder the next command starts in (the panel's current folder).
     * @param dir Path to the target directory.
     */
    void setWorkingDirectory(const QString &dir);

    /**
     * @brief Runs a command line, as if it had been typed. Ignored while another command runs.
     * @param command Command string to run.
     */
    void executeCommand(const QString &command);

    /**
     * @brief Clears the terminal output buffer.
     */
    void clearTerminal();

    /**
     * @brief Adds a line to the output panel without running anything
     *        (used for the results of commands started from the graph's menus).
     * @param text    Text to show; a trailing newline is added when missing.
     * @param isError Shown in red when true.
     */
    void appendLog(const QString &text, bool isError);

public slots:
    /// Ends the running command and everything it started (the Stop button).
    void stopCommand();

signals:
    /**
     * @brief Emitted when a command typed in the panel has finished, whatever its result.
     *        It may have changed the repository or nothing at all; listeners should check, not assume.
     */
    void commandExecuted();

private slots:
    void handleCommandSubmitted();
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessError(QProcess::ProcessError error);
    void forceKill();

private:
    QTextEdit *m_outputView{nullptr};
    QLineEdit *m_inputLine{nullptr};
    QToolButton *m_stopButton{nullptr};
    QLabel *m_cwdLabel{nullptr};

    QProcess m_process;
    QTimer m_killTimer;                       // Stop asks politely first, then forces after a moment
    QStringDecoder m_outDecoder{QStringDecoder::Utf8};   // keep their state between reads, so a
    QStringDecoder m_errDecoder{QStringDecoder::Utf8};   // character split over two reads stays whole

    QString m_workingDir;
    QString m_previousDir;                    // for "cd -"
    bool m_stopRequested{false};
    bool m_inputHadFocus{false};

    void setupUi();
    void runInShell(const QString &command);
    void changeDirectory(const QString &target);
    void handleChangeDirectory(const QString &argument);
    void updateWorkingDirectoryLabel();
    void setRunning(bool running);
    void appendFormattedText(const QString &text, const QColor &color);
};

} // namespace Git

#endif // TERMINALDOCKWIDGET_H
