#ifndef LOGWORKER_H
#define LOGWORKER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <vector>
#include <memory>
#include "core/GitTypes.h"

namespace Git {

class LogWorker : public QObject {
    Q_OBJECT

public:
    explicit LogWorker(QObject *parent = nullptr);
    ~LogWorker() override = default;

public slots:
    /**
     * @brief Background slot to parse repository log and run layout calculations.
     * @param repoPath Absolute path to the local Git repository directory.
     * @param maxCommits Maximum number of commits to fetch (0 for unlimited).
     */
    void processLog(const QString &repoPath, std::size_t maxCommits = 0);

signals:
    /**
     * @brief Emitted when log parsing and graph layout computation complete successfully.
     * @param nodes Fully populated vector of commit nodes with computed X/Y graph lanes.
     */
    void logReady(const std::vector<Git::CommitNode> &nodes);

    /**
     * @brief Emitted if opening the repository or traversing commits fails.
     * @param errorMessage Descriptive error message.
     */
    void errorOccurred(const QString &errorMessage);
};

} // namespace Git

#endif // LOGWORKER_H
