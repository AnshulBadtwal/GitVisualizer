#include "ui/CommitDetailsWidget.h"
#include <QDateTime>
#include <QFontDatabase>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

namespace Git {

namespace {

QString formatTime(int64_t seconds) {
    return QLocale::system().toString(
        QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds)), QLocale::LongFormat);
}

QString person(const std::string &name, const std::string &email) {
    QString text = QString::fromStdString(name);
    if (!email.empty()) {
        text += QStringLiteral(" <") + QString::fromStdString(email) + QLatin1Char('>');
    }
    return text;
}

QString joinNames(const std::vector<std::string> &names) {
    QStringList parts;
    for (const std::string &name : names) {
        parts << QString::fromStdString(name);
    }
    return parts.join(QStringLiteral(", "));
}

// Same rule as git_commit_summary: the subject is the first paragraph on one line,
// the body is everything after the first blank line.
void splitMessage(const QString &message, QString &subject, QString &body) {
    const QString trimmed = message.trimmed();
    const qsizetype gap = trimmed.indexOf(QStringLiteral("\n\n"));
    if (gap < 0) {
        subject = trimmed;
        body.clear();
    } else {
        subject = trimmed.left(gap);
        body = trimmed.mid(gap + 2).trimmed();
    }
    subject.replace(QLatin1Char('\n'), QLatin1Char(' '));
}

} // namespace

CommitDetailsWidget::CommitDetailsWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    auto makeLabel = [this]() {
        auto *label = new QLabel(this);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    };

    m_subject = makeLabel();
    QFont subjectFont = m_subject->font();
    subjectFont.setBold(true);
    m_subject->setFont(subjectFont);

    m_author = makeLabel();
    m_committer = makeLabel();
    m_committer->hide();

    m_hash = makeLabel();
    m_hash->setFont(mono);

    m_parents = new QLabel(this);
    m_parents->setWordWrap(true);
    m_parents->setTextFormat(Qt::RichText);
    m_parents->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_parents->setOpenExternalLinks(false);
    connect(m_parents, &QLabel::linkActivated, this, &CommitDetailsWidget::parentActivated);

    m_refs = makeLabel();

    m_body = new QPlainTextEdit(this);
    m_body->setReadOnly(true);

    m_filesSummary = new QLabel(this);

    m_files = new QListWidget(this);
    m_files->setFont(mono);
    connect(m_files, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            emit fileActivated(row);
        }
    });

    layout->addWidget(m_subject);
    layout->addWidget(m_author);
    layout->addWidget(m_committer);
    layout->addWidget(m_hash);
    layout->addWidget(m_parents);
    layout->addWidget(m_refs);
    layout->addWidget(m_body, 2);
    layout->addWidget(m_filesSummary);
    layout->addWidget(m_files, 3);

    clear();
}

void CommitDetailsWidget::clearFiles() {
    const QSignalBlocker blocker(m_files);   // clearing must not look like a file being selected
    m_files->clear();
}

void CommitDetailsWidget::clear() {
    m_oid.clear();
    m_subject->setText(QStringLiteral("No commit selected"));
    m_author->clear();
    m_committer->hide();
    m_hash->clear();
    m_parents->clear();
    m_refs->hide();
    m_body->clear();
    m_filesSummary->clear();
    clearFiles();
}

void CommitDetailsWidget::setCommit(const CommitNode &node) {
    const QString oid = QString::fromStdString(node.oid);

    if (oid != m_oid) {
        m_oid = oid;
        m_subject->setText(QString::fromStdString(node.message));
        m_committer->hide();
        m_body->clear();
        m_filesSummary->setText(QStringLiteral("Loading changed files..."));
        clearFiles();
    }

    m_author->setText(person(node.author, node.email) + QStringLiteral("  -  ") + formatTime(node.timestamp));
    m_hash->setText(oid);

    if (node.parentOids.empty()) {
        m_parents->setText(QStringLiteral("Parents: none (first commit)"));
    } else {
        QStringList links;
        for (const std::string &parent : node.parentOids) {
            const QString full = QString::fromStdString(parent);
            links << QStringLiteral("<a href=\"%1\">%2</a>").arg(full, full.left(7));
        }
        const QString label = node.parentOids.size() > 1 ? QStringLiteral("Merge of: ")
                                                         : QStringLiteral("Parent: ");
        m_parents->setText(label + links.join(QStringLiteral(", ")));
    }

    QStringList refs;
    if (node.isHead && node.checkedOutBranch.empty()) {
        refs << QStringLiteral("HEAD (detached)");
    }
    if (!node.branchNames.empty()) {
        refs << QStringLiteral("Branches: ") + joinNames(node.branchNames);
    }
    if (!node.remoteBranchNames.empty()) {
        refs << QStringLiteral("Remote: ") + joinNames(node.remoteBranchNames);
    }
    if (!node.tagNames.empty()) {
        refs << QStringLiteral("Tags: ") + joinNames(node.tagNames);
    }
    m_refs->setText(refs.join(QStringLiteral("     ")));
    m_refs->setVisible(!refs.isEmpty());
}

void CommitDetailsWidget::setDetails(const CommitDetails &details) {
    if (QString::fromStdString(details.oid) != m_oid) {
        return;   // an answer for a commit that is no longer shown
    }

    if (!details.ok()) {
        m_body->setPlainText(QString::fromStdString(details.error));
        m_filesSummary->clear();
        clearFiles();
        return;
    }

    QString subject;
    QString body;
    splitMessage(QString::fromStdString(details.fullMessage), subject, body);
    m_subject->setText(subject);
    m_body->setPlainText(body);

    const bool sameAsAuthor = details.committerName == details.authorName
                           && details.committerEmail == details.authorEmail
                           && details.committerTime == details.authorTime;
    if (sameAsAuthor) {
        m_committer->hide();
    } else {
        m_committer->setText(QStringLiteral("Committed by ")
                             + person(details.committerName, details.committerEmail)
                             + QStringLiteral("  -  ") + formatTime(details.committerTime));
        m_committer->show();
    }

    {
        const QSignalBlocker blocker(m_files);   // filling the list is not the person choosing a file
        m_files->clear();

        int added = 0;
        int removed = 0;
        for (const DiffFileSection &file : details.parsed.files) {
            QString status = QStringLiteral("M");
            if (file.change == FileChange::Added) {
                status = QStringLiteral("A");
            } else if (file.change == FileChange::Deleted) {
                status = QStringLiteral("D");
            }

            QString line = status + QStringLiteral("  ") + QString::fromStdString(file.path);
            if (file.binary) {
                line += QStringLiteral("  (binary)");
            } else {
                line += QStringLiteral("  +%1 -%2").arg(file.added).arg(file.removed);
            }
            m_files->addItem(line);

            added += file.added;
            removed += file.removed;
        }

        const int fileCount = static_cast<int>(details.parsed.files.size());
        if (!details.patchError.empty()) {
            m_filesSummary->setText(QString::fromStdString(details.patchError));
        } else if (fileCount == 0) {
            m_filesSummary->setText(QStringLiteral("No file changes"));
        } else {
            QString summary = QStringLiteral("%1 file(s) changed, +%2 -%3").arg(fileCount).arg(added).arg(removed);
            if (details.patchTruncated) {
                summary += QStringLiteral("  (diff truncated: the list may be incomplete)");
            }
            m_filesSummary->setText(summary);
        }
    }
}

} // namespace Git
