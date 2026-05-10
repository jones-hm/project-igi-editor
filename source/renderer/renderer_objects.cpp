#include "pch.h"
#include "renderer_objects.h"
#include <iostream>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ─── Shader Sources ───────────────────────────────────────────────────────────
static const char* OBJ_VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(std140) uniform Matrices {
    mat4 u_proj;
    mat4 u_view;
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
    gl_Position     = u_proj * u_view * worldPos;
}
)";

static const char* OBJ_FRAG_SRC = R"(
#version 330 core
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_fragPos;

uniform vec3 u_dirlight;   // directional light RGB
uniform vec3 u_ambient;    // ambient light RGB

out vec4 fragColor;

void main() {
    vec3 lightDir  = normalize(vec3(0.5, 1.0, 0.5));
    float diff     = max(dot(normalize(v_normal), lightDir), 0.0);
    vec3 color     = u_ambient + u_dirlight * diff;
    fragColor      = vec4(color, 1.0);
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

// ─── Constructor / Destructor ─────────────────────────────────────────────────
Renderer_Objects::Renderer_Objects() : shader_program_(0), ubo_binding_point_(0) {}

Renderer_Objects::~Renderer_Objects() {
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────
bool Renderer_Objects::Init() {
    shader_program_ = BuildShaderProgram();
    if (!shader_program_) {
        std::cerr << "[Renderer_Objects] Failed to build shader.\n";
        return false;
    }

    // Bind UBO block "Matrices" to binding point 0
    // This MUST match the binding point used by terrain renderer
    GLuint blockIdx = glGetUniformBlockIndex(shader_program_, "Matrices");
    if (blockIdx != GL_INVALID_INDEX) {
        glUniformBlockBinding(shader_program_, blockIdx, ubo_binding_point_);
        std::cout << "[Renderer_Objects] UBO 'Matrices' bound to point "
                  << ubo_binding_point_ << "\n";
    } else {
        std::cerr << "[Renderer_Objects] WARNING: UBO 'Matrices' block not found.\n";
    }

    std::cout << "[Renderer_Objects] Init OK.\n";
    return true;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────
void Renderer_Objects::Shutdown() {
    for (auto& pair : mesh_cache_)
        destroyModel(pair.second);
    mesh_cache_.clear();

    if (shader_program_) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
}

// ─── Draw ─────────────────────────────────────────────────────────────────────
void Renderer_Objects::Draw(GLuint ubo_mats, bool overlay_wireframe,
                            const std::vector<LevelObject>& objects)
{
    if (objects.empty())    return;
    if (!shader_program_)   return;

    // Bind our object shader
    glUseProgram(shader_program_);

    // Bind the shared UBO (same buffer terrain uses — contains proj + view)
    glBindBufferBase(GL_UNIFORM_BUFFER, ubo_binding_point_, ubo_mats);

    // OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
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

    for (const auto& obj : objects) {
        Mesh mesh = GetOrLoadMesh(obj.modelId);
        if (mesh.vertexCount == 0) continue;

        // ── Build model matrix ────────────────────────────────────────────────
        // IGI world scale: 1 unit = 1/4096 meter
        // We apply 0.001 to match terrain scale (terrain also uses 0.001)
        constexpr float SCALE = 0.001f;

        glm::mat4 model = glm::mat4(1.0f);

        // Translate
        glm::vec3 renderPos = obj.pos * SCALE;
        model = glm::translate(model, renderPos);

        // Rotate — IGI uses ZYX order, rotZ (gamma/yaw) is the primary axis
        model = glm::rotate(model, obj.rot.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Yaw
        model = glm::rotate(model, obj.rot.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Pitch
        model = glm::rotate(model, obj.rot.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Roll

        // Upload model matrix
        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));

        // Upload lighting
        glUniform3f(loc_dirlight, obj.dirlightR, obj.dirlightG, obj.dirlightB);
        glUniform3f(loc_ambient,  obj.ambientR,  obj.ambientG,  obj.ambientB);

        // Draw
        renderModel(mesh);
    }

    // Always reset polygon mode after draw
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Unbind shader
    glUseProgram(0);
}

// ─── GetOrLoadMesh ────────────────────────────────────────────────────────────
Mesh Renderer_Objects::GetOrLoadMesh(const std::string& modelId) {
    // Return cached mesh if already loaded
    auto it = mesh_cache_.find(modelId);
    if (it != mesh_cache_.end())
        return it->second;

    // Find the file on disk
    std::string filepath = FindModelFile(modelId);
    if (filepath.empty()) {
        std::cerr << "[Renderer_Objects] Model file not found: " << modelId << "\n";
        mesh_cache_[modelId] = {0, 0, 0}; // Mark as failed so we don't retry
        return mesh_cache_[modelId];
    }

    // Load and cache
    try {
        Mesh mesh = loadObjModel(filepath);
        mesh_cache_[modelId] = mesh;
        std::cout << "[Renderer_Objects] Loaded: " << filepath
                  << " | Verts: " << mesh.vertexCount << "\n";
        return mesh;
    } catch (const std::exception& e) {
        std::cerr << "[Renderer_Objects] Load failed for " << modelId
                  << ": " << e.what() << "\n";
        mesh_cache_[modelId] = {0, 0, 0};
        return mesh_cache_[modelId];
    }
}

// ─── FindModelFile ────────────────────────────────────────────────────────────
std::string Renderer_Objects::FindModelFile(const std::string& modelId) {
    // Search priority:
    // 1. objects/{modelId}.obj       (converted OBJ)
    // 2. objects/{modelId}.mef       (raw MEF binary)
    // 3. Fuzzy search inside objects/ folder by modelId substring

    const std::vector<std::string> searchPaths = {
        "objects/" + modelId + ".obj",
        "objects/" + modelId + ".mef",
        "output/missions/location0/common/objects/" + modelId + ".obj",
        "output/missions/location0/common/objects/" + modelId + ".mef"
    };

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            std::cout << "[Renderer_Objects] Found model: " << path << "\n";
            return path;
        }
    }

    // Fuzzy fallback — search objects/ folder for filename containing modelId
    if (std::filesystem::exists("objects")) {
        for (const auto& entry : std::filesystem::directory_iterator("objects")) {
            if (!entry.is_regular_file()) continue;
            std::string fname = entry.path().filename().string();
            std::string ext   = entry.path().extension().string();
            if (fname.find(modelId) != std::string::npos &&
               (ext == ".obj" || ext == ".mef"))
            {
                std::cout << "[Renderer_Objects] Fuzzy match: "
                          << entry.path().string() << "\n";
                return entry.path().string();
            }
        }
    }

    return ""; // Not found
}