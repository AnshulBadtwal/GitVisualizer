#include "workers/LogWorker.h"
#include "core/GitRepository.h"
#include "core/GraphLayoutEngine.h"
#include <QDebug>
#include <QMetaType>
#include <QString>

namespace Git {

LogWorker::LogWorker(QObject *parent)
    : QObject(parent)
{
}

void LogWorker::processLog(const QString &repoPath, std::size_t maxCommits) {
    if (repoPath.isEmpty()) {
        emit errorOccurred(QStringLiteral("Repository path cannot be empty."));
        return;
    }

    GitRepository repo;
    if (!repo.open(repoPath.toStdString())) {
        emit errorOccurred(QString::fromStdString(repo.lastError()));
        return;
    }
    repo.setCommitCache(&m_commitCache);

    // 1. Asynchronously fetch raw commits via libgit2 revwalk
    std::vector<CommitNode> nodes;
    if (!repo.fetchCommitHistory(nodes, maxCommits)) {
        emit errorOccurred(QString::fromStdString(repo.lastError()));
        return;
    }
    qDebug() << "commit cache:" << m_commitCache.hits() << "hits," << m_commitCache.misses() << "misses";
    if (nodes.empty()) {
        // A repository without commits is valid, not an error.
        emit logReady(nodes);
        return;
    }

    // 2. Compute visual lane assignments and topological DAG connections
    GraphLayoutEngine layoutEngine;
    layoutEngine.layout(nodes);

    // 3. Emit populated graph data across QThread boundary back to main UI thread
    emit logReady(nodes);
}

} // namespace Git
