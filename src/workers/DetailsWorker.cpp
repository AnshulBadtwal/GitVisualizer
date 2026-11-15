#include "workers/DetailsWorker.h"
#include "core/GitRepository.h"

namespace Git {

namespace {

// Rough size of a cached result in bytes. It only has to be consistent, not exact.
std::size_t detailsCost(const CommitDetails &d) {
    return sizeof(CommitDetails) + d.fullMessage.size() + d.patch.size() + d.parsed.lineKinds.size()
         + d.parsed.files.size() * sizeof(DiffFileSection) + 512;
}

} // namespace

DetailsWorker::DetailsWorker(QObject *parent)
    : QObject(parent)
{
}

void DetailsWorker::loadDetails(quint64 requestId, const QString &repoPath, const QString &oid) {
    // The person has already moved on to another commit: nobody is waiting for this one.
    if (requestId < m_latestRequest.load(std::memory_order_relaxed)) {
        return;
    }

    const std::string key = oid.toStdString();

    // A commit never changes, so a result computed earlier is still correct.
    if (const CommitDetails *cached = m_cache.find(key)) {
        emit detailsReady(requestId, *cached);
        return;
    }

    CommitDetails details;
    GitRepository repo;
    if (!repo.open(repoPath.toStdString())) {
        details.oid = key;
        details.error = repo.lastError();
        emit detailsReady(requestId, details);
        return;
    }

    if (!repo.loadCommitDetails(key, details, MaxPatchBytes)) {
        details.oid = key;
        details.error = repo.lastError();
        emit detailsReady(requestId, details);
        return;
    }

    // A patch that is missing because of the repository (shallow clone) can exist in another
    // clone of the same commit, so only complete results are kept.
    if (details.patchError.empty()) {
        m_cache.put(key, details, detailsCost(details));
    }
    emit detailsReady(requestId, details);
}

void DetailsWorker::clearCache() {
    m_cache.clear();
}

} // namespace Git
