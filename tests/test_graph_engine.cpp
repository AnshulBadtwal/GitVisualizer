#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "core/GitTypes.h"
#include "core/GraphLayoutEngine.h"

namespace Git {

class GraphLayoutEngineTest : public ::testing::Test {
protected:
    GraphLayoutEngine engine;

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

} // namespace Git
