#include "renderer_objects_internal.h"




void Renderer_Objects::InitSphereMesh() {
    const int LAT = 12;
    const int LON = 24;
    std::vector<float> verts;
    verts.reserve(LAT * LON * 6 * 8); // 2 triangles per quad, 3 verts each, 8 floats

    auto addVert = [&](float x, float y, float z) {
        verts.push_back(x); verts.push_back(y); verts.push_back(z);
        verts.push_back(x); verts.push_back(y); verts.push_back(z); // normal = position for unit sphere
        verts.push_back(0.0f); verts.push_back(0.0f);
    };

    for (int i = 0; i < LAT; ++i) {
        float lat0 = glm::pi<float>() * (-0.5f + (float)i / LAT);
        float lat1 = glm::pi<float>() * (-0.5f + (float)(i + 1) / LAT);
        float sin0 = std::sin(lat0), cos0 = std::cos(lat0);
        float sin1 = std::sin(lat1), cos1 = std::cos(lat1);

        for (int j = 0; j < LON; ++j) {
            float lon0 = 2.0f * glm::pi<float>() * (float)j / LON;
            float lon1 = 2.0f * glm::pi<float>() * (float)(j + 1) / LON;
            float clon0 = std::cos(lon0), slon0 = std::sin(lon0);
            float clon1 = std::cos(lon1), slon1 = std::sin(lon1);

            float x00 = cos0*clon0, y00 = cos0*slon0, z00 = sin0;
            float x01 = cos0*clon1, y01 = cos0*slon1, z01 = sin0;
            float x10 = cos1*clon0, y10 = cos1*slon0, z10 = sin1;
            float x11 = cos1*clon1, y11 = cos1*slon1, z11 = sin1;

            addVert(x00, y00, z00); addVert(x10, y10, z10); addVert(x11, y11, z11);
            addVert(x00, y00, z00); addVert(x11, y11, z11); addVert(x01, y01, z01);
        }
    }

    sphere_vertex_count_ = (int)(verts.size() / 8);
    glGenVertexArrays(1, &sphere_vao_);
    glGenBuffers(1, &sphere_vbo_);
    glBindVertexArray(sphere_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

// ─── DrawMagicObjSpheres ──────────────────────────────────────────────────────
// Draws a solid red sphere at each MagicObject ATTA attachment position.
void Renderer_Objects::DrawMagicObjSpheres(const std::vector<LevelObject>& objects, GLuint ubo_mats) {
    EnsureMagicObjIdsLoaded();
    if (magicobj_ids_.empty()) return;

    if (sphere_vao_ == 0) InitSphereMesh();
    if (sphere_vao_ == 0) return;

    glUseProgram(shader_program_);
    glBindBufferBase(GL_UNIFORM_BUFFER, ubo_binding_point_, ubo_mats);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    GLint loc_model     = glGetUniformLocation(shader_program_, "u_model");
    GLint loc_dirlight  = glGetUniformLocation(shader_program_, "u_dirlight");
    GLint loc_ambient   = glGetUniformLocation(shader_program_, "u_ambient");
    GLint loc_useTex    = glGetUniformLocation(shader_program_, "u_useTexture");
    GLint loc_alpha     = glGetUniformLocation(shader_program_, "u_alpha");
    GLint loc_baseColor = glGetUniformLocation(shader_program_, "u_baseColor");

    glUniform1i(loc_useTex, 0);
    glUniform1f(loc_alpha, 1.0f);
    glUniform3f(loc_dirlight, 0.5f, 0.5f, 0.5f);
    glUniform3f(loc_ambient,  0.5f, 0.5f, 0.5f);
    glUniform4f(loc_baseColor, 1.0f, 0.0f, 0.0f, 1.0f); // red

    // Sphere radius = 0.3 meters in world-unit space
    const float SPHERE_SCALE = WORLD_UNITS_PER_METER * 0.3f;

    auto drawSphere = [&](const glm::vec3& worldPos) {
        glm::mat4 sphereMat = glm::translate(glm::mat4(1.0f), worldPos);
        sphereMat = glm::scale(sphereMat, glm::vec3(SPHERE_SCALE));
        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(sphereMat));
        glBindVertexArray(sphere_vao_);
        glDrawArrays(GL_TRIANGLES, 0, sphere_vertex_count_);
    };

    for (const auto& obj : objects) {
        if (obj.deleted || obj.modelId.empty()) continue;

        glm::mat4 parentRot(1.0f);
        parentRot = glm::rotate(parentRot, (float)obj.rot.z, glm::vec3(0, 0, 1));
        parentRot = glm::rotate(parentRot, (float)obj.rot.x, glm::vec3(1, 0, 0));
        parentRot = glm::rotate(parentRot, (float)obj.rot.y, glm::vec3(0, 1, 0));

        glm::mat4 rootWorldMat(1.0f);
        rootWorldMat = glm::translate(rootWorldMat, glm::vec3((float)obj.pos.x, (float)obj.pos.y, (float)obj.pos.z));
        rootWorldMat = rootWorldMat * parentRot;

        // ── ATTA magic objects ───────────────────────────────────────────────
        std::string prefix = obj.isBuilding ? "building:" : "object:";
        std::string attKey = std::to_string(current_level_) + ":" + prefix + obj.modelId;
        auto ait = attachment_cache_.find(attKey);
        if (ait != attachment_cache_.end()) {
            for (const auto& att : ait->second) {
                if (!magicobj_ids_.count(att.modelId)) continue;
                glm::vec3 localOff(att.px, att.py, att.pz);
                glm::vec3 worldPos = glm::vec3(rootWorldMat * glm::vec4(localOff, 1.0f));
                drawSphere(worldPos);
            }
        }

        // ── XTVM magic vertices ──────────────────────────────────────────────
        // magicVertices are stored in mesh-local space (divided by 40.96).
        // Reconstruct world position using the full scaled model matrix.
        std::string meshKey = std::to_string(current_level_) + ":" + prefix + obj.modelId;
        auto mit = mesh_cache_.find(meshKey);
        if (mit != mesh_cache_.end()) {
            const Mesh& mesh = mit->second;
            if (!mesh.magicVertices.empty()) {
                glm::mat4 modelMat = rootWorldMat * glm::scale(glm::mat4(1.0f), glm::vec3(40.96f));
                for (const auto& mv : mesh.magicVertices) {
                    // skip null entries (all-zero position)
                    if (mv.x == 0.0f && mv.y == 0.0f && mv.z == 0.0f) continue;
                    glm::vec3 worldPos = glm::vec3(modelMat * glm::vec4(mv, 1.0f));
                    drawSphere(worldPos);
                }
            }
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);
}


// METAL_DOOR_SLIDE_UP (model 506_xx) appears in levels 12/13/14 as an EditRigidObj
// carrying a genuine multi-axis Euler tuple. It bypasses the engine's special door
// transform and so needs a different Euler application order than other rigid objects.
static bool IsMetalSlideUpDoorModel(const std::string& modelId) {
    if (modelId.empty()) return false;
    return modelId.rfind("506_", 0) == 0;
}

bool Renderer_Objects::IsSkippedModelId(const std::string& modelId) {
    if (modelId.empty()) return false;
    return modelId == "colbox" || modelId == "colbox2" || modelId == "colbox4" || modelId == "colbox66";
}

// ─── Shader Sources ───────────────────────────────────────────────────────────
static const char* OBJ_VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(std140) uniform Matrices {
    mat4 u_unused1;
    mat4 u_unused2;
    mat4 u_mvp; // Proj * View * GlobalScale
};


uniform mat4 u_model;

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_fragPos;

void main() {
    vec4 worldPos   = u_model * vec4(a_pos, 1.0);
    v_fragPos       = worldPos.xyz;
    v_normal        = mat3(transpose(inverse(u_model))) * a_normal;
    v_uv            = a_uv;
    gl_Position     = u_mvp * u_model * vec4(a_pos, 1.0);
}

)";

static const char* OBJ_FRAG_SRC = R"(
#version 330 core
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_fragPos;

uniform vec3 u_dirlight;   // directional light RGB
uniform vec3 u_ambient;    // ambient light RGB
uniform sampler2D u_texture;
uniform int u_useTexture;
uniform float u_alpha;     // material alpha (1.0 = opaque, <1.0 = transparent)
uniform vec4 u_baseColor;  // Base color when no texture
uniform vec3 u_tint; // per-object multiplicative tint (default white); magenta = missing-in-res warning
uniform float u_glassMin;  // glass sheen floor: clean (low-alpha) glass renders at
                           // least this opaque so the pane is visible. 0 = not glass.

out vec4 fragColor;

void main() {
    vec3 N = normalize(v_normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    float diff = max(dot(N, lightDir), 0.0);

    vec3 viewDir = normalize(vec3(0.0, 1.0, 1.0));
    vec3 halfVec = normalize(lightDir + viewDir);
    float spec = pow(max(dot(N, halfVec), 0.0), 32.0) * 0.25;

    vec3 light = u_ambient + u_dirlight * (diff + spec);

    vec4 texColor = (u_useTexture != 0) ? texture(u_texture, v_uv) : u_baseColor;

    float finalAlpha = (u_useTexture != 0 ? texColor.a : 1.0) * u_alpha;

    // Glass: even a perfectly clear pane (texture alpha ~0) must show a faint
    // reflective sheen, so floor the alpha. Add a subtle specular highlight so the
    // glass reads as a surface, not an empty hole.
    if (u_glassMin > 0.0) {
        finalAlpha = max(finalAlpha, u_glassMin);
        light += vec3(spec * 1.5);
    }

    fragColor = vec4(light * texColor.rgb * u_tint, finalAlpha);

    // Alpha-test cutout for foliage / fences / grilles only (alpha >= 0.9). Glass
    // (lower alpha or u_glassMin set) is NEVER cut out — it blends so you can see
    // through it while still seeing the pane.
    if (u_glassMin <= 0.0 && u_alpha >= 0.9 && texColor.a < 0.75) discard;
    if (fragColor.a < 0.01) discard;
}
)";

// ─── Picking Shader Sources ───────────────────────────────────────────────────
static const char* PICK_VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(std140) uniform Matrices {
    mat4 u_unused1;
    mat4 u_unused2;
    mat4 u_mvp;
};

uniform mat4 u_model;

void main() {
    gl_Position = u_mvp * u_model * vec4(a_pos, 1.0);
}
)";

