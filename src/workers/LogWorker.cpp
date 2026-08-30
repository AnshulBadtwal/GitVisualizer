#include "workers/LogWorker.h"
#include "core/GitRepository.h"
#include "core/GraphLayoutEngine.h"
#include <QMetaType>
#include <QStringLiteral>

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
        emit errorOccurred(QString::fromUtf8("Failed to open Git repository at: ") + repoPath);
        return;
    }

    // 1. Asynchronously fetch raw commits via libgit2 revwalk
    std::vector<CommitNode> nodes = repo.fetchCommitHistory(maxCommits);
    if (nodes.empty()) {
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
