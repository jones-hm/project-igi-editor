/******************************************************************************
 * @file    graph_overlay.h
 * @brief   World-space navigation-graph overlay (nodes + links) for the editor.
 *
 * GraphWorldToScreen is pure projection math (glm only) and is unit-tested.
 * RenderGraphOverlay performs ImGui draw calls and is editor-only.
 *****************************************************************************/

#pragma once
#include <glm/glm.hpp>
#include "parsers/graph_parser.h"

// Project a world-space point through `viewProj` to screen pixels (origin
// top-left). Returns false if the point is behind the camera (clip.w <= 0);
// in that case outScreen is left unchanged.
bool GraphWorldToScreen(const glm::mat4& viewProj, const glm::vec3& world,
                        float viewportW, float viewportH, glm::vec2& outScreen);

// Hit-test: return the id of the node nearest to (mouseX,mouseY) in screen
// space whose screen distance is within thresholdPx, or -1 if none. mouseX/
// mouseY use the same top-left origin as GraphWorldToScreen. Nodes behind the
// camera are ignored. Ties resolve to the first matching node.
int GRAPH_PickNode(const GraphFile& graph, const glm::mat4& viewProj,
                   float mouseX, float mouseY, float viewportW, float viewportH,
                   float thresholdPx);

// Draw the navigation graph as an overlay on top of the 3D viewport using the
// ImGui foreground draw list: links as lines, nodes as colored circles, and
// id labels. The selected node (if any) is highlighted. Implemented in
// graph_overlay.cpp (editor target only).
void RenderGraphOverlay(const GraphFile& graph, int selectedNodeId,
                        const glm::mat4& viewProj, float viewportW, float viewportH,
                        bool showLabels, bool showLinks);