static const char* PICK_FRAG_SRC = R"(
#version 330 core
uniform int u_object_id;
out vec4 fragColor;
void main() {
    int id = u_object_id;
    fragColor = vec4(
        float((id >> 16) & 0xFF) / 255.0,
        float((id >>  8) & 0xFF) / 255.0,
        float( id        & 0xFF) / 255.0,
        1.0
    );
}
)";

// ─── Shader Compile Helper ────────────────────────────────────────────────────
static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "[ObjShader] Compile error: " << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint BuildShaderProgram() {
    GLuint vert = CompileShader(GL_VERTEX_SHADER,   OBJ_VERT_SRC);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, OBJ_FRAG_SRC);
    if (!vert || !frag) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "[ObjShader] Link error: " << log << "\n";
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

static GLuint BuildPickShaderProgram() {
    GLuint vert = CompileShader(GL_VERTEX_SHADER,   PICK_VERT_SRC);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, PICK_FRAG_SRC);
    if (!vert || !frag) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "[PickShader] Link error: " << log << "\n";
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────
Renderer_Objects::Renderer_Objects() : shader_program_(0), ubo_binding_point_(0), selection_vao_(0), selection_vbo_(0) {}

Renderer_Objects::~Renderer_Objects() {
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────
bool Renderer_Objects::Init() {
    shader_program_ = BuildShaderProgram();
    if (!shader_program_) {
        Logger::Get().Log(LogLevel::ERR, "[Renderer_Objects] Failed to build shader.");
        return false;
    }


    // Bind UBO block "Matrices" to binding point 0
    // This MUST match the binding point used by terrain renderer
    GLuint blockIdx = glGetUniformBlockIndex(shader_program_, "Matrices");
    if (blockIdx != GL_INVALID_INDEX) {
        glUniformBlockBinding(shader_program_, blockIdx, ubo_binding_point_);
        Logger::Get().Log(LogLevel::INFO, "[Renderer_Objects] UBO 'Matrices' bound to point " + std::to_string(ubo_binding_point_));
    } else {
        Logger::Get().Log(LogLevel::WARNING, "[Renderer_Objects] WARNING: UBO 'Matrices' block not found.");
    }


    // Picking shader
    pick_shader_prog_ = BuildPickShaderProgram();
    if (!pick_shader_prog_) {
        Logger::Get().Log(LogLevel::WARNING, "[Renderer_Objects] Failed to build picking shader.");
    } else {
        GLuint blk = glGetUniformBlockIndex(pick_shader_prog_, "Matrices");
        if (blk != GL_INVALID_INDEX)
            glUniformBlockBinding(pick_shader_prog_, blk, ubo_binding_point_);
    }
    // FBO created on first use (size unknown at init time)

    Logger::Get().Log(LogLevel::INFO, "[Renderer_Objects] Init OK.");

    return true;
}

// ─── ClearCaches ──────────────────────────────────────────────────────────────
void Renderer_Objects::ClearCaches() {
    Logger::Get().Log(LogLevel::INFO, "[Renderer_Objects] Clearing all level caches... (meshes=" +
        std::to_string(mesh_cache_.size()) + " textures=" + std::to_string(texture_cache_.size()) +
        " attachments=" + std::to_string(attachment_cache_.size()) +
        " modelTexMap=" + std::to_string(model_texture_map_cache_.size()) +
        " texMapLevel=" + std::to_string(texture_map_level_) +
        " globalTexMapLoaded=" + std::to_string(global_texture_map_loaded_ ? 1 : 0) + ")");
    for (auto& pair : mesh_cache_) {
        destroyModel(pair.second);
    }
    mesh_cache_.clear();
    attachment_cache_.clear();

    for (auto& pair : texture_cache_) {
        if (pair.second) {
            glDeleteTextures(1, &pair.second);
        }
    }
    texture_cache_.clear();
    model_texture_map_cache_.clear();
    
    // We explicitly DO NOT clear global_texture_map_, model_level_map_,
    // texture_level_map_, window_model_ids_, portal_distances_, deathzone_ids_,
    // or magicobj_ids_ here. These are populated from global sources (lod.qvm, names.qvm, 
    // or scanning all 14 levels' DAT files) and their definitions are static across 
    // level switches. Clearing them causes lazy re-loads that might fail if cache 
    // stamps trick the asset extractor into skipping partial directories.
    texture_map_level_ = -1;
    persistent_dat_ = DATFile{};
    persistent_dat_path_.clear();
    logged_draw_buildings_.clear();

}

// ─── Shutdown ─────────────────────────────────────────────────────────────────
void Renderer_Objects::Shutdown() {
    for (auto& pair : mesh_cache_)
        destroyModel(pair.second);
    mesh_cache_.clear();
    attachment_cache_.clear();

    for (auto& pair : texture_cache_) {
        if (pair.second) {
            glDeleteTextures(1, &pair.second);
        }
    }
    texture_cache_.clear();
    model_texture_map_cache_.clear();
    texture_map_level_ = -1;
    persistent_dat_ = DATFile{};
    persistent_dat_path_.clear();

    if (shader_program_) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }

    if (selection_vao_) {
        glDeleteVertexArrays(1, &selection_vao_);
        selection_vao_ = 0;
    }
    if (selection_vbo_) {
        glDeleteBuffers(1, &selection_vbo_);
        selection_vbo_ = 0;
    }
    if (sphere_vao_) {
        glDeleteVertexArrays(1, &sphere_vao_);
        sphere_vao_ = 0;
    }
    if (sphere_vbo_) {
        glDeleteBuffers(1, &sphere_vbo_);
        sphere_vbo_ = 0;
    }
    if (pick_fbo_) {
        glDeleteFramebuffers(1, &pick_fbo_);
        pick_fbo_ = 0;
    }
    if (pick_color_tex_) {
        glDeleteTextures(1, &pick_color_tex_);
        pick_color_tex_ = 0;
    }
    if (pick_depth_rb_) {
        glDeleteRenderbuffers(1, &pick_depth_rb_);
        pick_depth_rb_ = 0;
    }
    if (pick_shader_prog_) {
        glDeleteProgram(pick_shader_prog_);
        pick_shader_prog_ = 0;
    }
    pick_fbo_w_ = 0;
    pick_fbo_h_ = 0;
}

