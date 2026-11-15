#ifndef DIFFVIEWERWIDGET_H
#define DIFFVIEWERWIDGET_H

#include <QString>
#include <QWidget>
#include <vector>
#include "core/GitTypes.h"

class QLabel;
class QTextEdit;

namespace Git {

class DiffHighlighter;   // defined in DiffViewerWidget.cpp

// Shows the patch of one commit. It does no Git work: DetailsWorker reads the commit on a worker
// thread and MainWindow hands the result to setDetails().
class DiffViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit DiffViewerWidget(QWidget *parent = nullptr);
    ~DiffViewerWidget() override = default;

    /// Back to the empty state ("No Commit Selected").
    void clear();

    /// Shows that the diff of this commit is being read.
    void showLoading(const QString &shortHash);

    /// Shows the patch (or the reason there is none) of a finished commit.
    void setDetails(const CommitDetails &details);

public slots:
    /// Scrolls so that the file with this index (the order of details.parsed.files) is at the top.
    void scrollToFile(int fileIndex);

private:
    void showMessage(const QString &headerText, const QString &bodyText);

    QTextEdit *m_diffDisplay{nullptr};
    QLabel *m_headerLabel{nullptr};
    DiffHighlighter *m_highlighter{nullptr};
    std::vector<DiffFileSection> m_files;
};

} // namespace Git

#endif // DIFFVIEWERWIDGET_H
