#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

// Scale applied to raw MEF coordinates (game units -> world units)
constexpr float kMefNativeScale = 1.0f / 40.96f;

struct RenderVertex {
    glm::vec3 pos{0.f};
    glm::vec3 normal{0.f};      // from XTRV bytes +12..+23
    glm::vec2 uv{0.f};
    uint16_t boneIndex{0};
    uint16_t localVertexId{0};  // XTRV.vn @+36
    float    weight{1.0f};      // XTRV.w  @+32
};

struct BoneInfo {
    std::string name;       // from MANB (16 bytes, null-trimmed)
    int parent{-1};         // -1 = root; computed from REIH num_child traversal
    glm::vec3 pivot{0.f};   // raw pivot from REIH part2 (NOT divided by 40.96)
    uint8_t numChild{0};
};

struct Attachment {
    std::string name;       // from ATTA, 16 bytes
    glm::vec3 pos{0.f};     // bytes +16..+27 in 72-byte ATTA record
    int32_t boneId{-1};     // bytes +68..+71 in 72-byte ATTA record
};

struct ParsedGeometry {
    struct RenderBlock {
        size_t triangleStart = 0;
        size_t triangleCount = 0;
        int materialSlot = 0;
    };

    std::vector<RenderVertex> vertices;
    std::vector<std::array<uint32_t, 3>> triangles;
    std::vector<RenderBlock> renderBlocks;
    bool fromRenderMesh = false;
    uint32_t modelType = 0;
    size_t renderBlockCount = 0;
    size_t collisionVertexCount = 0;
    size_t collisionFaceCount = 0;
    std::string renderLayout;
    std::vector<BoneInfo> bones;          // populated from REIH+MANB
    std::vector<Attachment> attachments;  // populated from ATTA
};

// Parse a binary MEF file and return all geometry + bone data.
// Throws std::runtime_error on any fatal parse error.
ParsedGeometry ParseMefFile(const std::string& filepath);