// ─── InitPickingFBO ───────────────────────────────────────────────────────────
void Renderer_Objects::InitPickingFBO(int w, int h) {
    // Delete existing resources
    if (pick_fbo_)       { glDeleteFramebuffers(1,  &pick_fbo_);       pick_fbo_ = 0; }
    if (pick_color_tex_) { glDeleteTextures(1,       &pick_color_tex_); pick_color_tex_ = 0; }
    if (pick_depth_rb_)  { glDeleteRenderbuffers(1,  &pick_depth_rb_);  pick_depth_rb_ = 0; }

    // Color texture (RGB8 — ID encoded as RGB)
    glGenTextures(1, &pick_color_tex_);
    glBindTexture(GL_TEXTURE_2D, pick_color_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &pick_depth_rb_);
    glBindRenderbuffer(GL_RENDERBUFFER, pick_depth_rb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Framebuffer
    glGenFramebuffers(1, &pick_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, pick_fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pick_color_tex_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pick_depth_rb_);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Logger::Get().Log(LogLevel::ERR, "[Renderer_Objects] Picking FBO incomplete: " + std::to_string(status));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    pick_fbo_w_ = w;
    pick_fbo_h_ = h;
}

// ─── DrawAttachmentsForPicking ────────────────────────────────────────────────
// Draws ATTA sub-models of parentModelId with the parent's pick ID so that
// clicking anywhere on an ATTA part selects the owning LevelObject.
// Mirrors DrawAttachmentsRecursive's transform calculation; skips transparency,
// lighting, and texture setup since the picking shader only cares about geometry.
void Renderer_Objects::DrawAttachmentsForPicking(
    const std::string& parentModelId, bool isBuilding,
    const glm::mat4& parentWorldMat, float parentScale,
    GLint loc_model, GLint loc_id, int parentObjIndex,
    std::unordered_set<std::string>& drawn)
{
    const std::string prefix = isBuilding ? "building:" : "object:";
    const std::string attKey = std::to_string(current_level_) + ":" + prefix + parentModelId;
    auto ait = attachment_cache_.find(attKey);
    if (ait == attachment_cache_.end()) return;

    const auto& atts = ait->second;
    for (size_t ri = 0; ri < atts.size(); ++ri) {
        const auto& att = atts[ri];
        // Find the attachment mesh (same cache lookup as DrawAttachmentsRecursive)
        std::string subKey = std::to_string(current_level_) + ":" + prefix + att.modelId;
        auto sit = mesh_cache_.find(subKey);
        if (sit == mesh_cache_.end() || sit->second.vertexCount == 0) {
            subKey = std::to_string(current_level_) + ":object:" + att.modelId;
            sit = mesh_cache_.find(subKey);
        }
        if (sit == mesh_cache_.end()) continue;
        const Mesh& subMesh = sit->second;

        // Build ATTA world transform (identical logic to DrawAttachmentsRecursive)
        glm::mat4 attLocalRot(
            att.r[0], att.r[1], att.r[2], 0.f,
            att.r[3], att.r[4], att.r[5], 0.f,
            att.r[6], att.r[7], att.r[8], 0.f,
            0.f,      0.f,      0.f,      1.f
        );
        glm::vec3 localOff(att.px, att.py, att.pz);
        glm::vec3 worldPos = glm::vec3(parentWorldMat * glm::vec4(localOff, 1.f));
        glm::mat4 parentRot = parentWorldMat;
        parentRot[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
        glm::mat4 childWorldMat(1.0f);
        childWorldMat = glm::translate(childWorldMat, worldPos);
        childWorldMat = childWorldMat * parentRot * attLocalRot;

        // Recurse regardless of whether this node has pick-able geometry
        std::string childKey = parentModelId + ">" + att.modelId;
        bool recurse = drawn.insert(childKey).second;

        // Skip ATTAs already promoted (by world-pos key or by direct record index).
        bool occupied = IsAttaPromoted(att.modelId, worldPos) ||
            promoted_atta_records_.count(parentModelId + ":" + std::to_string(ri)) > 0;

        if (subMesh.vertexCount > 0 && !occupied) {
            // Record this ATTA as a uniquely-pickable, promotable entry.
            int entry = (int)atta_pick_entries_.size();
            AttaPickEntry e;
            e.parentObjIndex         = parentObjIndex;
            e.modelId                = att.modelId;
            e.immediateParentModelId = parentModelId;
            e.worldPos               = worldPos;
            e.worldRot               = glm::mat3(childWorldMat);
            e.scale                  = parentScale;
            e.localPos               = glm::vec3(att.px, att.py, att.pz);
            e.recordIndex            = (int)ri;
            e.parentWorldMat         = parentWorldMat;
            atta_pick_entries_.push_back(e);

            glm::mat4 leafModel = glm::scale(childWorldMat, glm::vec3(40.96f * parentScale));
            glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(leafModel));
            glUniform1i(loc_id, kAttaPickBase + 1 + entry);

            if (!subMesh.subMeshes.empty()) {
                for (const auto& sub : subMesh.subMeshes) {
                    if (sub.VAO == 0 || sub.vertexCount == 0) continue;
                    glBindVertexArray(sub.VAO);
                    glDrawArrays(GL_TRIANGLES, 0, sub.vertexCount);
                }
            } else if (subMesh.VAO) {
                glBindVertexArray(subMesh.VAO);
                glDrawArrays(GL_TRIANGLES, 0, subMesh.vertexCount);
            }
        }

        if (recurse) {
            DrawAttachmentsForPicking(att.modelId, isBuilding, childWorldMat, parentScale,
                                      loc_model, loc_id, parentObjIndex, drawn);
        }
    }
}

// ─── DrawForPicking ───────────────────────────────────────────────────────────
void Renderer_Objects::DrawForPicking(GLuint ubo_mats,
                                      const std::vector<LevelObject>& objects,
                                      int draw_parts,
                                      const glm::vec3& camera_pos)
{
    if (!pick_shader_prog_) return;

    constexpr float BASE_SCALE = 40.96f;
    const int DRAW_OBJECTS   = 4;
    const int DRAW_BUILDINGS = 16;
    const int DRAW_PROPS     = 32;

    // Reset the per-pass ATTA pick capture and rebuild EditRigidObj occupancy so
    // ATTAs already promoted to real tasks aren't offered for promotion again.
    atta_pick_entries_.clear();
    editrigid_occupancy_.clear();
    suppressed_atta_keys_.clear();
    for (const auto& o : objects) {
        if (o.deleted || o.type != "EditRigidObj" || o.modelId.empty()) continue;
        editrigid_occupancy_.insert(AttaOccupancyKey(o.modelId, glm::vec3(o.pos)));
        if (o.name.rfind("ATTA:", 0) == 0) {
            suppressed_atta_keys_.insert(o.name.substr(5));
        }
    }

    // Set picking render state
    glBindFramebuffer(GL_FRAMEBUFFER, pick_fbo_);
    glViewport(0, 0, pick_fbo_w_, pick_fbo_h_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // GL_LEQUAL so that QSC child tasks (drawn AFTER the parent hull + ATTA) can
    // overwrite pick IDs written by ATTA sub-models at the same depth. Combined
    // with culling OFF (below), every child EditRigidObj wins over the parent ATTA.
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    // Match the visual pass: culling OFF. With back-face culling ON, child objects
    // whose meshes have reversed/inconsistent winding (consoles, crates, lights in
    // buildings) rendered visibly but were absent from the pick buffer.
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(pick_shader_prog_);
    glBindBufferBase(GL_UNIFORM_BUFFER, ubo_binding_point_, ubo_mats);

    GLint loc_model = glGetUniformLocation(pick_shader_prog_, "u_model");
    GLint loc_id    = glGetUniformLocation(pick_shader_prog_, "u_object_id");

    for (int i = 0; i < (int)objects.size(); ++i) {
        const auto& obj = objects[i];
        if (obj.deleted || obj.modelId.empty()) continue;
        if (obj.isSplineWaypoint || obj.isSplineContainer) continue;
        if (IsSkippedModelId(obj.modelId)) continue;

        // Selective rendering (mirrors Draw())
        bool shouldDraw = false;
        if (draw_parts & DRAW_OBJECTS) {
            shouldDraw = true;
        } else {
            if ((draw_parts & DRAW_BUILDINGS) && obj.isBuilding)  shouldDraw = true;
            if ((draw_parts & DRAW_PROPS)     && !obj.isBuilding) shouldDraw = true;
        }
        if (!shouldDraw) continue;

        // NOTE: We deliberately do NOT cull objects that sit inside buildings.
        // Every child / sub-object (trucks, soldiers, crates, lights inside a
        // garage or container) must be individually pickable. GPU depth testing
        // already gives correct occlusion: building hull walls and window glass
        // are rendered opaque into the pick buffer and occlude objects behind
        // them, while true openings (open doors) let the cursor reach the
        // interior object underneath. This is geometrically exact and order-
        // independent, so AABB-based interior occlusion is unnecessary.

        Mesh mesh = GetOrLoadMesh(obj.modelId, obj.isBuilding);
        if (mesh.vertexCount == 0) continue;
        // Allow collision-only meshes (fromRenderMesh==false) as fallback pick hitboxes
        // so vehicles, cargo, and any model without render vertices are still clickable.

        // Build model matrix (same convention as Draw())
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(obj.pos));
        model = glm::rotate(model, (float)obj.rot.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::rotate(model, (float)obj.rot.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, (float)obj.rot.y, glm::vec3(0.0f, 1.0f, 0.0f));
        if (IsWeaponModel(obj.modelId) || obj.type == "GunPickup" || obj.type == "AmmoPickup") {
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        model = glm::scale(model, glm::vec3(BASE_SCALE * obj.scale));

        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(loc_id, i + 1); // ID 0 = background

        // Draw hull submeshes
        if (!mesh.subMeshes.empty()) {
            for (const auto& sub : mesh.subMeshes) {
                if (sub.VAO == 0 || sub.vertexCount == 0) continue;
                glBindVertexArray(sub.VAO);
                glDrawArrays(GL_TRIANGLES, 0, sub.vertexCount);
            }
        } else if (mesh.VAO) {
            glBindVertexArray(mesh.VAO);
            glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        }

        // Render ATTA sub-models into the pick buffer, each with its OWN unique
        // pick ID (kAttaPickBase + entry). Pure MEF attachments (ceiling lights,
        // wall art, panels, crates that exist only in the building model and NOT
        // in objects.qsc) become individually clickable — the app promotes them
        // into real, editable EditRigidObj tasks. ATTAs that already have a
        // matching EditRigidObj are skipped (occupancy check inside).
        {
            glm::mat4 parentWorldMat(1.0f);
            parentWorldMat = glm::translate(parentWorldMat, glm::vec3(obj.pos));
            parentWorldMat = glm::rotate(parentWorldMat, (float)obj.rot.z, glm::vec3(0.f, 0.f, 1.f));
            parentWorldMat = glm::rotate(parentWorldMat, (float)obj.rot.x, glm::vec3(1.f, 0.f, 0.f));
            parentWorldMat = glm::rotate(parentWorldMat, (float)obj.rot.y, glm::vec3(0.f, 1.f, 0.f));
            std::unordered_set<std::string> drawn;
            DrawAttachmentsForPicking(obj.modelId, obj.isBuilding, parentWorldMat, obj.scale,
                                      loc_model, loc_id, i, drawn);
            glUseProgram(pick_shader_prog_);
            glBindBufferBase(GL_UNIFORM_BUFFER, ubo_binding_point_, ubo_mats);
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore state
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
}

// ─── PickObjectAtScreen ───────────────────────────────────────────────────────
int Renderer_Objects::PickObjectAtScreen(int x, int y, int w, int h,
                                          GLuint ubo_mats,
                                          const std::vector<LevelObject>& objects,
                                          int draw_parts,
                                          const glm::vec3& camera_pos)
{
    if (!pick_shader_prog_ || w <= 0 || h <= 0) return -1;

    // Resize FBO if window size changed
    if (w != pick_fbo_w_ || h != pick_fbo_h_) {
        InitPickingFBO(w, h);
    }
    if (!pick_fbo_) return -1;

    DrawForPicking(ubo_mats, objects, draw_parts, camera_pos);

    // Read back the single pixel under the cursor.
    // OpenGL origin is bottom-left; screen coords are top-left → flip Y.
    uint8_t pixel[3] = {0, 0, 0};
    glBindFramebuffer(GL_FRAMEBUFFER, pick_fbo_);
    glReadPixels(x, h - y - 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int id = (static_cast<int>(pixel[0]) << 16) |
             (static_cast<int>(pixel[1]) <<  8) |
              static_cast<int>(pixel[2]);

    return (id == 0) ? -1 : id - 1;
}

// ─── Draw ─────────────────────────────────────────────────────────────────────
void Renderer_Objects::Draw(GLuint ubo_mats, bool overlay_wireframe,
                            const std::vector<LevelObject>& objects, int selected_object_index, int hover_object_index, int draw_parts,
                            const glm::vec3& camera_pos, bool show_magic_obj_spheres)
{
    // Define the flags (must match renderer.h)
    const int DRAW_OBJECTS = 4;
    const int DRAW_BUILDINGS = 16;
    const int DRAW_PROPS = 32;

    if (objects.empty()) {
        static bool logged_once = false;
        if (!logged_once) {
            Logger::Get().Log(LogLevel::WARNING, "[Renderer_Objects] Draw called with EMPTY object list!");
            logged_once = true;
        }
        return;
    }

    EnsureWindowModelIdsLoaded();

    if (!shader_program_) return;

    // Rebuild EditRigidObj occupancy so any ATTA that has been promoted to (or is
    // duplicated by) a real EditRigidObj is suppressed in the attachment render —
    // otherwise the promoted object would draw on top of its original ATTA.
    editrigid_occupancy_.clear();
    suppressed_atta_keys_.clear();
    for (const auto& o : objects) {
        if (o.deleted || o.type != "EditRigidObj" || o.modelId.empty()) continue;
        editrigid_occupancy_.insert(AttaOccupancyKey(o.modelId, glm::vec3(o.pos)));
        if (o.name.rfind("ATTA:", 0) == 0) {
            suppressed_atta_keys_.insert(o.name.substr(5));
        }
    }

    // Bind our object shader
    glUseProgram(shader_program_);

    // Bind the shared UBO (same buffer terrain uses — contains proj + view)
    glBindBufferBase(GL_UNIFORM_BUFFER, ubo_binding_point_, ubo_mats);

    // OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE); // Disable backface culling to fix invisible interior walls
    glDisable(GL_BLEND);

    // Wireframe toggle
    if (overlay_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Uniform locations (fetched once per draw call — fast enough)
    GLint loc_model    = glGetUniformLocation(shader_program_, "u_model");
    GLint loc_dirlight = glGetUniformLocation(shader_program_, "u_dirlight");
    GLint loc_ambient  = glGetUniformLocation(shader_program_, "u_ambient");
    GLint loc_useTex   = glGetUniformLocation(shader_program_, "u_useTexture");
    GLint loc_tex      = glGetUniformLocation(shader_program_, "u_texture");
    GLint loc_alpha    = glGetUniformLocation(shader_program_, "u_alpha");
    GLint loc_baseColor = glGetUniformLocation(shader_program_, "u_baseColor");
    GLint loc_tint     = glGetUniformLocation(shader_program_, "u_tint");
    loc_glass_min_ = glGetUniformLocation(shader_program_, "u_glassMin");
    glUniform1f(loc_alpha, 1.0f); // default: fully opaque
    glUniform1f(loc_glass_min_, 0.0f); // default: not glass
    glUniform4f(loc_baseColor, 1.0f, 1.0f, 1.0f, 1.0f); // default: white
    glUniform3f(loc_tint, 1.0f, 1.0f, 1.0f); // default: no tint

    EnsurePortalDistancesLoaded();

    for (int pass = 0; pass < 2; ++pass) {
        bool isTransparentPass = (pass == 1);
        if (isTransparentPass) {
            glDepthMask(GL_FALSE);
        } else {
            glDepthMask(GL_TRUE);
        }

        for (const auto& obj : objects) {
            if (obj.deleted) continue;

            // Reset tint to white at the start of EVERY iteration so a magenta tint
            // set for a previous object can never leak into this one, regardless of
            // which early-continue path this object takes (issue 2).
            glUniform3f(loc_tint, 1.0f, 1.0f, 1.0f);

            // Selective rendering logic
        bool shouldDraw = false;
        if (draw_parts & DRAW_OBJECTS) {
            shouldDraw = true; // Draw everything
        } else {
            if ((draw_parts & DRAW_BUILDINGS) && obj.isBuilding) shouldDraw = true;
            if ((draw_parts & DRAW_PROPS) && !obj.isBuilding) shouldDraw = true;
        }

        if (!shouldDraw) continue;

        // SplineObjWaypoints and SplineObj containers are rendered by renderer_splines.cpp
        if (obj.isSplineWaypoint || obj.isSplineContainer) continue;

        // Skip empty modelId and collision-only proxies
        if (obj.modelId.empty()) continue;
        if (obj.modelId == "colbox" || obj.name == "colbox" ||
            obj.modelId == "colbox2" || obj.name == "colbox2" ||
            obj.modelId == "colbox4" || obj.name == "colbox4" ||
            obj.modelId == "colbox66" || obj.name == "colbox66") {
            continue;
        }

        Mesh mesh = GetOrLoadMesh(obj.modelId, obj.isBuilding);
        if (mesh.vertexCount == 0) continue;

        // Flag objects whose model is absent from the level .res: tint magenta so the
        // user sees it will be invisible in-game (issue 2). Reset to white at the top
        // of the next iteration guarantees this never leaks to other objects.
        if (obj.modelMissingInRes)
            glUniform3f(loc_tint, 1.0f, 0.2f, 1.0f); // magenta: missing in level .res

        // ── Build model matrix ────────────────────────────────────────────────
        // We now place it exactly at its world position (obj.pos) and apply its own rotation (obj.rot).

        glm::mat4 model = glm::mat4(1.0f);

        // 3. Scale
        float base_scale = 40.96f;
        float total_scale = base_scale * obj.scale;

        // 1. Translate to world position
        // obj.pos.z already includes the terrain snap offset from app.cpp,
        // so we use it directly without adding zOffset again.
        model = glm::translate(model, glm::vec3(obj.pos.x, obj.pos.y, obj.pos.z));

        // 2. Apply IGI rotations (Yaw, Pitch, Roll)
        // IGI rotation order: Yaw (Z), then Pitch (X), then Roll (Y)
        if (IsMetalSlideUpDoorModel(obj.modelId)) {
            // Slide-up metal doors (levels 12/13/14) carry genuine multi-axis Euler
            // tuples and seat correctly only with Z->Y->X order. Yaw-only objects are
            // order-invariant, so scoping this to the door model regresses nothing else.
            // FALLBACK if still wrong: try order X->Y->Z, or append a +/-90° spin about Z.
            model = glm::rotate(model, (float)obj.rot.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Yaw
            model = glm::rotate(model, (float)obj.rot.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Roll
            model = glm::rotate(model, (float)obj.rot.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
        } else {
            model = glm::rotate(model, (float)obj.rot.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Yaw
            model = glm::rotate(model, (float)obj.rot.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
            model = glm::rotate(model, (float)obj.rot.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Roll
        }

        // For weapons, they are authored with Y-up (legacy OBJ style), so they stand upright.
        // We rotate them by 90 degrees on Pitch (X axis) to lay them flat on the ground.
        bool isWeapon = IsWeaponModel(obj.modelId) || obj.type == "GunPickup" || obj.type == "AmmoPickup";
        if (isWeapon) {
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }

        // 3. Scale
        model = glm::scale(model, glm::vec3(total_scale));

        // Native MEF geometry is already authored in the game's coordinate space.
        // Do not apply the legacy OBJ Y-up -> Z-up correction here or buildings/AI
        // will stand on the wrong axis.

        // Upload model matrix
        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));

        // ── Lighting and Color ────────────────────────────────────────────────
        // Compute a hash-based fallback color for models/submeshes without textures.
        float r = 0.5f, g = 0.5f, b = 0.5f;
        if (!obj.modelId.empty()) {
            size_t hash = std::hash<std::string>{}(obj.modelId);
            r = 0.4f + (float)(hash & 0xFF) / 255.0f * 0.4f;
            g = 0.4f + (float)((hash >> 8) & 0xFF) / 255.0f * 0.4f;
            b = 0.4f + (float)((hash >> 16) & 0xFF) / 255.0f * 0.4f;
        }

        // Hull buildings that have no textures of their own but carry ATTA sub-models are
        // underground container shells — skip rendering them entirely. The ATTA sub-models
        // supply all visible geometry; rendering a hash-colored hull on top obscures them.
        bool skipHullRender = false;
        bool hasAnyTexture = (mesh.textureID > 0);
        if (!hasAnyTexture) {
            for (const auto& sub : mesh.subMeshes) {
                if (sub.textureID > 0) { hasAnyTexture = true; break; }
            }
        }
        
        // Skip any model built from collision fallback geometry (XTVC/ECFC — no XTRV/DNER
        // render vertices). These produce misshapen meshes with fabricated UVs and render
        // as flat-colored or wrongly-tiled boxes. Covers vehicle hulls, train cargo objects,
        // and any other collision-only model regardless of whether it has ATTA children.
        if (!mesh.fromRenderMesh) {
            skipHullRender = true;
        }
        // Underground/hollow building shells (no own textures, ATTA children) are
        // now rendered OPAQUE like the game (the user's reference shows solid walls).
        // The old semi-transparent treatment (0.25 / 0.65) made Level 12 look like a
        // see-through mess. Kept as a flag = false so the transparent-pass routing
        // below treats them as ordinary opaque geometry.
        bool isUndergroundContainer = false;

        // Is this a window/glass model? If so, render the whole mesh semi-transparent.
        const bool isWindowModel = window_model_ids_.count(obj.modelId) > 0;
        const bool isTransparentObject = isWindowModel || isUndergroundContainer;

        // Does this model have any ARGB sub-meshes (alphaMode==2)?  Those must
        // render in the transparent pass so they properly blend over the background
        // with depth-writes disabled. Opaque sub-meshes of the same model still
        // render in the opaque pass — mixed models (guard tower, fence posts) draw
        // in BOTH passes, skipping the wrong-pass sub-meshes each time.
        // Tree billboard meshes (900/902/905 series) use alpha-test discard, not
        // alpha-blend. Force them to the opaque pass so the shader's discard line
        // fires (u_alpha=1.0 ≥ 0.9) and transparent areas vanish instead of showing
        // as semi-transparent gray rectangles.
        const bool isTreeModel = (obj.modelId == "900_01_1" ||
                                  obj.modelId == "902_01_1" ||
                                  obj.modelId == "905_01_1");

        bool hasArgbSubMeshes = false;
        if (!isTreeModel) {
            for (const auto& sub : mesh.subMeshes) {
                if (sub.alphaMode == 2) { hasArgbSubMeshes = true; break; }
            }
        }
        if (current_level_ == 12 && !isWindowModel) {
            hasArgbSubMeshes = false;
        }
        // A model belongs in this pass if: it's a uniform transparent/opaque
        // object that matches the pass, OR it has mixed sub-meshes and we draw
        // it in both passes (filtering per-sub-mesh below).
        const bool drawInThisPass = !skipHullRender && (
            isTransparentObject == isTransparentPass ||
            (hasArgbSubMeshes && !isTransparentObject)
        );
        if (drawInThisPass) {
            if (isTransparentObject) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                // Window glass: 0.55 gives visible reflectivity while still see-through.
                // Underground shell: 0.65 makes it visibly semi-transparent.
                glUniform1f(loc_alpha, isUndergroundContainer ? 0.65f : 0.55f);
                // Clean (clear) glass panes get a sheen floor so they never vanish.
                if (isWindowModel) glUniform1f(loc_glass_min_, 0.30f);
            }

            // Pull hull surfaces slightly toward camera to prevent Z-fighting with terrain.
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0f, -1.0f);

            // Draw each submesh with its own texture and lighting
            if (!mesh.subMeshes.empty()) {
                // For mixed textured/untextured meshes, skip large untextured
                // submeshes that are likely foundations (they should be underground).
                int maxTexturedVerts = 0;
                bool hasTextured = false, hasUntextured = false;
                for (const auto& sub : mesh.subMeshes) {
                    if (sub.textureID > 0) {
                        hasTextured = true;
                        maxTexturedVerts = std::max(maxTexturedVerts, sub.vertexCount);
                    } else {
                        hasUntextured = true;
                    }
                }
                bool mixedMesh = hasTextured && hasUntextured;
                for (const auto& sub : mesh.subMeshes) {
                    if (sub.VAO == 0 || sub.vertexCount == 0) continue;
                    (void)mixedMesh; // render all submeshes — floors/stories must not be skipped

                    // For mixed models: ARGB sub-meshes only in transparent pass,
                    // opaque sub-meshes only in opaque pass (unless the whole model
                    // is a transparent object, in which case all sub-meshes go through).
                    if (!isTransparentObject && hasArgbSubMeshes) {
                        if (isTransparentPass && sub.alphaMode != 2) continue;
                        if (!isTransparentPass && sub.alphaMode == 2) continue;
                    }

                    bool isBlendSub = (sub.alphaMode == 2);
                    if (current_level_ == 12 && !isWindowModel) {
                        isBlendSub = false;
                    }
                    if (isTreeModel) isBlendSub = false;

                    if (isBlendSub) {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        float subAlpha = sub.baseColorFactor.a;
                        glUniform1f(loc_alpha, subAlpha);
                        // Per-sub-mesh depth-write in the transparent pass. Only genuinely
                        // see-through glass (low alpha) skips depth writes so it stays clear
                        // from every angle; alpha-tested foliage / near-opaque panes keep
                        // writing depth so leaves stay solid and don't blend into dark blobs.
                        // (A previous opaque frame sub-mesh could otherwise leave depth-writes
                        //  on for a following glass pane, making the front window opaque.)
                        if (isTransparentPass)
                            glDepthMask(subAlpha < 0.6f ? GL_FALSE : GL_TRUE);
                        else if (subAlpha >= 0.99f)
                            glDepthMask(GL_TRUE);
                    }

                    if (sub.textureID > 0) {
                        // Textured submesh: neutral lighting so the texture looks natural.
                        // Windows/glass keep their transparency (alpha 0.4 above) but render
                        // with the SAME normal lighting as everything else, so glass stays
                        // clear and see-through. The earlier flat-gray override (dirlight 0 /
                        // ambient 0.45) darkened panes into murky panels — reverted to the
                        // pre-3986cd9 "before" look the user asked to restore.
                        glUniform3f(loc_dirlight, 0.6f, 0.6f, 0.6f);
                        glUniform3f(loc_ambient,  0.4f, 0.4f, 0.4f);
                        glUniform1i(loc_useTex, 1);
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, sub.textureID);
                        glUniform1i(loc_tex, 0);
                    } else {
                        // Untextured submesh: use material baseColorFactor if available,
                        // otherwise fall back to the hash-based color.
                        glm::vec3 color(sub.baseColorFactor.r, sub.baseColorFactor.g, sub.baseColorFactor.b);
                        if (color.r >= 0.99f && color.g >= 0.99f && color.b >= 0.99f) {
                            color = glm::vec3(r, g, b);
                        }
                        glUniform3f(loc_dirlight, color.r * 0.6f, color.g * 0.6f, color.b * 0.6f);
                        glUniform3f(loc_ambient,  color.r * 0.4f, color.g * 0.4f, color.b * 0.4f);
                        glUniform1i(loc_useTex, 0);
                    }

                    glBindVertexArray(sub.VAO);
                    glDrawArrays(GL_TRIANGLES, 0, sub.vertexCount);

                    if (isBlendSub) {
                        glDisable(GL_BLEND);
                        glUniform1f(loc_alpha, 1.0f);
                    }
                }
                glBindVertexArray(0);
            } else {
                // Legacy single-texture path (e.g. old OBJ models)
                bool hasTexture = (mesh.textureID > 0);
                if (hasTexture) {
                    glUniform3f(loc_dirlight, 0.6f, 0.6f, 0.6f);
                    glUniform3f(loc_ambient,  0.4f, 0.4f, 0.4f);
                } else {
                    glUniform3f(loc_dirlight, 0.7f, 0.7f, 0.7f);
                    glUniform3f(loc_ambient,  r * 0.4f, g * 0.4f, b * 0.4f);
                }
                if (mesh.textureID > 0) {
                    glUniform1i(loc_useTex, 1);
                    GL_BindTexture2D(0, mesh.textureID);
                    glUniform1i(loc_tex, 0);
                } else {
                    glUniform1i(loc_useTex, 0);
                }
                renderModel(mesh);
            }

            if (isTransparentObject) {
                glDisable(GL_BLEND);
                glUniform1f(loc_alpha, 1.0f);
                glUniform1f(loc_glass_min_, 0.0f); // clear glass sheen for next object
            }

            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        // Only buildings with portal/ATTA sub-models need distance-based culling.
        // Non-buildings (AI, static props, containers) never have ATTA records.
        bool isCloseEnough = true;
        if (obj.isBuilding && Config::Get().enableLOD) {
            float distToCamera = glm::distance(camera_pos, glm::vec3(obj.pos));
            float portalDistance = 100.0f;
            if (portal_distances_.count(obj.modelId)) {
                portalDistance = portal_distances_[obj.modelId];
            }
            isCloseEnough = (distToCamera <= (portalDistance * WORLD_UNITS_PER_METER));
        }

        // ── Render ATTA sub-models (recursively) ─────────────────────────────
        // Any object type (Buildings, Vehicles, Magic Models, Doors, Weapons, etc.) can have ATTA sub-models.
        std::string prefix = obj.isBuilding ? "building:" : "object:";
        std::string attCacheKey = std::to_string(current_level_) + ":" + prefix + obj.modelId;
        bool hasAttachments = attachment_cache_.find(attCacheKey) != attachment_cache_.end();
        
        if (hasAttachments && isCloseEnough && !isWeapon) {
            if (attachment_cache_.find(attCacheKey) != attachment_cache_.end()) {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(-2.0f, -2.0f); // Prevent z-fighting with hull walls

                // Build the root object's unscaled world matrix (translate + rotate)
                glm::mat4 parentRot(1.0f);
                parentRot = glm::rotate(parentRot, (float)obj.rot.z, glm::vec3(0,0,1));
                parentRot = glm::rotate(parentRot, (float)obj.rot.x, glm::vec3(1,0,0));
                parentRot = glm::rotate(parentRot, (float)obj.rot.y, glm::vec3(0,1,0));

                glm::mat4 rootWorldMat(1.0f);
                rootWorldMat = glm::translate(rootWorldMat, glm::vec3((float)obj.pos.x, (float)obj.pos.y, (float)obj.pos.z));
                rootWorldMat = rootWorldMat * parentRot;

                std::unordered_set<std::string> drawn;
                DrawAttachmentsRecursive(obj.modelId, obj.modelId, obj.isBuilding, rootWorldMat, isTransparentPass,
                                          loc_model, loc_dirlight, loc_ambient,
                                          loc_useTex, loc_tex, loc_alpha, drawn);

                // DrawAttachmentsRecursive may leave GL_BLEND enabled and
                // depth-writes disabled for ARGB (glass/alpha) sub-meshes.
                // Reset both so the next object in the outer loop renders correctly
                // (prevents tree leaves and other alpha-tested geometry from
                //  appearing transparent when a building attachment drew before them).
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glUniform1f(loc_alpha, 1.0f);
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
        }

        // Companion parts (_XX_2.._XX_5) are NOT rendered alongside the main body.
        // Main body models (_XX_1) already contain a complete character mesh including
        // the face. Companion parts duplicate the head geometry at a different depth,
        // which produces a visible double-face overdraw artifact.
    }
    }

    // Always reset polygon mode after draw
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthFunc(GL_LEQUAL); // Restore global depth func changed above to GL_LESS
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Unbind shader
    glUseProgram(0);

    if (show_magic_obj_spheres) {
        DrawMagicObjSpheres(objects, ubo_mats);
    }
}

float Renderer_Objects::GetMeshZOffset(const std::string& modelId, bool isBuilding) {
    std::string cacheKey = std::to_string(current_level_) + ":" + (isBuilding ? "building:" : "object:") + modelId;
    auto it = mesh_cache_.find(cacheKey);
    Mesh mesh;
    if (it != mesh_cache_.end()) {
        mesh = it->second;
    } else {
        mesh = GetOrLoadMesh(modelId, isBuilding);
    }

    return mesh.mainZOffset;
}

glm::vec3 Renderer_Objects::GetMeshExtents(const std::string& modelId, bool isBuilding) {
    Mesh mesh = GetOrLoadMesh(modelId, isBuilding);
    return mesh.halfExtents;
}

void Renderer_Objects::DrawModelPreview(const std::string& modelId, GLuint ubo_mats,
                                        int vpX, int vpY, int vpW, int vpH,
                                        float rotX, float rotY) {
    if (!shader_program_ || modelId.empty() || vpW <= 0 || vpH <= 0) return;

    // Picker model IDs are almost all props; fall back to the building variant.
    Mesh mesh = GetOrLoadMesh(modelId, false);
    if (mesh.subMeshes.empty() && mesh.vertexCount == 0)
        mesh = GetOrLoadMesh(modelId, true);
    if (mesh.subMeshes.empty() && (mesh.VAO == 0 || mesh.vertexCount == 0)) return;

    // Fit the model into a unit-ish sphere so any model frames the same.
    float maxExt = std::max(std::max(mesh.halfExtents.x, mesh.halfExtents.y),
                            std::max(mesh.halfExtents.z, 1.0f));
    float fit = 1.0f / maxExt;

    // Preview camera: IGI models are Z-up. View from -Y, slightly raised, at origin.
    glm::mat4 proj = glm::perspective(glm::radians(40.0f),
                                      (float)vpW / (float)vpH, 0.05f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, -3.2f, 1.2f),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 mvp = proj * view;

    glm::mat4 model(1.0f);
    model = glm::rotate(model, rotY, glm::vec3(0.0f, 0.0f, 1.0f)); // horizontal spin (Z up)
    model = glm::rotate(model, rotX, glm::vec3(1.0f, 0.0f, 0.0f)); // vertical tumble
    model = glm::scale(model, glm::vec3(fit));
    model = glm::translate(model, -mesh.center);

    // Overwrite u_mvp (3rd mat4 in the shared UBO). The scene re-uploads the whole
    // UBO next frame, and the scene pass for this frame already finished, so this is safe.
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_mats);
    glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4),
                    glm::value_ptr(mvp));

    // Isolated viewport with its own cleared depth so the preview never z-fights HUD.
    glViewport(vpX, vpY, vpW, vpH);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vpX, vpY, vpW, vpH);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader_program_);
    glBindBufferBase(GL_UNIFORM_BUFFER, ubo_binding_point_, ubo_mats);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    GLint loc_model     = glGetUniformLocation(shader_program_, "u_model");
    GLint loc_dirlight  = glGetUniformLocation(shader_program_, "u_dirlight");
    GLint loc_ambient   = glGetUniformLocation(shader_program_, "u_ambient");
    GLint loc_useTex    = glGetUniformLocation(shader_program_, "u_useTexture");
    GLint loc_tex       = glGetUniformLocation(shader_program_, "u_texture");
    GLint loc_alpha     = glGetUniformLocation(shader_program_, "u_alpha");
    GLint loc_glass     = glGetUniformLocation(shader_program_, "u_glassMin");
    GLint loc_baseColor = glGetUniformLocation(shader_program_, "u_baseColor");
    glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1f(loc_alpha, 1.0f);
    if (loc_glass >= 0)     glUniform1f(loc_glass, 0.0f);
    if (loc_baseColor >= 0) glUniform4f(loc_baseColor, 1.0f, 1.0f, 1.0f, 1.0f);

    // Hash-based fallback color for untextured submeshes (matches the main draw).
    float r = 0.6f, g = 0.6f, b = 0.6f;
    {
        size_t h = std::hash<std::string>{}(modelId);
        r = 0.4f + (float)(h & 0xFF) / 255.0f * 0.4f;
        g = 0.4f + (float)((h >> 8) & 0xFF) / 255.0f * 0.4f;
        b = 0.4f + (float)((h >> 16) & 0xFF) / 255.0f * 0.4f;
    }

    auto drawSub = [&](GLuint vao, int vcount, GLuint texId, const glm::vec4& baseColor) {
        if (vao == 0 || vcount == 0) return;
        if (texId > 0) {
            glUniform3f(loc_dirlight, 0.7f, 0.7f, 0.7f);
            glUniform3f(loc_ambient,  0.45f, 0.45f, 0.45f);
            glUniform1i(loc_useTex, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texId);
            glUniform1i(loc_tex, 0);
        } else {
            glm::vec3 c(baseColor.r, baseColor.g, baseColor.b);
            if (c.r >= 0.99f && c.g >= 0.99f && c.b >= 0.99f) c = glm::vec3(r, g, b);
            glUniform3f(loc_dirlight, c.r * 0.7f, c.g * 0.7f, c.b * 0.7f);
            glUniform3f(loc_ambient,  c.r * 0.45f, c.g * 0.45f, c.b * 0.45f);
            glUniform1i(loc_useTex, 0);
        }
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vcount);
    };

    if (!mesh.subMeshes.empty()) {
        for (const auto& sub : mesh.subMeshes)
            drawSub(sub.VAO, sub.vertexCount, sub.textureID, sub.baseColorFactor);
    } else {
        drawSub(mesh.VAO, mesh.vertexCount, mesh.textureID, glm::vec4(1.0f));
    }

    glBindVertexArray(0);
    glDisable(GL_SCISSOR_TEST);
    // Caller restores full viewport and 2D HUD state.
}

// ─── LoadAttachmentsRecursive ────────────────────────────────────────────────
// Recursively scans the ATTA section of modelId's MEF file, caches each
// sub-model mesh AND its own attachment records, then recurses into children.
// The visited set prevents infinite loops on circular ATTA references.
bool Renderer_Objects::IsVehicleType(const std::string& type) {
    return type == "Car" || type == "Heli" || type == "Plane" || type == "Train";
}

// ─── GetOrLoadMesh ────────────────────────────────────────────────────────────
Mesh Renderer_Objects::GetOrLoadMesh(const std::string& modelId, bool isBuilding) {
    std::string cacheKey = std::to_string(current_level_) + ":" + (isBuilding ? "building:" : "object:") + modelId;
    Logger::Get().Log(LogLevel::DEBUG,
        "[Renderer_Objects] GetOrLoadMesh request cacheKey=" + cacheKey + " modelId=" + modelId);

    // Return cached mesh if already loaded
    auto it = mesh_cache_.find(cacheKey);
    if (it != mesh_cache_.end()) {
        Logger::Get().Log(LogLevel::DEBUG,
            "[Renderer_Objects] Cache hit for " + cacheKey +
            " vertexCount=" + std::to_string(it->second.vertexCount));
        return it->second;
    }

    // Find the file on disk
    std::string filepath = FindModelFile(modelId, isBuilding);
    if (filepath.empty()) {
        Logger::Get().Log(LogLevel::WARNING, "[Renderer_Objects] Model search FAILED for ID: " + modelId + ". Skipping render.");
        Mesh emptyMesh;
        emptyMesh.vertexCount = 0;
        mesh_cache_[cacheKey] = emptyMesh;
        return mesh_cache_[cacheKey];
    }
    Logger::Get().Log(LogLevel::DEBUG,
        "[Renderer_Objects] Resolved modelId=" + modelId + " to path=" + filepath);


    // Load and cache
    try {
        Mesh mesh = loadObjModel(filepath, "");
        ApplyTexturesToMesh(mesh, modelId);
        mesh_cache_[cacheKey] = mesh;
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Success: Loaded model '" + modelId + "' from " + filepath + " (" + std::to_string(mesh.vertexCount) + " vertices)");

        // Recursively parse ATTA records for this model and all nested children.
        std::unordered_set<std::string> visited;
        LoadAttachmentsRecursive(modelId, isBuilding, visited);

        // If the hull has no texture but ATTA sub-models do, inherit the first available
        // ATTA texture for the hull — this covers vehicles/magic objects whose collision
        // shell has no DAT entry but whose interior parts share the same texture sheet.
        {
            Mesh& cached = mesh_cache_[cacheKey];
            bool hullHasTex = (cached.textureID > 0);
            if (!hullHasTex) {
                for (const auto& sub : cached.subMeshes) {
                    if (sub.textureID > 0) { hullHasTex = true; break; }
                }
            }
            if (!hullHasTex) {
                std::string prefix = isBuilding ? "building:" : "object:";
                std::string attKey = std::to_string(current_level_) + ":" + prefix + modelId;
                GLuint inheritedTex = 0;
                auto ait = attachment_cache_.find(attKey);
                if (ait != attachment_cache_.end()) {
                    for (const auto& att : ait->second) {
                        std::string subKey = std::to_string(current_level_) + ":" + prefix + att.modelId;
                        auto sit = mesh_cache_.find(subKey);
                        if (sit != mesh_cache_.end()) {
                            for (const auto& sub : sit->second.subMeshes) {
                                if (sub.textureID > 0) { inheritedTex = sub.textureID; break; }
                            }
                        }
                        if (inheritedTex) break;
                    }
                }
                if (inheritedTex) {
                    for (auto& sub : cached.subMeshes) sub.textureID = inheritedTex;
                    cached.textureID = inheritedTex;
                    Logger::Get().Log(LogLevel::INFO,
                        "[TEX Native] Hull '" + modelId + "' has no texture; inherited from ATTA sub-model glId=" + std::to_string(inheritedTex));
                }
            }
        }

        return mesh_cache_[cacheKey];

    } catch (const std::exception& e) {
        Logger::Get().Log(LogLevel::ERR, "[Renderer_Objects] Load FAILED for " + modelId + ": " + std::string(e.what()));
        Mesh emptyMesh;
        mesh_cache_[cacheKey] = emptyMesh;
        return mesh_cache_[cacheKey];
    }

}


Mesh Renderer_Objects::CreateTextMesh(const std::string& text) {
    Mesh m;
    std::vector<float> vertices;
    
    // Simple stroke font - each character defined as line segments
    // Scale factor for text - reduced to be less intrusive
    const float scale = 20.0f;
    const float charSpacing = 25.0f;
    const float startX = -((text.size() * charSpacing) / 2.0f);
    
    float cursorX = startX;
    
    for (char c : text) {
        AddCharacterVertices(vertices, c, cursorX, 0.0f, scale);
        cursorX += charSpacing;
    }
    
    if (vertices.empty()) {
        // Fallback to a simple cross if text generation fails
        vertices = {
            -50.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
             50.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
             0.0f, -50.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
             0.0f,  50.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f
        };
    }
    
    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);
    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Position (xyz)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal (xyz) - use as color multiplier
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)12);
    glEnableVertexAttribArray(1);
    // TexCoord (uv) - unused
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)24);
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
    m.vertexCount = vertices.size() / 8;
    m.textureID = 0;
    m.vertexData = nullptr;
    return m;
}

