#include "renderer_internal.h"

/*
================================================================================
 Editor bitmap font (editor/qed/editor.fnt) for HUD text
================================================================================
*/

/*
================================================================================
 Renderer
================================================================================
*/
Renderer::Renderer() : ubo_mats_(0), ubo_fog_(0), splines_(objects_) {
}

Renderer::~Renderer() { Shutdown(); }

#include <sstream>
#include <iomanip>
#include <unordered_map>




bool Renderer::Init() {
  ubo_mats_ = GL_CreateBuffer(GL_UNIFORM_BUFFER, sizeof(ubo_mats_s), nullptr,
                              GL_DYNAMIC_DRAW);
  ubo_fog_ = GL_CreateBuffer(GL_UNIFORM_BUFFER, sizeof(ubo_fog_s), nullptr,
                             GL_STATIC_DRAW);

  if (!skydome_.Init()) {
    return false;
  }

  if (!flat_sky_layers_.Init()) {
    return false;
  }

  if (!terrain_.Init()) {
    return false;
  }

  if (!objects_.Init()) {
    return false;
  }

  // init default state
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_TRUE);
  glDepthRangef(RENDER_DEPTH_MIN, RENDER_DEPTH_MAX);

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);

#if defined(_WIN32)
  glPolygonOffset(-1.0f, -1.0f);
#else
  glPolygonOffset(-64.0f, -64.0f); // tune this
#endif
  glDisable(GL_POLYGON_OFFSET_LINE);

  GL_TryEnableVSync();

  return true;
}

void Renderer::Shutdown() {
  terrain_.Shutdown();
  objects_.Shutdown();
  flat_sky_layers_.Shutdown();

  skydome_.Shutdown();

  GL_DeleteBuffer(ubo_fog_);
  GL_DeleteBuffer(ubo_mats_);
}

void Renderer::BeginLoadLevel() {
  flat_sky_layers_.UnloadAllTexs();
  terrain_.UnloadAllTexs();
  objects_.ClearCaches();
  graph_overlay_ = GraphFile{};
  graph_overlay_visible_ = false;
  graph_overlay_dirty_ = false;
  graph_overlay_selected_ = -1;
  graph_overlay_path_.clear();
  graph_overlay_taskid_.clear();
  graph_overlay_area_.clear();
}

void Renderer::SetupClearColor(const glm::vec4 &color) {
  glClearColor(color.r, color.g, color.b, 1.0f);
}

void Renderer::SetupFog(const glm::vec4 &color, float fog_far) {
  ubo_fog_s ubo_fog;

  ubo_fog.color_ = color;
  ubo_fog.far_ = fog_far;

  GL_BufferData(ubo_fog_, GL_UNIFORM_BUFFER, sizeof(ubo_fog), &ubo_fog,
                GL_STATIC_DRAW);
}

void Renderer::SetupSkydome(const skydome_define_s &d) {
  skydome_.UpdateVertices(d);
}

void Renderer::LoadFlatSkyLayerTex(int layer_no, const pic_s *pic) {
  flat_sky_layers_.LoadLayerTex(layer_no, pic);
}

void Renderer::LoadTerrainMatTex(const pic_s *pic) { terrain_.LoadMatTex(pic); }

void Renderer::LoadTerrainLMPTex(const pic_s *pic) { terrain_.LoadLMPTex(pic); }

vert_flat_sky_layer_s *Renderer::MapFlatSkyLayersVB() {
  return flat_sky_layers_.MapVB();
}

void Renderer::UnmapFlatSkyLayersVB() { flat_sky_layers_.UnmapVB(); }

vert_pos_a_uv_s *Renderer::MapTerrainVB() { return terrain_.MapVB(); }

void Renderer::UnmapTerrainVB() { terrain_.UnmapVB(); }

uint32_t *Renderer::MapTerrainIB() { return terrain_.MapIB(); }

void Renderer::UnmapTerrainIB() { terrain_.UnmapIB(); }

render_chunk_s *Renderer::GetTerrainRenderChunckBuffer() {
  return terrain_.GetRenderChunckBuffer();
}

