#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <set>
#include <random>
#include <algorithm>
#include "core/GitTypes.h"
#include "core/GraphLayoutEngine.h"

namespace Git {

class GraphLayoutEngineTest : public ::testing::Test {
protected:
    GraphLayoutEngine engine;

    // 40-character id made of one repeated character, e.g. id('a') -> "aaaa...a"
    static std::string id(char c) { return std::string(40, c); }

    static int countType(const std::vector<LaneSegment>& segments, ConnectionPathType type) {
        int n = 0;
        for (const auto& seg : segments) {
            if (seg.pathType == type) ++n;
        }
        return n;
    }

    // Lanes touched by the top edge / bottom edge of a row. A line that leaves the bottom of one
    // row must arrive at the top of the next, otherwise the delegate would draw lines in mid-air.
    static std::set<int> topLanes(const CommitNode& node) {
        std::set<int> lanes(node.passThroughLanes.begin(), node.passThroughLanes.end());
        for (const auto& seg : node.incoming) lanes.insert(seg.fromLane);
        return lanes;
    }

    static std::set<int> bottomLanes(const CommitNode& node) {
        std::set<int> lanes(node.passThroughLanes.begin(), node.passThroughLanes.end());
        for (const auto& conn : node.connections) lanes.insert(conn.targetLane);
        return lanes;
    }

    static void expectRowsLineUp(const std::vector<CommitNode>& nodes) {
        ASSERT_TRUE(!nodes.empty());
        EXPECT_TRUE(topLanes(nodes.front()).empty());
        EXPECT_TRUE(bottomLanes(nodes.back()).empty());
        for (size_t i = 0; i + 1 < nodes.size(); ++i) {
            EXPECT_EQ(bottomLanes(nodes[i]), topLanes(nodes[i + 1]));
        }
        for (const auto& node : nodes) {
            EXPECT_TRUE(node.lane >= 0 && node.lane < node.maxLanes);
            for (const int lane : topLanes(node)) EXPECT_TRUE(lane >= 0 && lane < node.maxLanes);
            for (const int lane : bottomLanes(node)) EXPECT_TRUE(lane >= 0 && lane < node.maxLanes);
        }
    }

    // Shared topology, topological order:
    //   row 0: M (merge) -> parents C2 (first), F1
    //   row 1: F1 -> C1
    //   row 2: C2 -> C1
    //   row 3: C1 (root)
    std::vector<CommitNode> mergeShape() {
        return {
            createMockCommit(id('m'), {id('2'), id('f')}),
            createMockCommit(id('f'), {id('1')}),
            createMockCommit(id('2'), {id('1')}),
            createMockCommit(id('1'), {})
        };
    }