void Renderer_Objects::AddCharacterVertices(std::vector<float>& vertices, char c, float x, float y, float scale) {
    // Simple stroke definitions for common characters - using triangle strips for each line
    // Each line segment becomes a thin quad (2 triangles)
    auto addLineQuad = [&](float x1, float y1, float x2, float y2) {
        float thickness = 0.1f;
        float nx = 1.0f, ny = 0.0f, nz = 0.0f; // Red color
        
        // Calculate perpendicular offset for thickness
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = sqrt(dx*dx + dy*dy);
        if (len < 0.0001f) return;
        float ox = -dy / len * thickness;
        float oy = dx / len * thickness;
        
        // Quad vertices (2 triangles)
        // Triangle 1
        vertices.push_back(x + x1 * scale + ox); vertices.push_back(y + y1 * scale + oy); vertices.push_back(0.0f);
        vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
        vertices.push_back(0.0f); vertices.push_back(0.0f);
        
        vertices.push_back(x + x2 * scale + ox); vertices.push_back(y + y2 * scale + oy); vertices.push_back(0.0f);
        vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
        vertices.push_back(0.0f); vertices.push_back(0.0f);
        
        vertices.push_back(x + x1 * scale - ox); vertices.push_back(y + y1 * scale - oy); vertices.push_back(0.0f);
        vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
        vertices.push_back(0.0f); vertices.push_back(0.0f);
        
        // Triangle 2
        vertices.push_back(x + x1 * scale - ox); vertices.push_back(y + y1 * scale - oy); vertices.push_back(0.0f);
        vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
        vertices.push_back(0.0f); vertices.push_back(0.0f);
        
        vertices.push_back(x + x2 * scale + ox); vertices.push_back(y + y2 * scale + oy); vertices.push_back(0.0f);
        vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
        vertices.push_back(0.0f); vertices.push_back(0.0f);
        
        vertices.push_back(x + x2 * scale - ox); vertices.push_back(y + y2 * scale - oy); vertices.push_back(0.0f);
        vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
        vertices.push_back(0.0f); vertices.push_back(0.0f);
    };
    
    c = toupper(c);
    
    switch (c) {
        case 'M':
            addLineQuad(0, 1, 0, -1);
            addLineQuad(0, -1, 0.5, 0);
            addLineQuad(0.5, 0, 1, -1);
            addLineQuad(1, -1, 1, 1);
            break;
        case 'I':
            addLineQuad(0, 1, 0, -1);
            addLineQuad(-0.3, 1, 0.3, 1);
            addLineQuad(-0.3, -1, 0.3, -1);
            break;
        case 'S':
            addLineQuad(0.3, 1, 0, 1);
            addLineQuad(0, 1, 0, 0);
            addLineQuad(0, 0, 1, 0);
            addLineQuad(1, 0, 1, -1);
            addLineQuad(1, -1, 0.7, -1);
            break;
        case 'N':
            addLineQuad(0, -1, 0, 1);
            addLineQuad(0, 1, 1, -1);
            addLineQuad(1, -1, 1, 1);
            break;
        case 'G':
            addLineQuad(0.7, 1, 0, 1);
            addLineQuad(0, 1, 0, -1);
            addLineQuad(0, -1, 0.7, -1);
            addLineQuad(0.7, -1, 1, -1);
            addLineQuad(1, -1, 1, 0);
            addLineQuad(1, 0, 0.7, 0);
            break;
        case ':':
            addLineQuad(0, 0.3, 0, 0.3);
            addLineQuad(0, -0.3, 0, -0.3);
            break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            // Simple box for numbers with slash
            addLineQuad(0, 1, 0, -1);
            addLineQuad(0, -1, 1, -1);
            addLineQuad(1, -1, 1, 1);
            addLineQuad(1, 1, 0, 1);
            addLineQuad(0.2, -0.8, 0.8, -0.2);
            break;
        case '_':
            addLineQuad(0, -1, 1, -1);
            break;
        default:
            // Default to a box for unknown characters
            addLineQuad(0, 1, 0, -1);
            addLineQuad(0, -1, 1, -1);
            addLineQuad(1, -1, 1, 1);
            addLineQuad(1, 1, 0, 1);
            break;
    }
}