// Reads the depth buffer at screen (mx, my_topdown) and unprojects to a world-space
// point using the full world->clip matrix (proj*view*scale). Returns false if nothing
// was drawn there (cursor over sky / cleared depth). (issue 3)
void Renderer::SetupUBOMats(const view_define_s &vd) {
  glm::mat4 mat_proj_persp = glm::perspective(
      vd.fovy_, (float)vd.viewport_width_ / vd.viewport_height_,
      vd.render_z_near_, vd.render_z_far_);

  glm::vec3 scaled_down_pos = vd.pos_ * RENDERER_MODEL_SCALE_DOWN;
  glm::mat4 mat_view = glm::lookAt(
      scaled_down_pos, scaled_down_pos + vd.forward_ * WORLD_UNITS_PER_METER,
      vd.up_);
  glm::mat4 mat_follow_view = glm::lookAt(VEC3_ORIGIN, vd.forward_, vd.up_);
  glm::mat4 mat_scale =
      glm::scale(glm::mat4(1.0f), glm::vec3(RENDERER_MODEL_SCALE_DOWN));

  mat_proj_ = mat_proj_persp;
  mat_view_ = mat_view;

  // setup uniform buffer

  ubo_mats_s ubo_mats;

  // skydome
  ubo_mats.mvp_mat_follow_view_ = mat_proj_persp * mat_follow_view * mat_scale;

  // flat sky layer
  ubo_mats.mvp_flat_sky_layer_ =
      glm::ortho(0.0f, (float)vd.viewport_width_, 0.0f,
                 (float)vd.viewport_height_, -1.0f, 1.0f);

  // terrain
  ubo_mats.mvp_objects_ = mat_proj_persp * mat_view * mat_scale;

  // flush ubo buffer
  GL_BufferData(ubo_mats_, GL_UNIFORM_BUFFER, sizeof(ubo_mats), &ubo_mats,
                GL_DYNAMIC_DRAW);
}

void Renderer::DrawAnimSkeleton(const std::vector<glm::mat4>& boneWorldTransforms,
                                 const glm::mat4& objWorldMat) {
    if (boneWorldTransforms.empty()) return;

    GLint prevProg;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glUseProgram(0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glm::mat4 mvp = mat_proj_ * mat_view_ * glm::scale(glm::mat4(1.0f), glm::vec3(RENDERER_MODEL_SCALE_DOWN));
    glLoadMatrixf(glm::value_ptr(mvp));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glLineWidth(3.0f);
    glBegin(GL_LINES);
    for (size_t i = 1; i < boneWorldTransforms.size(); ++i) {
        glm::vec4 childPos = objWorldMat * boneWorldTransforms[i] * glm::vec4(0, 0, 0, 1);
        glm::vec4 parentPos = objWorldMat * boneWorldTransforms[i - 1] * glm::vec4(0, 0, 0, 1);
        glColor4f(0.2f, 0.9f, 0.2f, 0.7f);
        glVertex4f(parentPos.x, parentPos.y, parentPos.z, parentPos.w);
        glVertex4f(childPos.x, childPos.y, childPos.z, childPos.w);
    }
    glEnd();

    glPointSize(5.0f);
    glColor4f(0.6f, 1.0f, 0.6f, 1.0f);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < boneWorldTransforms.size(); ++i) {
        glm::vec4 worldPos = objWorldMat * boneWorldTransforms[i] * glm::vec4(0, 0, 0, 1);
        glVertex4f(worldPos.x, worldPos.y, worldPos.z, worldPos.w);
    }
    glEnd();

    glLineWidth(1.0f);
    glPointSize(1.0f);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    if (prevProg) glUseProgram((GLuint)prevProg);
}

