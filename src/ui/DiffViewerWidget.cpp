#include "ui/DiffViewerWidget.h"
#include <QFontDatabase>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QStringLiteral>
#include <sstream>

namespace Git {

DiffViewerWidget::DiffViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void DiffViewerWidget::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Header Toolbar
    auto *headerLayout = new QHBoxLayout();
    m_headerLabel = new QLabel(QStringLiteral("No Commit Selected"), this);
    m_headerLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #888;"));

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("Unified Diff"), Unified);
    m_modeCombo->addItem(QStringLiteral("Side-by-Side (Formatted)"), SideBySide);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiffViewerWidget::handleViewModeChanged);

    headerLayout->addWidget(m_headerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_modeCombo);

    // Diff Display Panel
    m_diffDisplay = new QTextEdit(this);
    m_diffDisplay->setReadOnly(true);
    m_diffDisplay->setLineWrapMode(QTextEdit::NoWrap);

    // Fixed-width font configuration
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    m_diffDisplay->setFont(font);

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_diffDisplay);
}

void DiffViewerWidget::clear() {
    m_currentCommitOid.clear();
    m_rawDiffPatch.clear();
    m_headerLabel->setText(QStringLiteral("No Commit Selected"));
    m_diffDisplay->clear();
}

void DiffViewerWidget::loadDiffForCommit(const QString &repoPath, const QString &commitOid) {
    if (repoPath.isEmpty() || commitOid.isEmpty()) {
        clear();
        return;
    }

    m_currentRepoPath = repoPath;
    m_currentCommitOid = commitOid;

    git_repository *repoPtr = nullptr;
    if (git_repository_open(&repoPtr, repoPath.toStdString().c_str()) < 0 || !repoPtr) {
        m_diffDisplay->setPlainText(QStringLiteral("Error: Unable to open repository for diff calculation."));
        return;
    }
    UniqueGitRepository repo(repoPtr);

    m_headerLabel->setText(QStringLiteral("Diff for Commit: ") + commitOid.left(7));
    m_rawDiffPatch = computeDiffText(repo.get(), commitOid.toStdString());
    
    renderDiff();
}

QString DiffViewerWidget::computeDiffText(git_repository *repo, const std::string &oidStr) {
    git_oid oid;
    if (git_oid_fromstr(&oid, oidStr.c_str()) < 0) {
        return QStringLiteral("Invalid Commit OID.");
    }

    git_commit *commitPtr = nullptr;
    if (git_commit_lookup(&commitPtr, repo, &oid) < 0 || !commitPtr) {
        return QStringLiteral("Failed to lookup commit.");
    }
    UniqueGitCommit commit(commitPtr);

    git_tree *treePtr = nullptr;
    if (git_commit_tree(&treePtr, commit.get()) < 0 || !treePtr) {
        return QStringLiteral("Failed to retrieve commit tree.");
    }
    UniqueGitTree tree(treePtr);

    UniqueGitTree parentTree;
    if (git_commit_parentcount(commit.get()) > 0) {
        git_commit *parentCommitPtr = nullptr;
        if (git_commit_parent(&parentCommitPtr, commit.get(), 0) == 0 && parentCommitPtr) {
            UniqueGitCommit parentCommit(parentCommitPtr);
            git_tree *pTreePtr = nullptr;
            if (git_commit_tree(&pTreePtr, parentCommit.get()) == 0 && pTreePtr) {
                parentTree.reset(pTreePtr);
            }
        }
    }

    git_diff *diffPtr = nullptr;
    if (git_diff_tree_to_tree(&diffPtr, repo, parentTree.get(), tree.get(), nullptr) < 0 || !diffPtr) {
        return QStringLiteral("Failed to generate tree-to-tree diff.");
    }
    UniqueGitDiff diff(diffPtr);

    std::string diffOutput;
    git_diff_print(diff.get(), GIT_DIFF_FORMAT_PATCH, diffLineCallback, &diffOutput);

    return QString::fromUtf8(diffOutput.c_str());
}

int DiffViewerWidget::diffLineCallback(const git_diff_delta *,
                                       const git_diff_hunk *,
                                       const git_diff_line *line,
                                       void *payload) 
{
    auto *output = static_cast<std::string*>(payload);
    if (line && output) {
        output->append(line->content, line->content_len);
    }
    return 0;
}

void DiffViewerWidget::handleViewModeChanged(int) {
    renderDiff();
}

void DiffViewerWidget::renderDiff() {
    m_diffDisplay->clear();

    if (m_rawDiffPatch.isEmpty()) {
        m_diffDisplay->setPlainText(QStringLiteral("(No changes in this commit)"));
        return;
    }

    QTextCursor cursor(m_diffDisplay->document());
    QStringList lines = m_rawDiffPatch.split(QLatin1Char('\n'));

    QTextCharFormat normalFormat;
    normalFormat.setForeground(QColor(180, 180, 180));

    QTextCharFormat addedFormat;
    addedFormat.setForeground(QColor(46, 204, 113));
    addedFormat.setBackground(QColor(46, 204, 113, 30));

    QTextCharFormat removedFormat;
    removedFormat.setForeground(QColor(231, 76, 60));
    removedFormat.setBackground(QColor(231, 76, 60, 30));

    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(52, 152, 219));
    headerFormat.setFontWeight(QFont::Bold);

    QTextCharFormat hunkFormat;
    hunkFormat.setForeground(QColor(155, 89, 182));

    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("diff --git")) || 
            line.startsWith(QStringLiteral("index ")) ||
            line.startsWith(QStringLiteral("--- ")) || 
            line.startsWith(QStringLiteral("+++ "))) {
            cursor.setBlockCharFormat(headerFormat);
        } else if (line.startsWith(QStringLiteral("@@"))) {
            cursor.setBlockCharFormat(hunkFormat);
        } else if (line.startsWith(QStringLiteral("+"))) {
            cursor.setBlockCharFormat(addedFormat);
        } else if (line.startsWith(QStringLiteral("-"))) {
            cursor.setBlockCharFormat(removedFormat);
        } else {
            cursor.setBlockCharFormat(normalFormat);
        }

        cursor.insertText(line + QStringLiteral("\n"));
    }
}

} // namespace Git
