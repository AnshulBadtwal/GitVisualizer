#ifndef COMMITDETAILSWIDGET_H
#define COMMITDETAILSWIDGET_H

#include <QString>
#include <QWidget>
#include "core/GitTypes.h"

class QLabel;
class QListWidget;
class QPlainTextEdit;

namespace Git {

// The panel next to the diff: who, when, the full message, the parents (clickable), the branches
// and tags on the commit, and the list of changed files (clicking a file scrolls the diff to it).
//
// It is filled in two steps so it never feels slow: setCommit() shows what the graph already
// knows at once, and setDetails() adds the rest when the worker thread has read the commit.
class CommitDetailsWidget : public QWidget {
    Q_OBJECT

public:
    explicit CommitDetailsWidget(QWidget *parent = nullptr);
    ~CommitDetailsWidget() override = default;

    /// Back to the empty state.
    void clear();

    /// Shows what is known from the graph row. Calling it again for the commit that is already
    /// shown only refreshes branches, tags and parents (they can change when the graph reloads);
    /// the message and the file list stay.
    void setCommit(const CommitNode &node);

    /// Adds the full message, the committer and the changed files. Ignored if `details` is for
    /// another commit than the one shown (an answer that arrived late).
    void setDetails(const CommitDetails &details);

signals:
    /// A parent link was clicked. `oid` is the full id of the parent.
    void parentActivated(const QString &oid);

    /// A file in the list was selected. `fileIndex` is its position in details.parsed.files.
    void fileActivated(int fileIndex);

private:
    void clearFiles();

    QString m_oid;   // the commit shown right now

    QLabel *m_subject{nullptr};
    QLabel *m_author{nullptr};
    QLabel *m_committer{nullptr};
    QLabel *m_hash{nullptr};
    QLabel *m_parents{nullptr};
    QLabel *m_refs{nullptr};
    QPlainTextEdit *m_body{nullptr};
    QLabel *m_filesSummary{nullptr};
    QListWidget *m_files{nullptr};
};

} // namespace Git

#endif // COMMITDETAILSWIDGET_H
