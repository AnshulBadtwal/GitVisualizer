#include "ui/DiffViewerWidget.h"
#include <QAbstractTextDocumentLayout>
#include <QFontDatabase>
#include <QLabel>
#include <QPalette>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Git {

// Colors one line per block, using the line kinds that parsePatch worked out on the worker
// thread. Block number N of the document is line N of the patch.
class DiffHighlighter : public QSyntaxHighlighter {
public:
    DiffHighlighter(QTextDocument *document, bool darkTheme)
        : QSyntaxHighlighter(document)
    {
        m_header.setFontWeight(QFont::Bold);
        m_header.setForeground(darkTheme ? QColor(100, 180, 240) : QColor(30, 110, 180));
        m_hunk.setForeground(darkTheme ? QColor(190, 140, 220) : QColor(130, 60, 160));
        m_added.setForeground(darkTheme ? QColor(120, 220, 150) : QColor(20, 110, 55));
        m_added.setBackground(QColor(46, 204, 113, 45));
        m_removed.setForeground(darkTheme ? QColor(240, 130, 120) : QColor(170, 35, 25));
        m_removed.setBackground(QColor(231, 76, 60, 45));
    }

    /// Does not re-highlight by itself: detach the highlighter, change the text, call this,
    /// then attach it again (see DiffViewerWidget::setDetails) so the text is colored only once.
    void setKinds(const std::vector<DiffLineKind> &kinds) { m_kinds = kinds; }

protected:
    void highlightBlock(const QString &text) override {
        const int block = currentBlock().blockNumber();
        if (block < 0 || static_cast<std::size_t>(block) >= m_kinds.size() || text.isEmpty()) {
            return;   // also covers the empty block after the last line, and plain messages
        }
        const int length = static_cast<int>(text.length());
        switch (m_kinds[static_cast<std::size_t>(block)]) {
        case DiffLineKind::FileHeader: setFormat(0, length, m_header); break;
        case DiffLineKind::Hunk:       setFormat(0, length, m_hunk); break;
        case DiffLineKind::Added:      setFormat(0, length, m_added); break;
        case DiffLineKind::Removed:    setFormat(0, length, m_removed); break;
        case DiffLineKind::Context:    break;   // normal text color of the theme
        }
    }

private:
    std::vector<DiffLineKind> m_kinds;
    QTextCharFormat m_header;
    QTextCharFormat m_hunk;
    QTextCharFormat m_added;
    QTextCharFormat m_removed;
};

namespace {

// QTextDocument starts a new block at a lone '\\r' and at U+2029, which would shift every
// block number away from its patch line. Only '\\n' separates lines in a patch, so remove
// the carriage returns and neutralize the two separator characters.
QString displayText(const std::string &utf8) {
    QString text = QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
    text.remove(QLatin1Char('\r'));
    text.replace(QChar(0x2028), QChar(0xFFFD));
    text.replace(QChar(0x2029), QChar(0xFFFD));
    return text;
}

} // namespace

DiffViewerWidget::DiffViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_headerLabel = new QLabel(QStringLiteral("No Commit Selected"), this);
    m_headerLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));

    m_diffDisplay = new QTextEdit(this);
    m_diffDisplay->setReadOnly(true);
    m_diffDisplay->setUndoRedoEnabled(false);
    m_diffDisplay->setLineWrapMode(QTextEdit::NoWrap);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    m_diffDisplay->setFont(font);

    const bool darkTheme = palette().color(QPalette::Base).lightness() < 128;
    m_highlighter = new DiffHighlighter(m_diffDisplay->document(), darkTheme);

    layout->addWidget(m_headerLabel);
    layout->addWidget(m_diffDisplay, 1);
}

void DiffViewerWidget::showMessage(const QString &headerText, const QString &bodyText) {
    m_highlighter->setKinds({});
    m_headerLabel->setText(headerText);
    m_diffDisplay->setPlainText(bodyText);
}

void DiffViewerWidget::clear() {
    m_files.clear();
    showMessage(QStringLiteral("No Commit Selected"), QString());
}

void DiffViewerWidget::showLoading(const QString &shortHash) {
    m_files.clear();
    showMessage(QStringLiteral("Diff for commit ") + shortHash, QStringLiteral("Loading..."));
}

void DiffViewerWidget::setDetails(const CommitDetails &details) {
    m_files.clear();

    if (!details.ok()) {
        showMessage(QStringLiteral("Diff unavailable"), QString::fromStdString(details.error));
        return;
    }

    QString title = QStringLiteral("Diff for commit ") + QString::fromStdString(details.oid).left(7);
    if (details.parentOids.empty()) {
        title += QStringLiteral(" (first commit of the history)");
    } else if (details.parentOids.size() > 1) {
        title += QStringLiteral(" (merge: changes against the first parent)");
    }

    if (!details.patchError.empty()) {
        showMessage(title, QString::fromStdString(details.patchError));
        return;
    }
    if (details.patch.empty()) {
        showMessage(title, QStringLiteral("(No file changes in this commit)"));
        return;
    }

    QString text = displayText(details.patch);
    if (details.patchTruncated) {
        // These extra lines are past the end of the line kinds, so they stay uncolored.
        if (!text.endsWith(QLatin1Char('\n'))) {
            text += QLatin1Char('\n');
        }
        text += QStringLiteral("\n[Diff truncated: this commit is too large to show completely.]\n");
    }

    m_files = details.parsed.files;
    m_headerLabel->setText(title);

    // Detach while the text and the line kinds change, so the text is colored once, not twice.
    m_highlighter->setDocument(nullptr);
    m_diffDisplay->setPlainText(text);
    m_highlighter->setKinds(details.parsed.lineKinds);
    m_highlighter->setDocument(m_diffDisplay->document());

    m_diffDisplay->moveCursor(QTextCursor::Start);
}

void DiffViewerWidget::scrollToFile(int fileIndex) {
    if (fileIndex < 0 || static_cast<std::size_t>(fileIndex) >= m_files.size()) {
        return;
    }

    QTextDocument *document = m_diffDisplay->document();
    const QTextBlock block = document->findBlockByNumber(
        static_cast<int>(m_files[static_cast<std::size_t>(fileIndex)].firstLine));
    if (!block.isValid()) {
        return;
    }

    // The scroll bar of a QTextEdit counts pixels of the document, so the top of the block is the value.
    const QRectF rect = document->documentLayout()->blockBoundingRect(block);
    m_diffDisplay->verticalScrollBar()->setValue(static_cast<int>(rect.top()));
}

} // namespace Git
