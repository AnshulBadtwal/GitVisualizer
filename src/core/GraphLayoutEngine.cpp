#include "core/GraphLayoutEngine.h"
#include <algorithm>

namespace Git {

namespace {

// Marks a lane that is not waiting for any commit.
constexpr int kFreeLane = -1;

// Number of lanes in use (index of the last occupied lane + 1).
int activeLaneCount(const std::vector<int>& lanes) {
    for (size_t i = lanes.size(); i > 0; --i) {
        if (lanes[i - 1] != kFreeLane) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

// Assigns `targetRow` to lane `index`, growing the lane list if needed.
void setLane(std::vector<int>& lanes, int index, int targetRow) {
    const auto idx = static_cast<size_t>(index);
    if (idx >= lanes.size()) {
        lanes.resize(idx + 1, kFreeLane);
    }
    lanes[idx] = targetRow;
}

// Leftmost lane waiting for `targetRow`, or -1 if none does.
int findLaneWaitingFor(const std::vector<int>& lanes, int targetRow) {
    for (size_t i = 0; i < lanes.size(); ++i) {
        if (lanes[i] == targetRow) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

QColor GraphLayoutEngine::colorForLane(int lane) {
    static const std::vector<QColor> colors = {
        QColor(80, 160, 255),
        QColor(255, 107, 107),
        QColor(78, 205, 196),
        QColor(255, 190, 11),
        QColor(155, 89, 182),
        QColor(46, 204, 113),
        QColor(255, 121, 198),
        QColor(52, 152, 219),
        QColor(230, 126, 34),
        QColor(26, 188, 156)
    };
    if (lane < 0) return colors[0];
    return colors[static_cast<size_t>(lane) % colors.size()];
}

int GraphLayoutEngine::findFreeLane(const std::vector<int>& lanes, const std::vector<bool>& blocked) {
    // Lanes past the end of either vector count as free and unblocked, so this always returns.
    for (size_t i = 0;; ++i) {
        const bool isFree = i >= lanes.size() || lanes[i] == kFreeLane;
        const bool isBlocked = i < blocked.size() && blocked[i];
        if (isFree && !isBlocked) {
            return static_cast<int>(i);
        }
    }
}

void GraphLayoutEngine::layout(std::vector<CommitNode>& nodes) {
    if (nodes.empty()) {
        return;
    }

    const int rowCount = static_cast<int>(nodes.size());

    // Map OID -> row index in the nodes array
    std::unordered_map<std::string, int> oidToRow;
    oidToRow.reserve(nodes.size());
    for (int i = 0; i < rowCount; ++i) {
        oidToRow[nodes[static_cast<size_t>(i)].oid] = i;
    }

    // lanes[i] = row index of the commit that lane i is heading towards, or kFreeLane.
    // Several lanes may head to the same commit: that commit is a branch point (fork),
    // and the extra lanes join it when its row is reached.
    std::vector<int> lanes;
    int globalMaxLanes = 1;

    for (int row = 0; row < rowCount; ++row) {
        CommitNode& node = nodes[static_cast<size_t>(row)];

        // Start from a clean slate so layout() can be run again on the same nodes.
        node.connections.clear();
        node.incoming.clear();
        node.passThroughLanes.clear();

        const std::vector<int> lanesAbove = lanes;

        // 1. Which lanes arrive at this commit from above?
        std::vector<int> waiting;
        for (size_t i = 0; i < lanesAbove.size(); ++i) {
            if (lanesAbove[i] == row) {
                waiting.push_back(static_cast<int>(i));
            }
        }

        // 2. Commit lane: the leftmost arriving lane, or a fresh lane for a branch tip.
        const int nodeLane = waiting.empty() ? findFreeLane(lanes, {}) : waiting.front();
        node.lane = nodeLane;

        // 3. Top half of the row: arriving lines, and unrelated lanes passing straight through.
        for (const int w : waiting) {
            LaneSegment seg;
            seg.fromLane = w;
            seg.toLane = nodeLane;
            seg.pathType = (w == nodeLane) ? ConnectionPathType::Active : ConnectionPathType::Fork;
            seg.color = colorForLane(w);
            node.incoming.push_back(seg);
        }
        for (size_t i = 0; i < lanesAbove.size(); ++i) {
            if (lanesAbove[i] != kFreeLane && lanesAbove[i] != row) {
                node.passThroughLanes.push_back(static_cast<int>(i));
            }
        }

        // 4. Every lane that arrived here ends at this commit.
        for (const int w : waiting) {
            lanes[static_cast<size_t>(w)] = kFreeLane;
        }

        // Lanes used above this row (and the commit's own lane) are not handed out again to
        // a merge parent of this row, so a line that ends here is never mistaken for one
        // that starts here.
        std::vector<bool> blocked(std::max(lanesAbove.size(), static_cast<size_t>(nodeLane) + 1), false);
        for (size_t i = 0; i < lanesAbove.size(); ++i) {
            blocked[i] = lanesAbove[i] != kFreeLane;
        }
        blocked[static_cast<size_t>(nodeLane)] = true;

        // 5. Bottom half of the row: one connection per parent that is in the list.
        for (size_t p = 0; p < node.parentOids.size(); ++p) {
            const auto parentIt = oidToRow.find(node.parentOids[p]);
            // Parent not loaded (truncated history) or listed before its child: no line, no lane.
            if (parentIt == oidToRow.end() || parentIt->second <= row) {
                continue;
            }
            const int parentRow = parentIt->second;

            Connection conn;
            conn.targetRow = parentRow;

            if (p == 0) {
                // First parent: the branch simply continues in this commit's lane, even if
                // another lane is already heading to the same parent. They meet at the parent.
                setLane(lanes, nodeLane, parentRow);
                conn.targetLane = nodeLane;
                conn.pathType = ConnectionPathType::Active;
            } else {
                // Merge parent: join a lane already heading there, otherwise open a new one.
                int mergeLane = findLaneWaitingFor(lanes, parentRow);
                if (mergeLane < 0) {
                    mergeLane = findFreeLane(lanes, blocked);
                    setLane(lanes, mergeLane, parentRow);
                }
                conn.targetLane = mergeLane;
                conn.pathType = ConnectionPathType::Merge;
            }

            conn.color = colorForLane(conn.targetLane);
            node.connections.push_back(conn);
        }

        // 6. Widest point of this row, then drop unused lanes on the right.
        globalMaxLanes = std::max({globalMaxLanes, nodeLane + 1,
                                   activeLaneCount(lanesAbove), activeLaneCount(lanes)});
        lanes.resize(static_cast<size_t>(activeLaneCount(lanes)));
    }

    // Apply maxLanes across all nodes for uniform column scaling
    for (auto& node : nodes) {
        node.maxLanes = globalMaxLanes;
    }
}

} // namespace Git
