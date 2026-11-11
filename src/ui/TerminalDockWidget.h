#ifndef TERMINALDOCKWIDGET_H
#define TERMINALDOCKWIDGET_H

#include <QDockWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>
#include <QStringList>

namespace Git {

class TerminalDockWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit TerminalDockWidget(const QString &title, QWidget *parent = nullptr);
    ~TerminalDockWidget() override;

    /**
     * @brief Sets the active working directory for the embedded terminal shell.
     * @param dir Path to the target directory.
     */
    void setWorkingDirectory(const QString &dir);

    /**
     * @brief Executes a raw command line string directly in the embedded shell session.
     * @param command Command string to run.
     */
    void executeCommand(const QString &command);

    /**
     * @brief Clears the terminal output buffer.
     */
    void clearTerminal();

    /**
     * @brief Adds a line to the output panel without sending it to the shell
     *        (used for the results of commands started from the graph's menus).
     * @param text    Text to show; a trailing newline is added when missing.
     * @param isError Shown in red when true.
     */
    void appendLog(const QString &text, bool isError);

signals:
    /**
     * @brief Emitted after a command starting with "git" was sent to the shell. The command may
     *        still be running and may not have changed anything; listeners should check, not assume.
     */
    void commandExecuted();

private slots:
    void handleCommandSubmitted();
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QTextEdit *m_outputView{nullptr};
    QLineEdit *m_inputLine{nullptr};
    QProcess m_shellProcess;
    QString m_workingDir;

    void setupUi();
    void startShellSession();
    void appendFormattedText(const QString &text, const QColor &color);
};

} // namespace Git

#endif // TERMINALDOCKWIDGET_H
