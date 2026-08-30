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
    GraphLayoutEngine();
    ~GraphLayoutEngine() = default;

    /**
     * @brief Computes lane indices (X-coordinates) and connection paths for all commits.
     * @param nodes Vector of raw topological commits extracted from GitRepository.
     */
    void layout(std::vector<CommitNode>& nodes);

    /**
     * @brief Returns a consistent palette color for a given lane index.
     */
    static QColor colorForLane(int lane);

private:
    std::vector<QColor> m_laneColors;

    void initializeColors();
    int findFreeLane(const std::vector<bool>& activeLanes) const;
};

} // namespace Git

#endif // GRAPHLAYOUTENGINE_H
