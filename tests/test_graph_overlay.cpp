#include <gtest/gtest.h>
#include "renderer/graph_overlay.h"
#include <glm/glm.hpp>

// ============================================================
//  GraphWorldToScreen — pure projection math
// ============================================================

TEST(GraphWorldToScreenTest, OriginProjectsToViewportCenter) {
    glm::vec2 out{};
    bool vis = GraphWorldToScreen(glm::mat4(1.0f), glm::vec3(0.0f), 800.0f, 600.0f, out);
    ASSERT_TRUE(vis);
    EXPECT_FLOAT_EQ(out.x, 400.0f);
    EXPECT_FLOAT_EQ(out.y, 300.0f);
}

TEST(GraphWorldToScreenTest, NdcTopRightMapsToScreenTopRight) {
    // With identity view-proj, world (1,1,0) is NDC (1,1) -> top-right pixel.
    glm::vec2 out{};
    bool vis = GraphWorldToScreen(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 0.0f), 800.0f, 600.0f, out);
    ASSERT_TRUE(vis);
    EXPECT_FLOAT_EQ(out.x, 800.0f);   // x: (1*0.5+0.5)*800
    EXPECT_FLOAT_EQ(out.y, 0.0f);     // y is flipped: (-1*0.5+0.5)*600
}

TEST(GraphWorldToScreenTest, ReturnsFalseWhenBehindCamera) {
    // Build a matrix whose clip.w becomes negative for z=+1.
    glm::mat4 m(1.0f);
    m[2][3] = -1.0f;  // column 2, row 3 -> clip.w gets -z
    m[3][3] = 0.0f;
    glm::vec2 out{};
    bool vis = GraphWorldToScreen(m, glm::vec3(0.0f, 0.0f, 1.0f), 800.0f, 600.0f, out);
    EXPECT_FALSE(vis);
}