    // Helper to generate a mock commit node
    CommitNode createMockCommit(const std::string& oid, const std::vector<std::string>& parentOids) {
        CommitNode node;
        node.oid = oid;
        node.shortHash = oid.substr(0, 7);
        node.message = "Mock commit " + node.shortHash;
        node.author = "Test Engineer";
        node.email = "test@example.com";
        node.timestamp = 1700000000;
        node.parentOids = parentOids;
        return node;
    }
};

// 1. Linear Commit History (Single Branch)
TEST_F(GraphLayoutEngineTest, LinearHistoryStaysInSingleLane) {
    std::vector<CommitNode> nodes = {
        createMockCommit("c333333333333333333333333333333333333333", {"c222222222222222222222222222222222222222"}),
        createMockCommit("c222222222222222222222222222222222222222", {"c111111111111111111111111111111111111111"}),
        createMockCommit("c111111111111111111111111111111111111111", {})
    };

    engine.layout(nodes);

    ASSERT_EQ(nodes.size(), 3u);

    // All commits in a straight sequence must share lane 0
    EXPECT_EQ(nodes[0].lane, 0);
    EXPECT_EQ(nodes[1].lane, 0);
    EXPECT_EQ(nodes[2].lane, 0);

    // Max lanes across the entire graph should be 1
    EXPECT_EQ(nodes[0].maxLanes, 1);
    EXPECT_EQ(nodes[1].maxLanes, 1);
    EXPECT_EQ(nodes[2].maxLanes, 1);

    // Verify outward connections
    ASSERT_EQ(nodes[0].connections.size(), 1u);
    EXPECT_EQ(nodes[0].connections[0].targetRow, 1);
    EXPECT_EQ(nodes[0].connections[0].targetLane, 0);
    EXPECT_EQ(nodes[0].connections[0].pathType, ConnectionPathType::Active);
}

// 2. Simple Branch Split and Merge
TEST_F(GraphLayoutEngineTest, MergeCommitAllocatesSecondaryLane) {
    // History topology (topological order):
    // Row 0: Merge commit (M) -> parents: Main (C2), Feature (F1)
    // Row 1: Feature commit (F1) -> parent: Root (C1)
    // Row 2: Main commit (C2) -> parent: Root (C1)
    // Row 3: Root commit (C1)
    std::vector<CommitNode> nodes = {
        createMockCommit("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", {"c222222222222222222222222222222222222222", "ffffffffffffffffffffffffffffffffffffffff"}),
        createMockCommit("ffffffffffffffffffffffffffffffffffffffff", {"c111111111111111111111111111111111111111"}),
        createMockCommit("c222222222222222222222222222222222222222", {"c111111111111111111111111111111111111111"}),
        createMockCommit("c111111111111111111111111111111111111111", {})
    };

    engine.layout(nodes);

    ASSERT_EQ(nodes.size(), 4u);

    // Merge commit should be on main trunk lane (0)
    EXPECT_EQ(nodes[0].lane, 0);

    // Merge commit must have 2 connections (Primary + Merge connection)
    ASSERT_EQ(nodes[0].connections.size(), 2u);
    EXPECT_EQ(nodes[0].connections[0].pathType, ConnectionPathType::Active);
    EXPECT_EQ(nodes[0].connections[1].pathType, ConnectionPathType::Merge);

    // Secondary parent lane must differ from main lane
    EXPECT_NE(nodes[0].connections[1].targetLane, nodes[0].lane);

    // Graph requires at least 2 distinct lanes
    EXPECT_GE(nodes[0].maxLanes, 2);
}

// 3. Lane Color Utility Test
TEST_F(GraphLayoutEngineTest, ColorForLaneReturnsValidColors) {
    QColor color0 = GraphLayoutEngine::colorForLane(0);
    QColor color1 = GraphLayoutEngine::colorForLane(1);
    QColor color10 = GraphLayoutEngine::colorForLane(10); // Wraps around palette

    EXPECT_TRUE(color0.isValid());
    EXPECT_TRUE(color1.isValid());
    EXPECT_TRUE(color10.isValid());
    EXPECT_NE(color0, color1);
    EXPECT_EQ(color0, color10); // 10 % 10 == 0
}

// ---------------------------------------------------------------------------
// Phase 1 tests
// ---------------------------------------------------------------------------

// 4. Empty input is a harmless no-op
TEST_F(GraphLayoutEngineTest, EmptyInputIsANoOp) {
    std::vector<CommitNode> nodes;
    engine.layout(nodes);
    EXPECT_TRUE(nodes.empty());
}

// 5. A lone root commit
TEST_F(GraphLayoutEngineTest, SingleCommitHasNoLines) {
    std::vector<CommitNode> nodes = { createMockCommit(id('a'), {}) };
    engine.layout(nodes);

    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].lane, 0);
    EXPECT_EQ(nodes[0].maxLanes, 1);
    EXPECT_TRUE(nodes[0].connections.empty());
    EXPECT_TRUE(nodes[0].incoming.empty());
    EXPECT_TRUE(nodes[0].passThroughLanes.empty());
}

// 6. A branch tip has no line above its dot; a continuing commit does
TEST_F(GraphLayoutEngineTest, IncomingSegmentsFollowTheHistory) {
    std::vector<CommitNode> nodes = {
        createMockCommit(id('3'), {id('2')}),
        createMockCommit(id('2'), {id('1')}),
        createMockCommit(id('1'), {})
    };
    engine.layout(nodes);

    EXPECT_TRUE(nodes[0].incoming.empty());
    ASSERT_EQ(nodes[1].incoming.size(), 1u);
    EXPECT_EQ(nodes[1].incoming[0].fromLane, 0);
    EXPECT_EQ(nodes[1].incoming[0].toLane, 0);
    EXPECT_EQ(nodes[1].incoming[0].pathType, ConnectionPathType::Active);
}