Mesh Renderer_Objects::CreateCubeMesh() {
    Mesh m;
    std::vector<float> v = {
        -500.0f,-500.0f,-500.0f, 0,0,-1, 0,0,  500.0f, 500.0f,-500.0f, 0,0,-1, 1,1,  500.0f,-500.0f,-500.0f, 0,0,-1, 1,0,
        -500.0f,-500.0f,-500.0f, 0,0,-1, 0,0, -500.0f, 500.0f,-500.0f, 0,0,-1, 0,1,  500.0f, 500.0f,-500.0f, 0,0,-1, 1,1,
        -500.0f,-500.0f, 500.0f, 0,0, 1, 0,0,  500.0f,-500.0f, 500.0f, 0,0, 1, 1,0,  500.0f, 500.0f, 500.0f, 0,0, 1, 1,1,
        -500.0f,-500.0f, 500.0f, 0,0, 1, 0,0,  500.0f, 500.0f, 500.0f, 0,0, 1, 1,1, -500.0f, 500.0f, 500.0f, 0,0, 1, 0,1,
        -500.0f, 500.0f, 500.0f,-1,0, 0, 1,0, -500.0f, 500.0f,-500.0f,-1,0, 0, 1,1, -500.0f,-500.0f,-500.0f,-1,0, 0, 0,1,
        -500.0f, 500.0f, 500.0f,-1,0, 0, 1,0, -500.0f,-500.0f,-500.0f,-1,0, 0, 0,1, -500.0f,-500.0f, 500.0f,-1,0, 0, 0,0,
         500.0f, 500.0f, 500.0f, 1,0, 0, 1,0,  500.0f,-500.0f,-500.0f, 1,0, 0, 0,1,  500.0f, 500.0f,-500.0f, 1,0, 0, 1,1,
         500.0f, 500.0f, 500.0f, 1,0, 0, 1,0,  500.0f,-500.0f, 500.0f, 1,0, 0, 0,0,  500.0f,-500.0f,-500.0f, 1,0, 0, 0,1,
        -500.0f,-500.0f,-500.0f, 0,-1,0, 0,1,  500.0f,-500.0f,-500.0f, 0,-1,0, 1,1,  500.0f,-500.0f, 500.0f, 0,-1,0, 1,0,
        -500.0f,-500.0f,-500.0f, 0,-1,0, 0,1,  500.0f,-500.0f, 500.0f, 0,-1,0, 1,0, -500.0f,-500.0f, 500.0f, 0,-1,0, 0,0,
        -500.0f, 500.0f,-500.0f, 0, 1,0, 0,1,  500.0f, 500.0f, 500.0f, 0, 1,0, 1,0,  500.0f, 500.0f,-500.0f, 0, 1,0, 1,1,
        -500.0f, 500.0f,-500.0f, 0, 1,0, 0,1, -500.0f, 500.0f, 500.0f, 0, 1,0, 0,0,  500.0f, 500.0f, 500.0f, 0, 1,0, 1,0
    };
    glGenVertexArrays(1, &m.VAO); glGenBuffers(1, &m.VBO);
    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size()*4, v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24); glEnableVertexAttribArray(2);
    m.vertexCount = 36; m.textureID = 0; m.vertexData = nullptr;
    glBindVertexArray(0); return m;
}

