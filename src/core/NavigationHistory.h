#ifndef NAVIGATIONHISTORY_H
#define NAVIGATIONHISTORY_H

#include <functional>
#include <optional>
#include <string>
#include "core/CustomContainers.h"

// Back / forward through the commits the person has looked at, like the buttons of a web
// browser. Two stacks do the work (CustomStack, the hand-written linked-list stack):
//
//     m_back      commits visited before the current one; the top is the most recent
//     m_forward   commits left by going back; the top is the one that Forward returns to
//
// Visiting a new commit pushes the current one on `m_back` and empties `m_forward`
// (exactly what a browser does when you follow a link after going back).
// Qt-free, so it is unit-tested on its own.

namespace Git {

class NavigationHistory {
public:
    /// Tells whether a commit can still be shown (for example: it is still in the loaded history).
    using Exists = std::function<bool(const std::string &oid)>;

    /// The person selected a commit. Selecting the commit that is already current changes nothing.
    void visit(const std::string &oid);

    [[nodiscard]] bool canGoBack() const { return !m_back.isEmpty(); }
    [[nodiscard]] bool canGoForward() const { return !m_forward.isEmpty(); }

    /// Moves one step back and returns that commit, or nullopt if there is none. Entries for
    /// which `exists` returns false (a commit that a reload removed) are dropped on the way.
    /// An empty `exists` accepts everything.
    [[nodiscard]] std::optional<std::string> back(const Exists &exists = {});

    /// Like back(), in the other direction.
    [[nodiscard]] std::optional<std::string> forward(const Exists &exists = {});

    /// The commit the history is standing on; empty before the first visit.
    [[nodiscard]] const std::string &current() const { return m_current; }

    /// Forgets everything (another repository was opened).
    void clear();

private:
    CustomStack<std::string> m_back;
    CustomStack<std::string> m_forward;
    std::string m_current;
};

} // namespace Git

#endif // NAVIGATIONHISTORY_H
