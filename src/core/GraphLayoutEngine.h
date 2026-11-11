#ifndef GRAPHLAYOUTENGINE_H
#define GRAPHLAYOUTENGINE_H

#include <vector>
#include <unordered_map>
#include <string>
#include <QColor>
#include "core/GitTypes.h"

namespace Git {

class GraphLayoutEngine {
public:
    GraphLayoutEngine() = default;
    ~GraphLayoutEngine() = default;

    /**
     * @brief Computes lane indices (X-coordinates) and per-row line segments for all commits.
     *
     * Input must be in topological order (children before parents), as produced by
     * GitRepository::fetchCommitHistory. Every row ends up with everything needed to draw
     * itself without looking at other rows:
     *   - lane              : the column of the commit dot
     *   - incoming          : lines arriving from the row above (including branches joining)
     *   - passThroughLanes  : unrelated branches running straight through the row
     *   - connections       : lines leaving towards the parents
     *
     * Calling layout() again on the same nodes recomputes everything from scratch.
     * Parents missing from the list (truncated history) or listed before their child
     * get no connection and no lane.
     *
     * @param nodes Vector of raw topological commits extracted from GitRepository.
     */
    void layout(std::vector<CommitNode>& nodes);

    /**
     * @brief Returns a consistent palette color for a given lane index.
     */
    static QColor colorForLane(int lane);

private:
    /// Lowest lane that is free in `lanes` and not marked in `blocked`.
    static int findFreeLane(const std::vector<int>& lanes, const std::vector<bool>& blocked);
};

} // namespace Git

#endif // GRAPHLAYOUTENGINE_H
