#include <gtest/gtest.h>
#include "parsers/graph_parser.h"
#include "utils.h"
#include <string>
#include <cmath>
#include <filesystem>

// ============================================================
//  Graph Parser — graph1.dat structural tests
//
//  Path: <exe_dir>\missions\location0\level1\graph1.dat
// ============================================================

static std::string GraphPath() {
    return Utils::GetIGIRootPath() + "\\missions\\location0\\level1\\graphs\\graph1.dat";
}

class GraphParserTest : public ::testing::Test {
protected:
    GraphFile graph;
    void SetUp() override {
        graph = GRAPH_Parse(GraphPath());
    }
};

TEST_F(GraphParserTest, FileExistsAndParsesValid) {
    ASSERT_TRUE(graph.valid) << "Graph parse failed\nPath: " << GraphPath();
}

TEST_F(GraphParserTest, HasNodes) {
    EXPECT_GT(graph.nodes.size(), 0u);
}

TEST_F(GraphParserTest, AllNodeIdsNonNegative) {
    for (const auto& n : graph.nodes)
        EXPECT_GE(n.id, 0) << "Negative node ID: " << n.id;
}

TEST_F(GraphParserTest, AllCoordinatesAreFinite) {
    for (const auto& n : graph.nodes) {
        EXPECT_TRUE(std::isfinite(n.x)) << "Non-finite X for node " << n.id;
        EXPECT_TRUE(std::isfinite(n.y)) << "Non-finite Y for node " << n.id;
        EXPECT_TRUE(std::isfinite(n.z)) << "Non-finite Z for node " << n.id;
    }
}

TEST_F(GraphParserTest, AllMaterialValuesInRange) {
    for (const auto& n : graph.nodes)
        EXPECT_TRUE(n.material >= 0 && n.material <= 23)
            << "Material " << n.material << " out of 0-23 range for node " << n.id;
}

// ------------------------------------------------------------
//  GRAPH_Save round-trip tests
// ------------------------------------------------------------

static std::string TempOut(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

// graph1.dat is a sparse 1-node graph; the save round-trip tests need a graph
// with several nodes so "other nodes unchanged" is meaningful. graph7.dat in the
// same folder has 11 node records.
static std::string GraphPathMulti() {
    return Utils::GetIGIRootPath() + "\\missions\\location0\\level1\\graphs\\graph7.dat";
}

TEST_F(GraphParserTest, SaveRoundTripPersistsModifiedPosition) {
    GraphFile graph = GRAPH_Parse(GraphPathMulti());
    ASSERT_TRUE(graph.valid);
    ASSERT_GT(graph.nodes.size(), 1u);

    GraphFile edited = graph;
    const int targetId = edited.nodes[0].id;
    edited.nodes[0].x += 123.0;
    edited.nodes[0].y += 45.0;
    edited.nodes[0].z += 6.0;

    const std::string out = TempOut("igi_graph_roundtrip.dat");
    ASSERT_TRUE(GRAPH_Save(GraphPathMulti(), out, edited));

    GraphFile reloaded = GRAPH_Parse(out);
    ASSERT_TRUE(reloaded.valid);
    ASSERT_EQ(reloaded.nodes.size(), graph.nodes.size());

    const GraphNode* rn = nullptr;
    for (const auto& n : reloaded.nodes)
        if (n.id == targetId) { rn = &n; break; }
    ASSERT_NE(rn, nullptr);

    EXPECT_DOUBLE_EQ(rn->x, graph.nodes[0].x + 123.0);
    EXPECT_DOUBLE_EQ(rn->y, graph.nodes[0].y + 45.0);
    EXPECT_DOUBLE_EQ(rn->z, graph.nodes[0].z + 6.0);

    std::filesystem::remove(out);
}

// ------------------------------------------------------------
//  GRAPH_Write full-serializer round-trip
// ------------------------------------------------------------

TEST_F(GraphParserTest, WriteRoundTripPreservesNodesAndEdges) {
    GraphFile g = GRAPH_Parse(GraphPathMulti());
    ASSERT_TRUE(g.valid);
    ASSERT_GT(g.nodes.size(), 1u);

    const std::string out = TempOut("igi_graph_write.dat");
    ASSERT_TRUE(GRAPH_Write(GraphPathMulti(), out, g));

    GraphFile r = GRAPH_Parse(out);
    ASSERT_TRUE(r.valid);
    ASSERT_EQ(r.nodes.size(), g.nodes.size());
    ASSERT_EQ(r.edges.size(), g.edges.size());
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        EXPECT_EQ(r.nodes[i].id, g.nodes[i].id);
        EXPECT_DOUBLE_EQ(r.nodes[i].x, g.nodes[i].x);
        EXPECT_DOUBLE_EQ(r.nodes[i].y, g.nodes[i].y);
        EXPECT_DOUBLE_EQ(r.nodes[i].z, g.nodes[i].z);
        EXPECT_EQ(r.nodes[i].material, g.nodes[i].material);
        EXPECT_EQ(r.nodes[i].criteria, g.nodes[i].criteria);
    }
    std::filesystem::remove(out);
}