std::string Renderer_Objects::FindModelFile(const std::string& modelId, bool isBuilding) {
    if (modelId.empty()) return "";

    // Helper: search one directory for exact modelId.mef match.
    auto searchOneDirExact = [&](const std::string& dirStr) -> std::string {
        if (dirStr.empty()) return "";
        std::filesystem::path modelsPath(dirStr);
        if (!std::filesystem::exists(modelsPath)) return "";

        // Exact match
        std::filesystem::path exactPath = modelsPath / (modelId + ".mef");
        if (std::filesystem::exists(exactPath)) return exactPath.string();
        return "";
    };

    // Helper: search one directory for fuzzy match by type prefix.
    auto searchOneDirFuzzy = [&](const std::string& dirStr) -> std::string {
        if (dirStr.empty()) return "";
        std::filesystem::path modelsPath(dirStr);
        if (!std::filesystem::exists(modelsPath)) return "";

        // Companion-part guard: IDs ending in _2 .. _9 (face, hands, legs, etc.)
        // must match exactly or not at all. The fuzzy fallback would otherwise
        // return the main body mesh (_01_1) for any missing companion part,
        // causing double-body / double-face rendering.
        {
            const size_t lastUs = modelId.rfind('_');
            if (lastUs != std::string::npos && lastUs + 1 < modelId.size()) {
                const char lastCh = modelId[lastUs + 1];
                if (lastCh >= '2' && lastCh <= '9') return "";
            }
        }

        // Fuzzy scan
        std::string bestMatch;
        for (const auto& entry : std::filesystem::directory_iterator(modelsPath)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".mef") continue;
            const std::string fname = entry.path().filename().string();
            const std::string stem  = entry.path().stem().string();

            // Full modelId in filename
            if (fname.find(modelId) != std::string::npos) return entry.path().string();

            // Variation fallback: require type prefix at stem start (e.g. "003_" not "1003_")
            size_t firstUnderscore = modelId.find_first_of('_');
            if (firstUnderscore != std::string::npos) {
                const std::string typeId = modelId.substr(0, firstUnderscore);
                if (stem.rfind(typeId + "_", 0) == 0) {
                    if (bestMatch.empty() || stem.find(typeId + "_01") != std::string::npos) {
                        bestMatch = entry.path().string();
                    }
                }
            }
        }
        return bestMatch;
    };

    // ─── PHASE 1: EXACT MATCHES ───────────────────────────────────────────────
    
    // 1. Search local extracted models for current level
    const std::string exeModels = Utils::GetExeDirectory() + "\\editor\\models\\level" + std::to_string(current_level_);
    std::string result = searchOneDirExact(exeModels);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found locally (Exact): " + result);
        return result;
    }

    // 2. Fall back to game's native models directory for current level
    const std::string igiModels = Utils::GetIGIModelsPath(current_level_);
    result = searchOneDirExact(igiModels);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in IGI root (Exact): " + result);
        return result;
    }

    // 3. Search other levels for exact match (cross-level references)
    for (int lvl = 1; lvl <= 14; ++lvl) {
        if (lvl == current_level_) continue;
        const std::string lvlLocal = Utils::GetExeDirectory() + "\\editor\\models\\level" + std::to_string(lvl);
        result = searchOneDirExact(lvlLocal);
        if (!result.empty()) {
            Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in level" + std::to_string(lvl) + " local (Exact): " + result);
            return result;
        }
        const std::string lvlIgi = Utils::GetIGIModelsPath(lvl);
        result = searchOneDirExact(lvlIgi);
        if (!result.empty()) {
            Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in level" + std::to_string(lvl) + " IGI (Exact): " + result);
            return result;
        }
    }

    // 4. Search common location0 assets
    const std::string commonLocal = Utils::GetExeDirectory() + "\\editor\\models\\common";
    result = searchOneDirExact(commonLocal);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in common local (Exact): " + result);
        return result;
    }
    const std::string commonIgi = Utils::GetIGIRootPath() + "\\missions\\location0\\common\\models";
    result = searchOneDirExact(commonIgi);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in common IGI (Exact): " + result);
        return result;
    }

    // 5. Search the flat IGI editor/models directory produced by --extract-level.
    //    All levels are extracted to a single flat dir (D:/IGI1/editor/models/) alongside
    //    the common/ and level1/ subdirs, so this covers levels 2-14 model MEFs.
    const std::string igiContentModels = Utils::GetIGIRootPath() + "\\editor\\models";
    result = searchOneDirExact(igiContentModels);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in IGI editor/models flat dir (Exact): " + result);
        return result;
    }

    // ─── PHASE 2: FUZZY FALLBACK (Only run if exact match fails everywhere) ───

    // 1. Current level local
    result = searchOneDirFuzzy(exeModels);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found locally (Fuzzy): " + result);
        return result;
    }

    // 2. Current level IGI root
    result = searchOneDirFuzzy(igiModels);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in IGI root (Fuzzy): " + result);
        return result;
    }

    // Lazy On-Demand Extraction check
    EnsureGlobalTextureMapLoaded();
    auto mit = model_level_map_.find(modelId);
    if (mit != model_level_map_.end()) {
        int targetLvl = mit->second;
        if (targetLvl != current_level_) {
            Logger::Get().Log(LogLevel::INFO,
                "[Renderer_Objects] Lazy extracting textures/models on-demand for level " + std::to_string(targetLvl) +
                " to resolve cross-level model: " + modelId);
            AssetExtractor::EnsureLevelAssets(targetLvl, Utils::GetIGIRootPath(), Utils::GetExeDirectory());
        }
    }

    // 3. Other levels fuzzy
    for (int lvl = 1; lvl <= 14; ++lvl) {
        if (lvl == current_level_) continue;
        const std::string lvlLocal = Utils::GetExeDirectory() + "\\editor\\models\\level" + std::to_string(lvl);
        result = searchOneDirFuzzy(lvlLocal);
        if (!result.empty()) {
            Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in level" + std::to_string(lvl) + " local (Fuzzy): " + result);
            return result;
        }
        const std::string lvlIgi = Utils::GetIGIModelsPath(lvl);
        result = searchOneDirFuzzy(lvlIgi);
        if (!result.empty()) {
            Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in level" + std::to_string(lvl) + " IGI (Fuzzy): " + result);
            return result;
        }
    }

    // 4. Common fuzzy
    result = searchOneDirFuzzy(commonLocal);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in common local (Fuzzy): " + result);
        return result;
    }
    result = searchOneDirFuzzy(commonIgi);
    if (!result.empty()) {
        Logger::Get().Log(LogLevel::DEBUG, "[Renderer_Objects] Model found in common IGI (Fuzzy): " + result);
        return result;
    }

    Logger::Get().Log(LogLevel::ERR,
        "[Renderer_Objects] Model NOT FOUND: '" + modelId +
        "' — searched level " + std::to_string(current_level_) +
        ", all other levels, and common folder.");
    return "";
}


