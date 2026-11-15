#ifndef DETAILSWORKER_H
#define DETAILSWORKER_H

#include <QObject>
#include <QString>
#include <atomic>
#include "core/GitTypes.h"
#include "core/OidCache.h"

namespace Git {

// Reads one commit (message, people, parents, patch, changed files) on a worker thread, so a
// huge commit cannot freeze the window. It owns a cache of finished results by commit id; the
// cache is only touched by this object's thread, so it needs no lock.
class DetailsWorker : public QObject {
    Q_OBJECT

public:
    /// The patch of one commit is cut after about this many bytes (see CommitDetails::patchTruncated).
    static constexpr std::size_t MaxPatchBytes = 2u * 1024u * 1024u;

    explicit DetailsWorker(QObject *parent = nullptr);
    ~DetailsWorker() override = default;

    /// Call from the GUI thread, right before sending a request: requests with a smaller id are
    /// skipped when their turn comes, so scrolling quickly through commits does not queue up work.
    void setLatestRequest(quint64 requestId) { m_latestRequest.store(requestId, std::memory_order_relaxed); }

public slots:
    void loadDetails(quint64 requestId, const QString &repoPath, const QString &oid);

    /// Forgets every cached commit (frees memory; entries never go stale, so this is optional).
    void clearCache();

signals:
    /// `requestId` is the id of the request this answers. `details.ok()` is false if the commit
    /// could not be read.
    void detailsReady(quint64 requestId, const Git::CommitDetails &details);

private:
    std::atomic<quint64> m_latestRequest{0};
    OidCache<CommitDetails> m_cache{64u * 1024u * 1024u};
};

} // namespace Git

#endif // DETAILSWORKER_H
