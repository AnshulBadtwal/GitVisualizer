#include "ui/SearchBar.h"
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QToolButton>

namespace Git {

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    m_input = new QLineEdit(this);
    m_input->setClearButtonEnabled(true);
    m_input->setPlaceholderText(QStringLiteral("Search commits"));
    m_input->setToolTip(QStringLiteral(
        "All words must match (subject, author, e-mail, branch/tag or start of the commit id).\n"
        "Fields:  author:name   msg:text   hash:1a2b   ref:name\n"
        "Enter / F3: next match    Shift+F3: previous match    Esc: close"));

    m_previous = new QToolButton(this);
    m_previous->setArrowType(Qt::UpArrow);
    m_previous->setAutoRaise(true);
    m_previous->setToolTip(QStringLiteral("Previous match (Shift+F3)"));

    m_next = new QToolButton(this);
    m_next->setArrowType(Qt::DownArrow);
    m_next->setAutoRaise(true);
    m_next->setToolTip(QStringLiteral("Next match (F3)"));

    m_status = new QLabel(this);

    m_close = new QToolButton(this);
    m_close->setText(QStringLiteral("x"));
    m_close->setAutoRaise(true);
    m_close->setToolTip(QStringLiteral("Close (Esc)"));

    layout->addWidget(m_input, 1);
    layout->addWidget(m_previous);
    layout->addWidget(m_next);
    layout->addWidget(m_status);
    layout->addWidget(m_close);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(DebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, [this]() { emit queryChanged(m_input->text()); });
    connect(m_input, &QLineEdit::textChanged, this, [this]() { m_debounce.start(); });

    // Enter right after typing runs the search at once; Enter again goes to the next match.
    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        if (m_debounce.isActive()) {
            m_debounce.stop();
            emit queryChanged(m_input->text());
        } else {
            emit nextRequested();
        }
    });

    connect(m_previous, &QToolButton::clicked, this, &SearchBar::previousRequested);
    connect(m_next, &QToolButton::clicked, this, &SearchBar::nextRequested);
    connect(m_close, &QToolButton::clicked, this, &SearchBar::closeRequested);

    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);   // only while the focus is inside the bar
    connect(escape, &QShortcut::activated, this, &SearchBar::closeRequested);
}

QString SearchBar::text() const {
    return m_input->text();
}

void SearchBar::focusInput() {
    m_input->setFocus();
    m_input->selectAll();
}

void SearchBar::setStatus(const QString &text) {
    m_status->setText(text);
}

void SearchBar::setNoMatch(bool noMatch) {
    m_input->setStyleSheet(noMatch ? QStringLiteral("QLineEdit { background-color: rgba(231, 76, 60, 70); }")
                                   : QString());
}

} // namespace Git
