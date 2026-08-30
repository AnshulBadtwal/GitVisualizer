#include "core/GraphLayoutEngine.h"
#include <algorithm>
#include <map>

namespace Git {

GraphLayoutEngine::GraphLayoutEngine() {
    initializeColors();
}

void GraphLayoutEngine::initializeColors() {
    // Distinct, vibrant palette optimized for both dark and light themes
    m_laneColors = {
        QColor(80, 160, 255),  // Bright Blue (Main)
        QColor(255, 107, 107), // Coral Red
        QColor(78, 205, 196),  // Teal
        QColor(255, 190, 11),  // Amber Yellow
        QColor(155, 89, 182),  // Soft Purple
        QColor(46, 204, 113),  // Emerald Green
        QColor(255, 121, 198), // Pink
        QColor(52, 152, 219),  // Cerulean
        QColor(230, 126, 34),  // Orange
        QColor(26, 188, 156)   // Mint
    };
}

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

int GraphLayoutEngine::findFreeLane(const std::vector<bool>& activeLanes) const {
    for (size_t i = 0; i < activeLanes.size(); ++i) {
        if (!activeLanes[i]) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(activeLanes.size());
}

void GraphLayoutEngine::layout(std::vector<CommitNode>& nodes) {
    if (nodes.empty()) {
        return;
    }

    // Map OID -> row index in the nodes array
    std::unordered_map<std::string, int> oidToRow;
    oidToRow.reserve(nodes.size());
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        oidToRow[nodes[static_cast<size_t>(i)].oid] = i;
    }

    // Active tracking lanes: lane index -> OID expected next in this lane
    std::map<int, std::string> activeLaneToOid;
    
    // Track maximum simultanous lanes allocated across graph
    int globalMaxLanes = 1;

    for (int currentRow = 0; currentRow < static_cast<int>(nodes.size()); ++currentRow) {
        auto& currentNode = nodes[static_cast<size_t>(currentRow)];
        int assignedLane = -1;

        // 1. Check if an active lane is already waiting for this commit
        for (auto const& [lane, expectedOid] : activeLaneToOid) {
            if (expectedOid == currentNode.oid) {
                assignedLane = lane;
                break;
            }
        }

        // 2. If no existing lane claims this node, allocate a new free lane
        if (assignedLane == -1) {
            std::vector<bool> occupied(activeLaneToOid.empty() ? 0 : activeLaneToOid.rbegin()->first + 1, false);
            for (auto const& [lane, oid] : activeLaneToOid) {
                if (lane >= 0 && lane < static_cast<int>(occupied.size())) {
                    occupied[static_cast<size_t>(lane)] = true;
                }
            }
            assignedLane = findFreeLane(occupied);
        }

        currentNode.lane = assignedLane;
        
        // Remove this commit from the active lane tracking map since it's now processed
        activeLaneToOid.erase(assignedLane);

        // 3. Process parents and construct outward Connection links
        if (!currentNode.parentOids.empty()) {
            // First parent continues on the primary assigned lane
            const std::string& firstParentOid = currentNode.parentOids[0];
            
            // Check if first parent is already being waited on by another active branch
            bool firstParentHasLane = false;
            for (auto const& [lane, expectedOid] : activeLaneToOid) {
                if (expectedOid == firstParentOid) {
                    firstParentHasLane = true;
                    break;
                }
            }

            if (!firstParentHasLane) {
                activeLaneToOid[assignedLane] = firstParentOid;
            }

            // Record parent connections
            for (size_t parentIdx = 0; parentIdx < currentNode.parentOids.size(); ++parentIdx) {
                const std::string& parentOid = currentNode.parentOids[parentIdx];
                auto parentIt = oidToRow.find(parentOid);

                if (parentIt != oidToRow.end()) {
                    int parentRow = parentIt->second;
                    
                    Connection conn;
                    conn.targetRow = parentRow;

                    if (parentIdx == 0) {
                        // Main trunk line or straight continuation
                        conn.targetLane = assignedLane;
                        conn.pathType = ConnectionPathType::Active;
                        conn.color = colorForLane(assignedLane);
                    } else {
                        // Merge commit: secondary parent branching in from another lane
                        int secondaryLane = -1;
                        for (auto const& [lane, expectedOid] : activeLaneToOid) {
                            if (expectedOid == parentOid) {
                                secondaryLane = lane;
                                break;
                            }
                        }

                        if (secondaryLane == -1) {
                            std::vector<bool> occupied(activeLaneToOid.empty() ? 0 : activeLaneToOid.rbegin()->first + 1, false);
                            for (auto const& [lane, oid] : activeLaneToOid) {
                                if (lane >= 0 && lane < static_cast<int>(occupied.size())) {
                                    occupied[static_cast<size_t>(lane)] = true;
                                }
                            }
                            secondaryLane = findFreeLane(occupied);
                            activeLaneToOid[secondaryLane] = parentOid;
                        }

                        conn.targetLane = secondaryLane;
                        conn.pathType = ConnectionPathType::Merge;
                        conn.color = colorForLane(secondaryLane);
                    }

                    currentNode.connections.push_back(conn);
                }
            }
        }

        // Keep track of total active lanes for layout rendering bounds
        int currentMax = activeLaneToOid.empty() ? assignedLane + 1 : std::max(assignedLane + 1, activeLaneToOid.rbegin()->first + 1);
        globalMaxLanes = std::max(globalMaxLanes, currentMax);
    }

    // Apply maxLanes across all nodes for uniform column scaling
    for (auto& node : nodes) {
        node.maxLanes = globalMaxLanes;
    }
}

} // namespace Git
