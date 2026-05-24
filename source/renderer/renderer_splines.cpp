#include "pch.h"
#include "renderer_splines.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void Renderer_Splines::Init() {}

glm::vec3 Renderer_Splines::HermitePoint(float t,
    const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& t0, const glm::vec3& t1)
{
    float t2 = t * t, t3 = t2 * t;
    return (2.f*t3 - 3.f*t2 + 1.f)*p0
         + (t3 - 2.f*t2 + t)*t0
         + (-2.f*t3 + 3.f*t2)*p1
         + (t3 - t2)*t1;
}

void Renderer_Splines::Draw(
    const std::vector<LevelObject>& objects,
    GLuint ubo_mats,
    GLuint shader_program)
{
    if (!shader_program) return;

    glUseProgram(shader_program);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_mats);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    for (const auto& obj : objects) {
        if (!obj.isSplineContainer || obj.deleted) continue;
        if (Renderer_Objects::IsSkippedModelId(obj.modelId)) continue;

        const auto& children = obj.childrenIndices;

        // In IGI QSC, only the first SplineObjWaypoint carries the segmentModelId.
        // Subsequent waypoints leave it empty. Find the shared model here and use
        // it as a fallback so all segments in the spline get rendered.
        std::string fallbackSegmentModelId;
        for (int ci : children) {
            if (ci >= 0 && ci < (int)objects.size() && !objects[ci].segmentModelId.empty()) {
                fallbackSegmentModelId = objects[ci].segmentModelId;
                break;
            }
        }

        for (size_t i = 0; i + 1 < children.size(); ++i) {
            int si = children[i];
            int ei = children[i + 1];
            int pi = (i > 0) ? children[i - 1] : si;
            int ni = (i + 2 < children.size()) ? children[i + 2] : ei;

            if (si < 0 || si >= (int)objects.size()) continue;
            if (ei < 0 || ei >= (int)objects.size()) continue;
            if (pi < 0 || pi >= (int)objects.size()) pi = si;
            if (ni < 0 || ni >= (int)objects.size()) ni = ei;

            if (objects[si].deleted || objects[ei].deleted) continue;

            DrawSplineSegment(
                objects[si], objects[ei],
                objects[pi], objects[ni],
                obj, ubo_mats, shader_program,
                fallbackSegmentModelId);
        }
    }

    glUseProgram(0);
}

void Renderer_Splines::DrawSplineSegment(
    const LevelObject& start,
    const LevelObject& end,
    const LevelObject& prev,
    const LevelObject& nextNext,
    const LevelObject& parent,
    GLuint ubo_mats,
    GLuint shader_program,
    const std::string& fallbackSegmentModelId)
{
    const std::string& segModelId = start.segmentModelId.empty() ? fallbackSegmentModelId : start.segmentModelId;
    if (segModelId.empty()) return;
    if (Renderer_Objects::IsSkippedModelId(segModelId)) return;

    Mesh mesh = obj_renderer_.GetOrLoadMesh(segModelId, false);
    if (mesh.vertexCount == 0) return;

    GLint loc_model    = glGetUniformLocation(shader_program, "u_model");
    GLint loc_dirlight = glGetUniformLocation(shader_program, "u_dirlight");
    GLint loc_ambient  = glGetUniformLocation(shader_program, "u_ambient");
    GLint loc_useTex   = glGetUniformLocation(shader_program, "u_useTexture");
    GLint loc_tex      = glGetUniformLocation(shader_program, "u_texture");

    // Catmull-Rom tangents as Hermite tangents for both endpoints
    glm::vec3 p0     = glm::vec3(start.pos);
    glm::vec3 p1     = glm::vec3(end.pos);
    glm::vec3 p_prev = glm::vec3(prev.pos);
    glm::vec3 p_next = glm::vec3(nextNext.pos);

    glm::vec3 tan0 = (p1 - p_prev) * 0.5f;
    glm::vec3 tan1 = (p_next - p0) * 0.5f;

    // Clamp tangent magnitude to interval length to prevent Hermite overshoot/looping
    // at waypoints where adjacent segments have very different lengths or directions.
    float intervalLen2 = glm::length(p1 - p0);
    float t0len = glm::length(tan0);
    float t1len = glm::length(tan1);
    if (t0len > intervalLen2) tan0 *= intervalLen2 / t0len;
    if (t1len > intervalLen2) tan1 *= intervalLen2 / t1len;

    float localX = mesh.center.x + mesh.halfExtents.x;
    if (localX < 0.1f) localX = 0.1f;

    int steps = parent.splineSegmentCount;
    if (steps <= 0) steps = 20;
    if (steps > 64) steps = 64;

    for (int i = 0; i < steps; ++i) {
        float t      = (float)i       / (float)steps;
        float t_next = (float)(i + 1) / (float)steps;

        glm::vec3 pos     = HermitePoint(t,      p0, p1, tan0, tan1);
        glm::vec3 nextPos = HermitePoint(t_next, p0, p1, tan0, tan1);

        if (glm::distance(pos, nextPos) < 0.001f) {
            pos     = glm::mix(p0, p1, t);
            nextPos = glm::mix(p0, p1, t_next);
        }

        glm::vec3 tangent = glm::normalize(nextPos - pos);

        // Yaw: align local +X with tangent in the XY plane.
        float gamma = std::atan2(tangent.y, tangent.x);
        // Pitch: tilt tile up/down to follow slope (around pre-yaw local Y).
        float horiz = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        float pitch  = std::atan2(tangent.z, horiz);

        float dist = glm::distance(pos, nextPos);
        float scaleX = dist / localX;

        glm::mat4 model = glm::translate(glm::mat4(1.f), pos);
        model = glm::rotate(model, gamma, glm::vec3(0.f, 0.f, 1.f));   // yaw around world Z
        model = glm::rotate(model, -pitch, glm::vec3(0.f, 1.f, 0.f));  // pitch around local Y
        model = glm::scale(model, glm::vec3(scaleX, 40.96f, 40.96f));

        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));

        for (const auto& sub : mesh.subMeshes) {
            if (sub.VAO == 0 || sub.vertexCount == 0) continue;
            if (sub.textureID > 0) {
                glUniform3f(loc_dirlight, 0.6f, 0.6f, 0.6f);
                glUniform3f(loc_ambient,  0.4f, 0.4f, 0.4f);
                glUniform1i(loc_useTex, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sub.textureID);
                glUniform1i(loc_tex, 0);
            } else {
                glUniform3f(loc_dirlight, 0.7f, 0.7f, 0.7f);
                glUniform3f(loc_ambient,  0.2f, 0.2f, 0.2f);
                glUniform1i(loc_useTex, 0);
            }
            glBindVertexArray(sub.VAO);
            glDrawArrays(GL_TRIANGLES, 0, sub.vertexCount);
        }
        glBindVertexArray(0);
    }
}
