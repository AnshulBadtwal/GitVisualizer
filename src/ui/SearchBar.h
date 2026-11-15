#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QString>
#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;

namespace Git {

// The find bar above the commit graph: a text box, previous/next buttons and a short status
// ("3 of 17", "No matches"). It only reports what the person does; MainWindow runs the search.
class SearchBar : public QWidget {
    Q_OBJECT

public:
    /// How long typing must pause before queryChanged() is sent.
    static constexpr int DebounceMs = 250;

    explicit SearchBar(QWidget *parent = nullptr);
    ~SearchBar() override = default;

    [[nodiscard]] QString text() const;

    /// Puts the cursor in the text box and selects what is there, so typing replaces it.
    void focusInput();

    /// Short text next to the buttons, e.g. "3 of 17". Empty hides it.
    void setStatus(const QString &text);

    /// Tints the text box when the query matches nothing.
    void setNoMatch(bool noMatch);

signals:
    /// The text changed and typing paused, or Enter was pressed while a change was still waiting.
    void queryChanged(const QString &text);
    void nextRequested();
    void previousRequested();
    void closeRequested();

private:
    QLineEdit *m_input{nullptr};
    QToolButton *m_previous{nullptr};
    QToolButton *m_next{nullptr};
    QToolButton *m_close{nullptr};
    QLabel *m_status{nullptr};
    QTimer m_debounce;
};

} // namespace Git

#endif // SEARCHBAR_H