void Renderer::DrawSkinnedMesh(const std::string& modelId, bool isBuilding,
                                const std::vector<glm::mat4>& boneWorldTransforms,
                                const glm::mat4& objWorldMat) {
    if (boneWorldTransforms.empty()) return;
    const ParsedGeometry* geo = objects_.GetOrLoadSkinGeometry(modelId, isBuilding);
    if (!geo || geo->vertices.empty() || geo->triangles.empty()) return;

    // Rest-pose bone world positions (raw units) — same convention the static
    // mesh cache used to bake geo->vertices[i].pos in the first place.
    static std::map<std::string, std::vector<glm::vec3>> s_restPosCache;
    auto rpIt = s_restPosCache.find(modelId);
    if (rpIt == s_restPosCache.end()) {
        rpIt = s_restPosCache.emplace(modelId, ComputeBoneWorldPositionsPublic(geo->bones)).first;
    }
    const std::vector<glm::vec3>& boneRestPos = rpIt->second;
    if (boneRestPos.empty()) return;

    // The BEF skeleton's root is conventionally at local (0,0,0); the MEF's
    // root sits at its own absolute rest position. Shift the deformed mesh
    // back into the MEF's frame after animating in BEF-relative space.
    const glm::vec3 rootOffset = boneRestPos[0] * kMefNativeScale;

    std::vector<glm::vec3> deformedPos(geo->vertices.size());
    std::vector<glm::vec3> deformedNormal(geo->vertices.size());
    for (size_t i = 0; i < geo->vertices.size(); ++i) {
        const RenderVertex& rv = geo->vertices[i];
        uint16_t b = rv.boneIndex;
        if (b >= boneRestPos.size() || b >= boneWorldTransforms.size()) {
            deformedPos[i] = rv.pos;
            deformedNormal[i] = rv.normal;
            continue;
        }
        glm::vec3 localOffset = rv.pos - boneRestPos[b] * kMefNativeScale;
        deformedPos[i] = glm::vec3(boneWorldTransforms[b] * glm::vec4(localOffset, 1.0f)) + rootOffset;
        deformedNormal[i] = glm::mat3(boneWorldTransforms[b]) * rv.normal;
    }

    // Resolve the same per-material textures the static (rigid) draw uses, so
    // the live-skinned mesh looks identical when not moving instead of a flat
    // debug color. Cheap: GetOrLoadMesh hits the cache (the model is already
    // loaded for the rigid draw elsewhere).
    Mesh mesh = objects_.GetOrLoadMesh(modelId, isBuilding);
    std::unordered_map<int, GLuint> slotToTexture;
    for (const auto& sub : mesh.subMeshes) slotToTexture[sub.materialSlot] = sub.textureID;
    const GLuint fallbackTexture = mesh.textureID;

    GLint prevProg;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glUseProgram(0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glm::mat4 mvp = mat_proj_ * mat_view_ * glm::scale(glm::mat4(1.0f), glm::vec3(RENDERER_MODEL_SCALE_DOWN));
    glLoadMatrixf(glm::value_ptr(mvp));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // The static (rigid) object draw never culls ("Disable backface culling
    // to fix invisible interior walls" — see Renderer_Objects::Draw). Match
    // that here: culling against the deformed mesh risks the wrong winding
    // after the bone transform and silently dropping faces (looked like a
    // broken/incomplete blob instead of the full body).
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Simple fixed light direction, lit in world space via the object's rotation only.
    // Guard against degenerate (zero-length) source normals — some skeletal
    // models' raw XTRV normals aren't reliable — which would otherwise NaN
    // through normalize()/dot() and render the whole mesh black.
    const glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, -0.6f, 0.7f));
    const glm::mat3 normalMat = glm::mat3(objWorldMat);

    auto litColor = [&](uint32_t vi) {
        glm::vec3 n = normalMat * deformedNormal[vi];
        float nLen = glm::length(n);
        float diff = 1.0f;
        if (nLen > 1e-5f) {
            diff = glm::dot(n / nLen, lightDir);
            if (!std::isfinite(diff)) diff = 1.0f;
            diff = glm::clamp(diff, 0.5f, 1.0f);
        }
        glColor4f(diff, diff, diff, 1.0f);
    };
    auto emitVertex = [&](uint32_t vi) {
        litColor(vi);
        glTexCoord2f(geo->vertices[vi].uv.x, 1.0f - geo->vertices[vi].uv.y);
        glm::vec4 worldPos = objWorldMat * glm::vec4(deformedPos[vi], 1.0f);
        glVertex4f(worldPos.x, worldPos.y, worldPos.z, worldPos.w);
    };

    // Single pass over ALL triangles with one bound texture. Per-render-block
    // texture switching was dropping/garbling geometry (geo->renderBlocks'
    // triangle ranges don't reliably tile the full triangle list the way the
    // static mesh builder's regrouped subMeshes do) — drawing every triangle
    // in one go guarantees the full, correct shape; only the per-block detail
    // (e.g. a separate face texture) is sacrificed.
    GLuint tex = fallbackTexture;
    if (tex == 0) {
        for (const auto& [slot, t] : slotToTexture) { if (t != 0) { tex = t; break; } }
    }
    if (tex != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    glBegin(GL_TRIANGLES);
    for (const auto& tri : geo->triangles) {
        for (int k = 0; k < 3; ++k) {
            uint32_t vi = tri[k];
            if (vi >= deformedPos.size()) continue;
            emitVertex(vi);
        }
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    if (prevProg) glUseProgram((GLuint)prevProg);
}