// 7. The convergence bug: both branches must meet at the root, and the trunk stays straight
TEST_F(GraphLayoutEngineTest, ConvergingBranchesMeetAtTheParent) {
    auto nodes = mergeShape();
    engine.layout(nodes);

    ASSERT_EQ(nodes.size(), 4u);

    // Trunk stays in lane 0 all the way down, including the root
    EXPECT_EQ(nodes[2].lane, 0);
    EXPECT_EQ(nodes[3].lane, 0);
    EXPECT_EQ(nodes[2].connections[0].targetLane, nodes[3].lane);

    // The side branch lives in lane 1 and joins the root at the bottom
    EXPECT_EQ(nodes[1].lane, 1);
    EXPECT_EQ(nodes[1].connections[0].targetLane, 1);
    ASSERT_EQ(nodes[3].incoming.size(), 2u);
    EXPECT_EQ(countType(nodes[3].incoming, ConnectionPathType::Active), 1);
    ASSERT_EQ(countType(nodes[3].incoming, ConnectionPathType::Fork), 1);
    for (const auto& seg : nodes[3].incoming) {
        if (seg.pathType == ConnectionPathType::Fork) {
            EXPECT_EQ(seg.fromLane, 1);
            EXPECT_EQ(seg.toLane, 0);
        }
    }
}

// 8. Parallel branches are recorded as pass-through lanes (this is what fixes long edges)
TEST_F(GraphLayoutEngineTest, ParallelBranchesRecordPassThroughLanes) {
    auto nodes = mergeShape();
    engine.layout(nodes);

    EXPECT_TRUE(nodes[0].passThroughLanes.empty());
    EXPECT_EQ(nodes[1].passThroughLanes, (std::vector<int>{0}));
    EXPECT_EQ(nodes[2].passThroughLanes, (std::vector<int>{1}));
    EXPECT_TRUE(nodes[3].passThroughLanes.empty());
}

// 9. Two children of one commit (a branch point)
TEST_F(GraphLayoutEngineTest, BranchPointJoinsBothChildren) {
    std::vector<CommitNode> nodes = {
        createMockCommit(id('a'), {id('p')}),
        createMockCommit(id('b'), {id('p')}),
        createMockCommit(id('p'), {})
    };
    engine.layout(nodes);

    EXPECT_EQ(nodes[0].lane, 0);
    EXPECT_EQ(nodes[1].lane, 1);
    EXPECT_EQ(nodes[2].lane, 0);
    EXPECT_EQ(nodes[1].passThroughLanes, (std::vector<int>{0}));
    EXPECT_EQ(nodes[2].maxLanes, 2);
    ASSERT_EQ(nodes[2].incoming.size(), 2u);
    EXPECT_EQ(countType(nodes[2].incoming, ConnectionPathType::Fork), 1);
}

// 10. Octopus merge: one lane per parent
TEST_F(GraphLayoutEngineTest, OctopusMergeUsesOneLanePerParent) {
    std::vector<CommitNode> nodes = {
        createMockCommit(id('m'), {id('a'), id('b'), id('c')}),
        createMockCommit(id('a'), {}),
        createMockCommit(id('b'), {}),
        createMockCommit(id('c'), {})
    };
    engine.layout(nodes);

    ASSERT_EQ(nodes[0].connections.size(), 3u);
    EXPECT_EQ(nodes[0].connections[0].pathType, ConnectionPathType::Active);
    EXPECT_EQ(nodes[0].connections[1].pathType, ConnectionPathType::Merge);
    EXPECT_EQ(nodes[0].connections[2].pathType, ConnectionPathType::Merge);

    EXPECT_EQ(nodes[0].connections[0].targetLane, 0);
    EXPECT_EQ(nodes[0].connections[1].targetLane, 1);
    EXPECT_EQ(nodes[0].connections[2].targetLane, 2);

    EXPECT_EQ(nodes[1].lane, 0);
    EXPECT_EQ(nodes[2].lane, 1);
    EXPECT_EQ(nodes[3].lane, 2);
    EXPECT_EQ(nodes[0].maxLanes, 3);
}

// 11. Truncated history (limit hit): missing parents leave no line and no phantom lane
TEST_F(GraphLayoutEngineTest, MissingParentLeavesNoConnectionAndNoLane) {
    std::vector<CommitNode> nodes = {
        createMockCommit(id('a'), {id('b'), id('x')}),   // 'x' was cut off
        createMockCommit(id('b'), {id('y')})             // 'y' was cut off
    };
    engine.layout(nodes);

    ASSERT_EQ(nodes[0].connections.size(), 1u);
    EXPECT_EQ(nodes[0].connections[0].targetRow, 1);
    EXPECT_TRUE(nodes[1].connections.empty());
    EXPECT_EQ(nodes[0].maxLanes, 1);
}

