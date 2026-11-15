#include "core/NavigationHistory.h"
#include <utility>

namespace Git {

namespace {

// Pops `from` until it finds a commit that still exists. Entries that no longer exist are dropped.
std::optional<std::string> popValid(CustomStack<std::string> &from, const NavigationHistory::Exists &exists) {
    while (!from.isEmpty()) {
        std::string candidate = std::move(from.top());
        from.pop();
        if (!exists || exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace

void NavigationHistory::visit(const std::string &oid) {
    if (oid.empty() || oid == m_current) {
        return;
    }
    if (!m_current.empty()) {
        m_back.push(m_current);
    }
    m_current = oid;
    m_forward.clear();   // a new path: what Forward used to lead to is gone
}

std::optional<std::string> NavigationHistory::back(const Exists &exists) {
    std::optional<std::string> target = popValid(m_back, exists);
    if (!target) {
        return std::nullopt;
    }
    m_forward.push(m_current);   // m_current is never empty while m_back had entries
    m_current = *target;
    return target;
}

std::optional<std::string> NavigationHistory::forward(const Exists &exists) {
    std::optional<std::string> target = popValid(m_forward, exists);
    if (!target) {
        return std::nullopt;
    }
    m_back.push(m_current);
    m_current = *target;
    return target;
}

void NavigationHistory::clear() {
    m_back.clear();
    m_forward.clear();
    m_current.clear();
}

} // namespace Git