// ─── InitSelectionBox ────────────────────────────────────────────────────────
void Renderer_Objects::InitSelectionBox() {
    // Create a simple cube wireframe
    float boxSize = WORLD_UNITS_PER_METER * 2.0f; // 2m box
    float halfSize = boxSize * 0.5f;
    
    float vertices[] = {
        // Front face
        -halfSize, -halfSize,  halfSize,
         halfSize, -halfSize,  halfSize,
         halfSize,  halfSize,  halfSize,
        -halfSize,  halfSize,  halfSize,
        // Back face
        -halfSize, -halfSize, -halfSize,
         halfSize, -halfSize, -halfSize,
         halfSize,  halfSize, -halfSize,
        -halfSize,  halfSize, -halfSize
    };
    
    glGenVertexArrays(1, &selection_vao_);
    glGenBuffers(1, &selection_vbo_);
    
    glBindVertexArray(selection_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, selection_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
}

// ─── DrawSelectionBox ─────────────────────────────────────────────────────────
void Renderer_Objects::DrawSelectionBox(const LevelObject& obj, GLuint ubo_mats, const glm::vec4& color) {
    if (selection_vao_ == 0) {
        InitSelectionBox();
    }
    
    // Simple shader for solid color
    static const char* simple_vert = R"(
#version 330 core
layout(std140) uniform Matrices {
    mat4 u_unused1;
    mat4 u_unused2;
    mat4 u_mvp;
};
uniform mat4 u_model;
layout(location = 0) in vec3 a_pos;
void main() {
    gl_Position = u_mvp * u_model * vec4(a_pos, 1.0);
}
)";

    static const char* simple_frag = R"(
#version 330 core
uniform vec4 u_color;
out vec4 fragColor;
void main() {
    fragColor = u_color;
}
)";
    
    static GLuint simple_shader = 0;
    if (simple_shader == 0) {
        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert, 1, &simple_vert, nullptr);
        glCompileShader(vert);
        
        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &simple_frag, nullptr);
        glCompileShader(frag);
        
        simple_shader = glCreateProgram();
        glAttachShader(simple_shader, vert);
        glAttachShader(simple_shader, frag);
        glLinkProgram(simple_shader);
        
        glDeleteShader(vert);
        glDeleteShader(frag);
    }
    
    glUseProgram(simple_shader);
    glBindBufferBase(GL_UNIFORM_BUFFER, ubo_binding_point_, ubo_mats);
    
    // Build model matrix for selection box (slightly larger than object)
    // Cast dvec3 to vec3 for rendering
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(obj.pos));
    model = glm::rotate(model, static_cast<float>(obj.rot.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::rotate(model, static_cast<float>(obj.rot.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, static_cast<float>(obj.rot.x), glm::vec3(1.0f, 0.0f, 0.0f));

    // For weapons, they are authored with Y-up (legacy OBJ style), so they stand upright.
    // We rotate them by 90 degrees on Pitch (X axis) to lay them flat on the ground.
    bool isWeapon = IsWeaponModel(obj.modelId) || obj.type == "GunPickup" || obj.type == "AmmoPickup";
    if (isWeapon) {
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    model = glm::scale(model, glm::vec3(obj.scale * 1.2f)); // 20% larger
    
    GLint loc_model = glGetUniformLocation(simple_shader, "u_model");
    glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));
    
    GLint loc_color = glGetUniformLocation(simple_shader, "u_color");
    glUniform4fv(loc_color, 1, glm::value_ptr(color));
    
    // Draw wireframe
    glBindVertexArray(selection_vao_);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    
    // Draw edges
    GLuint indices[] = {
        0,1, 1,2, 2,3, 3,0, // Front face
        4,5, 5,6, 6,7, 7,4, // Back face
        0,4, 1,5, 2,6, 3,7  // Connecting edges
    };
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // Unbind any existing EBO
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, indices);
    
    // Reset state
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
    glUseProgram(0);
}

// ─── DrawAttachmentsForSpline ─────────────────────────────────────────────────
// Public entry point called by Renderer_Splines once per SEGMENT INTERVAL.
// unscaledWorldMat = translate + rotate at the segment midpoint (no scale).
// leafScale.x = intervalLen (stretch ATTA to cover the full segment in X).
// leafScale.y/z = 40.96 (normal world scale for width/height).
