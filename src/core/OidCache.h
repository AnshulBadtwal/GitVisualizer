#ifndef OIDCACHE_H
#define OIDCACHE_H

#include <cstddef>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

// Cache keyed by a Git object id. An id is a hash of the object's content, so the value stored
// for it can never go stale: a changed commit would have a different id. That is why this cache
// needs no invalidation, only a size limit.
//
// Least-recently-used eviction, O(1) find and put (hash map + doubly linked list).
// Not thread-safe: use one instance per thread (see LogWorker and DiffViewerWidget).
// Qt-free on purpose, so it can be unit-tested without a QApplication.

namespace Git {

template <typename Value>
class OidCache {
public:
    /// @param maxCost Total cost allowed. The unit is yours (bytes, entries, ...); put() says
    ///                what each entry costs.
    explicit OidCache(std::size_t maxCost) : m_maxCost(maxCost) {}

    OidCache(const OidCache&) = delete;
    OidCache& operator=(const OidCache&) = delete;

    /// The cached value, or nullptr. A hit becomes the most recently used entry.
    /// The pointer stays valid until the next put(), erase() or clear().
    [[nodiscard]] const Value* find(const std::string& oid) {
        const auto it = m_index.find(oid);
        if (it == m_index.end()) {
            ++m_misses;
            return nullptr;
        }
        ++m_hits;
        m_entries.splice(m_entries.begin(), m_entries, it->second);   // keeps the element's address
        return &it->second->value;
    }

    /// Stores (or replaces) a value as the most recently used entry, then evicts the least
    /// recently used entries until the total cost fits. An entry that costs more than the
    /// whole cache is not stored.
    void put(const std::string& oid, Value value, std::size_t cost) {
        erase(oid);
        if (cost > m_maxCost) {
            return;
        }
        m_entries.push_front(Entry{oid, std::move(value), cost});
        m_index.insert_or_assign(oid, m_entries.begin());
        m_totalCost += cost;

        // The new entry fits by itself, so this loop stops before it would remove it.
        while (m_totalCost > m_maxCost) {
            const Entry& oldest = m_entries.back();
            m_totalCost -= oldest.cost;
            m_index.erase(oldest.oid);
            m_entries.pop_back();
        }
    }

    void erase(const std::string& oid) {
        const auto it = m_index.find(oid);
        if (it == m_index.end()) {
            return;
        }
        m_totalCost -= it->second->cost;
        m_entries.erase(it->second);
        m_index.erase(it);
    }

    void clear() {
        m_entries.clear();
        m_index.clear();
        m_totalCost = 0;
    }

    [[nodiscard]] std::size_t size() const { return m_entries.size(); }
    [[nodiscard]] std::size_t totalCost() const { return m_totalCost; }
    [[nodiscard]] std::size_t maxCost() const { return m_maxCost; }
    [[nodiscard]] std::size_t hits() const { return m_hits; }
    [[nodiscard]] std::size_t misses() const { return m_misses; }

private:
    struct Entry {
        std::string oid;
        Value value;
        std::size_t cost;
    };

    std::list<Entry> m_entries;   // front = most recently used
    std::unordered_map<std::string, typename std::list<Entry>::iterator> m_index;
    std::size_t m_maxCost;
    std::size_t m_totalCost{0};
    std::size_t m_hits{0};
    std::size_t m_misses{0};
};

} // namespace Git

#endif // OIDCACHE_H