// 12. Wrongly ordered input must not leave a lane waiting forever
TEST_F(GraphLayoutEngineTest, ParentListedBeforeChildIsIgnored) {
    std::vector<CommitNode> nodes = {
        createMockCommit(id('p'), {}),
        createMockCommit(id('c'), {id('p')})
    };
    engine.layout(nodes);

    EXPECT_TRUE(nodes[1].connections.empty());
    EXPECT_EQ(nodes[0].maxLanes, 1);
}

// 13. Running layout() twice gives the same result, not doubled lines
TEST_F(GraphLayoutEngineTest, LayoutTwiceGivesTheSameResult) {
    auto nodes = mergeShape();
    engine.layout(nodes);
    auto firstRun = nodes;
    engine.layout(nodes);

    ASSERT_EQ(nodes.size(), firstRun.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        EXPECT_EQ(nodes[i].lane, firstRun[i].lane);
        EXPECT_EQ(nodes[i].connections.size(), firstRun[i].connections.size());
        EXPECT_EQ(nodes[i].incoming.size(), firstRun[i].incoming.size());
        EXPECT_EQ(nodes[i].passThroughLanes, firstRun[i].passThroughLanes);
    }
}

// 14. Line colors follow the lane they are drawn in
TEST_F(GraphLayoutEngineTest, LineColorsFollowTheirLane) {
    auto nodes = mergeShape();
    engine.layout(nodes);

    for (const auto& node : nodes) {
        for (const auto& conn : node.connections) {
            EXPECT_EQ(conn.color, GraphLayoutEngine::colorForLane(conn.targetLane));
        }
        for (const auto& seg : node.incoming) {
            EXPECT_EQ(seg.color, GraphLayoutEngine::colorForLane(seg.fromLane));
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 2 contract: each row can be drawn on its own
// ---------------------------------------------------------------------------

// 15. Hand-made shapes: every line that leaves a row arrives at the next one
TEST_F(GraphLayoutEngineTest, RowEdgesLineUpOnKnownShapes) {
    auto merge = mergeShape();
    engine.layout(merge);
    expectRowsLineUp(merge);

    std::vector<CommitNode> octopus = {
        createMockCommit(id('m'), {id('a'), id('b'), id('c')}),
        createMockCommit(id('a'), {}),
        createMockCommit(id('b'), {}),
        createMockCommit(id('c'), {})
    };
    engine.layout(octopus);
    expectRowsLineUp(octopus);

    std::vector<CommitNode> fork = {
        createMockCommit(id('a'), {id('p')}),
        createMockCommit(id('b'), {id('p')}),
        createMockCommit(id('p'), {})
    };
    engine.layout(fork);
    expectRowsLineUp(fork);
}

// 16. Random histories (fixed seed): merges, forks, several roots
TEST_F(GraphLayoutEngineTest, RowEdgesLineUpOnRandomHistories) {
    std::mt19937 rng(12345);

    for (int round = 0; round < 60; ++round) {
        const int count = 30 + static_cast<int>(rng() % 170);

        std::vector<CommitNode> nodes;
        for (int i = 0; i < count; ++i) {
            std::vector<std::string> parents;
            const int rowsBelow = count - 1 - i;
            if (rowsBelow > 0 && rng() % 20 != 0) {            // about 5% are extra roots
                int wanted = 1;
                if (rng() % 6 == 0) wanted = 2 + static_cast<int>(rng() % 2);   // merges / octopus
                const int window = std::min(rowsBelow, 12);     // parents are usually close by
                for (int attempt = 0; attempt < wanted * 3 && static_cast<int>(parents.size()) < wanted; ++attempt) {
                    const std::string parent = "c" + std::to_string(i + 1 + static_cast<int>(rng() % static_cast<unsigned>(window)));
                    if (std::find(parents.begin(), parents.end(), parent) == parents.end()) {
                        parents.push_back(parent);
                    }
                }
            }
            nodes.push_back(createMockCommit("c" + std::to_string(i), parents));
        }

        engine.layout(nodes);
        expectRowsLineUp(nodes);
    }
}

} // namespace Git
