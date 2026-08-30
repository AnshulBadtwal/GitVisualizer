#ifndef DIFFVIEWERWIDGET_H
#define DIFFVIEWERWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include <memory>
#include <git2.h>
#include "core/GitTypes.h"
#include "core/GitRepository.h"

namespace Git {

// RAII deleters for libgit2 diff objects
struct GitDiffDeleter {
    void operator()(git_diff *diff) const {
        if (diff) {
            git_diff_free(diff);
        }
    }
};

struct GitTreeDeleter {
    void operator()(git_tree *tree) const {
        if (tree) {
            git_tree_free(tree);
        }
    }
};

using UniqueGitDiff = std::unique_ptr<git_diff, GitDiffDeleter>;
using UniqueGitTree = std::unique_ptr<git_tree, GitTreeDeleter>;

class DiffViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit DiffViewerWidget(QWidget *parent = nullptr);
    ~DiffViewerWidget() override = default;

    /**
     * @brief Loads and displays diff for a specific commit or uncommitted changes.
     * @param repoPath Path to the active Git repository.
     * @param commitOid Commit SHA-1 hex string to inspect.
     */
    void loadDiffForCommit(const QString &repoPath, const QString &commitOid);

    /**
     * @brief Clears the current diff view.
     */
    void clear();

private slots:
    void handleViewModeChanged(int index);

private:
    enum ViewMode {
        Unified = 0,
        SideBySide = 1
    };

    QTextEdit *m_diffDisplay{nullptr};
    QComboBox *m_modeCombo{nullptr};
    QLabel *m_headerLabel{nullptr};

    QString m_currentRepoPath;
    QString m_currentCommitOid;
    QString m_rawDiffPatch;

    void setupUi();
    void renderDiff();
    
    [[nodiscard]] QString computeDiffText(git_repository *repo, const std::string &oidStr);
    
    static int diffLineCallback(const git_diff_delta *delta,
                                const git_diff_hunk *hunk,
                                const git_diff_line *line,
                                void *payload);
};

} // namespace Git

#endif // DIFFVIEWERWIDGET_H
