#include "pch.h"
#include "graph_overlay.h"

bool GraphWorldToScreen(const glm::mat4& viewProj, const glm::vec3& world,
                        float viewportW, float viewportH, glm::vec2& outScreen) {
    const glm::vec4 clip = viewProj * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0f) return false;  // behind the camera

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    outScreen.x = (ndc.x * 0.5f + 0.5f) * viewportW;
    outScreen.y = (-ndc.y * 0.5f + 0.5f) * viewportH;  // y flipped: screen origin top-left
    return true;
}