TEST_F(GraphParserTest, WriteSupportsAddingAndRemovingNodes) {
    GraphFile g = GRAPH_Parse(GraphPathMulti());
    ASSERT_TRUE(g.valid);
    const size_t origCount = g.nodes.size();

    // Remove the first node and add a brand-new one.
    const int removedId = g.nodes.front().id;
    g.nodes.erase(g.nodes.begin());
    GraphNode added; added.id = 999; added.x = 1.0; added.y = 2.0; added.z = 3.0;
    g.nodes.push_back(added);

    const std::string out = TempOut("igi_graph_addrem.dat");
    ASSERT_TRUE(GRAPH_Write(GraphPathMulti(), out, g));

    GraphFile r = GRAPH_Parse(out);
    ASSERT_TRUE(r.valid);
    EXPECT_EQ(r.nodes.size(), origCount);  // -1 +1
    EXPECT_EQ(GRAPH_FindNode(r, removedId), nullptr);
    const GraphNode* a = GRAPH_FindNode(r, 999);
    ASSERT_NE(a, nullptr);
    EXPECT_DOUBLE_EQ(a->x, 1.0);
    std::filesystem::remove(out);
}

// ------------------------------------------------------------
//  GRAPH_NodeKind classification (pure, no game file)
// ------------------------------------------------------------

static GraphNode NodeWithCriteria(const std::string& c) {
    GraphNode n; n.id = 1; n.criteria = c; return n;
}

TEST(GraphNodeKindTest, DoorCriteriaIsDoor) {
    EXPECT_EQ(GRAPH_NodeKind(NodeWithCriteria("NODECRITERIA_DOOR")), GraphNodeKind::Door);
}
TEST(GraphNodeKindTest, ViewCriteriaIsView) {
    EXPECT_EQ(GRAPH_NodeKind(NodeWithCriteria("NODECRITERIA_VIEW")), GraphNodeKind::View);
}
TEST(GraphNodeKindTest, StairCriteriaIsStair) {
    EXPECT_EQ(GRAPH_NodeKind(NodeWithCriteria("NODECRITERIA_STAIR")), GraphNodeKind::Stair);
}
TEST(GraphNodeKindTest, EmptyCriteriaIsDefault) {
    EXPECT_EQ(GRAPH_NodeKind(NodeWithCriteria("")), GraphNodeKind::Default);
}
TEST(GraphNodeKindTest, UnknownCriteriaIsDefault) {
    EXPECT_EQ(GRAPH_NodeKind(NodeWithCriteria("SOMETHING_ELSE")), GraphNodeKind::Default);
}
TEST(GraphNodeKindTest, DoorTakesPrecedenceOverView) {
    EXPECT_EQ(GRAPH_NodeKind(NodeWithCriteria("DOOR_VIEW")), GraphNodeKind::Door);
}

// ------------------------------------------------------------
//  GRAPH_FindNode lookup (pure, no game file)
// ------------------------------------------------------------

TEST(GraphFindNodeTest, FindsExistingId) {
    GraphFile g;
    GraphNode a; a.id = 10; GraphNode b; b.id = 25;
    g.nodes = { a, b };
    const GraphNode* found = GRAPH_FindNode(g, 25);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, 25);
}
TEST(GraphFindNodeTest, ReturnsNullForMissingId) {
    GraphFile g;
    GraphNode a; a.id = 10;
    g.nodes = { a };
    EXPECT_EQ(GRAPH_FindNode(g, 999), nullptr);
}

TEST_F(GraphParserTest, SaveLeavesOtherNodesUnchanged) {
    GraphFile graph = GRAPH_Parse(GraphPathMulti());
    ASSERT_TRUE(graph.valid);
    ASSERT_GT(graph.nodes.size(), 1u);

    GraphFile edited = graph;
    edited.nodes[0].x += 999.0;

    const std::string out = TempOut("igi_graph_unchanged.dat");
    ASSERT_TRUE(GRAPH_Save(GraphPathMulti(), out, edited));

    GraphFile reloaded = GRAPH_Parse(out);
    ASSERT_TRUE(reloaded.valid);
    ASSERT_EQ(reloaded.nodes.size(), graph.nodes.size());

    // Node at index 1 (untouched) must be byte-identical in position.
    EXPECT_DOUBLE_EQ(reloaded.nodes[1].x, graph.nodes[1].x);
    EXPECT_DOUBLE_EQ(reloaded.nodes[1].y, graph.nodes[1].y);
    EXPECT_DOUBLE_EQ(reloaded.nodes[1].z, graph.nodes[1].z);

    std::filesystem::remove(out);
}
