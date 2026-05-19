#include "mef_native.h"

#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Internal types
// ---------------------------------------------------------------------------

namespace {

struct ChunkInfo {
    std::string name;
    uint32_t size  = 0;
    uint32_t align = 0;
    uint32_t skip  = 0;
    size_t   start = 0;
    size_t   data  = 0;
};

struct D3drInfo {
    uint32_t numFaces  = 0;
    uint32_t numMeshes = 0;
    uint32_t verts0    = 0;
    uint32_t verts1    = 0;
    uint32_t numVerts  = 0;
    uint32_t rawSize   = 0;
    bool     valid     = false;
};

constexpr uint32_t kIlffHeaderSize  = 20;
constexpr uint32_t kChunkHeaderSize = 16;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

template <typename T>
T ReadValue(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + sizeof(T) > bytes.size()) {
        throw std::runtime_error("Unexpected end of MEF stream");
    }
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

std::vector<uint8_t> ReadWholeFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open MEF file: " + filepath);
    }

    const std::streamsize size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to read MEF file size: " + filepath);
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error("Failed to read MEF file bytes: " + filepath);
    }

    return bytes;
}

// ---------------------------------------------------------------------------
// ILFF chunk parsing
// ---------------------------------------------------------------------------

std::vector<ChunkInfo> ParseIlffChunks(const std::vector<uint8_t>& bytes, const std::string& filepath) {
    if (bytes.size() < kIlffHeaderSize) {
        throw std::runtime_error("MEF file too small for ILFF header: " + filepath);
    }

    if (std::memcmp(bytes.data(), "ILFF", 4) != 0) {
        throw std::runtime_error("Invalid MEF magic header (expected ILFF): " + filepath);
    }

    const uint32_t declaredSize = ReadValue<uint32_t>(bytes, 4);
    const uint32_t align        = ReadValue<uint32_t>(bytes, 8);
    const uint32_t skip         = ReadValue<uint32_t>(bytes, 12);

    if (declaredSize != bytes.size()) {
        throw std::runtime_error("ILFF size mismatch in: " + filepath);
    }
    if (align != 4) {
        throw std::runtime_error("Unsupported ILFF alignment in: " + filepath);
    }
    if (skip != 0) {
        throw std::runtime_error("Unsupported ILFF skip field in: " + filepath);
    }

    std::vector<ChunkInfo> chunks;
    size_t pos = kIlffHeaderSize;

    while (pos + kChunkHeaderSize <= bytes.size()) {
        ChunkInfo chunk;
        chunk.start = pos;
        chunk.name.assign(reinterpret_cast<const char*>(bytes.data() + pos), 4);
        chunk.size  = ReadValue<uint32_t>(bytes, pos + 4);
        chunk.align = ReadValue<uint32_t>(bytes, pos + 8);
        chunk.skip  = ReadValue<uint32_t>(bytes, pos + 12);
        chunk.data  = pos + kChunkHeaderSize;

        if (chunk.data + chunk.size > bytes.size()) {
            throw std::runtime_error("MEF chunk exceeds file bounds in: " + filepath);
        }

        chunks.push_back(chunk);

        if (chunk.skip == 0) {
            pos = chunk.data + chunk.size;
            break;
        }

        const size_t nextPos = chunk.start + chunk.skip;
        if (nextPos <= chunk.start || nextPos + kChunkHeaderSize > bytes.size()) {
            throw std::runtime_error("Invalid ILFF chunk link in: " + filepath);
        }

        pos = nextPos;
    }

    if (chunks.empty()) {
        throw std::runtime_error("MEF file contains no ILFF chunks: " + filepath);
    }

    return chunks;
}

std::string DescribeChunks(const std::vector<ChunkInfo>& chunks) {
    std::string text;
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        if (!text.empty()) {
            text += " | ";
        }
        text += chunk.name +
            "(size=" + std::to_string(chunk.size) +
            ", skip=" + std::to_string(chunk.skip) +
            ", data=" + std::to_string(chunk.data) + ")";
    }
    return text;
}

const ChunkInfo* FindChunk(const std::vector<ChunkInfo>& chunks, std::string_view name, size_t ordinal = 0) {
    size_t seen = 0;
    for (const auto& chunk : chunks) {
        if (chunk.name == name) {
            if (seen == ordinal) {
                return &chunk;
            }
            ++seen;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// HSEM / D3DR header parsers
// ---------------------------------------------------------------------------

uint32_t ReadModelType(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    const ChunkInfo* hsem = FindChunk(chunks, "HSEM");
    if (!hsem || hsem->size < 36) {
        return 0;
    }
    return ReadValue<uint32_t>(bytes, hsem->data + 32);
}

D3drInfo ReadD3drInfo(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks, uint32_t modelType) {
    D3drInfo info;
    const ChunkInfo* d3dr = FindChunk(chunks, "D3DR");
    if (!d3dr) {
        return info;
    }

    info.rawSize = d3dr->size;
    if (d3dr->size < 16) {
        return info;
    }

    info.valid = true;
    if (modelType == 1) {
        if (d3dr->size < 24) {
            info.valid = false;
            return info;
        }
        info.numFaces  = ReadValue<uint32_t>(bytes, d3dr->data + 4);
        info.numMeshes = ReadValue<uint32_t>(bytes, d3dr->data + 8);
        info.verts0    = ReadValue<uint32_t>(bytes, d3dr->data + 12);
        info.verts1    = ReadValue<uint32_t>(bytes, d3dr->data + 16);
        info.numVerts  = ReadValue<uint32_t>(bytes, d3dr->data + 20);
    } else if (modelType == 3) {
        if (d3dr->size < 20) {
            info.valid = false;
            return info;
        }
        info.numFaces  = ReadValue<uint32_t>(bytes, d3dr->data + 8);
        info.numMeshes = ReadValue<uint32_t>(bytes, d3dr->data + 12);
        info.numVerts  = ReadValue<uint32_t>(bytes, d3dr->data + 16);
    } else {
        info.numFaces  = ReadValue<uint32_t>(bytes, d3dr->data + 4);
        info.numMeshes = ReadValue<uint32_t>(bytes, d3dr->data + 8);
        info.numVerts  = ReadValue<uint32_t>(bytes, d3dr->data + 12);
    }

    return info;
}

// ---------------------------------------------------------------------------
// XTRV vertex parser
// ---------------------------------------------------------------------------

std::vector<RenderVertex> ParseRenderVertices(const std::vector<uint8_t>& bytes, const ChunkInfo& chunk, uint32_t modelType) {
    uint32_t vertexSize = 0;
    uint32_t uvOffset   = 24; // default: pos(12) + normal(12) + uv(8)
    switch (modelType) {
    case 0:
        vertexSize = 32;
        uvOffset   = 24;  // pos(12) + normal(12) + uv(8)
        break;
    case 1:
        vertexSize = 40;
        uvOffset   = 24;  // pos(12) + normal(12) + uv0(8) + w(4) + vn(2) + bn(2)
        break;
    case 3:
        vertexSize = 40;
        uvOffset   = 24;  // pos(12) + normal(12) + uv(8) + ...
        break;
    default:
        throw std::runtime_error("Unsupported MEF modelType in XTRV");
    }

    const size_t count = chunk.size / vertexSize;
    std::vector<RenderVertex> vertices(count);

    for (size_t i = 0; i < count; ++i) {
        const size_t base = chunk.data + i * vertexSize;

        vertices[i].pos = glm::vec3(
            ReadValue<float>(bytes, base + 0),
            ReadValue<float>(bytes, base + 4),
            ReadValue<float>(bytes, base + 8)
        ) * kMefNativeScale;

        // Normal is present for all model types at +12..+23
        vertices[i].normal = glm::vec3(
            ReadValue<float>(bytes, base + 12),
            ReadValue<float>(bytes, base + 16),
            ReadValue<float>(bytes, base + 20)
        );

        vertices[i].uv = glm::vec2(
            ReadValue<float>(bytes, base + uvOffset),
            ReadValue<float>(bytes, base + uvOffset + 4)
        );

        if (modelType == 1) {
            vertices[i].weight        = ReadValue<float>   (bytes, base + 32);
            vertices[i].localVertexId = ReadValue<uint16_t>(bytes, base + 36);
            vertices[i].boneIndex     = ReadValue<uint16_t>(bytes, base + 38);
        }
    }

    return vertices;
}

// ---------------------------------------------------------------------------
// DNER/ECAF triangle parsers
// ---------------------------------------------------------------------------

std::vector<std::array<uint32_t, 3>> ParsePackedRenderTriangles(
    const std::vector<uint8_t>& bytes,
    const ChunkInfo& chunk,
    uint32_t modelType,
    std::vector<ParsedGeometry::RenderBlock>& outBlocks,
    size_t& outBlockCount)
{
    const size_t headerSize = (modelType == 3) ? 32 : 28;
    std::vector<std::array<uint32_t, 3>> triangles;
    size_t cursor    = 0;
    size_t blockCount = 0;

    while (cursor + headerSize <= chunk.size) {
        const size_t base          = chunk.data + cursor;
        const int16_t  indexCount  = ReadValue<int16_t> (bytes, base + 12);
        const int16_t  nextoffs    = ReadValue<int16_t> (bytes, base + 14);
        const int16_t  materialSlot = (modelType == 3)
            ? ReadValue<int16_t>(bytes, base + 16)
            : static_cast<int16_t>(blockCount);
        const uint16_t vertsOffset = (modelType == 3)
            ? ReadValue<uint16_t>(bytes, base + 20)
            : ReadValue<uint16_t>(bytes, base + 18);
        const uint16_t vertsCount  = (modelType == 3)
            ? ReadValue<uint16_t>(bytes, base + 22)
            : ReadValue<uint16_t>(bytes, base + 20);
        (void)vertsCount; // used for diagnostics only
        const size_t indexBytes    = (indexCount > 0) ? static_cast<size_t>(indexCount) * sizeof(uint16_t) : 0;

        if (cursor + headerSize + indexBytes > chunk.size) {
            break;
        }

        const size_t facesBase         = base + headerSize;
        const size_t blockTriangleStart = triangles.size();

        for (int16_t i = 0; i + 2 < indexCount; i += 3) {
            const uint16_t a = ReadValue<uint16_t>(bytes, facesBase + static_cast<size_t>(i + 0) * sizeof(uint16_t));
            const uint16_t b = ReadValue<uint16_t>(bytes, facesBase + static_cast<size_t>(i + 1) * sizeof(uint16_t));
            const uint16_t c = ReadValue<uint16_t>(bytes, facesBase + static_cast<size_t>(i + 2) * sizeof(uint16_t));
            triangles.push_back({
                static_cast<uint32_t>(vertsOffset) + a,
                static_cast<uint32_t>(vertsOffset) + b,
                static_cast<uint32_t>(vertsOffset) + c
            });
        }

        const size_t blockTriangleCount = triangles.size() - blockTriangleStart;
        if (blockTriangleCount > 0) {
            outBlocks.push_back({ blockTriangleStart, blockTriangleCount, materialSlot });
        }

        ++blockCount;
        cursor += headerSize + indexBytes;
        if (nextoffs == -1) {
            break;
        }
    }

    outBlockCount = blockCount;
    return triangles;
}

std::vector<std::array<uint32_t, 3>> ParseSplitBoneTriangles(
    const std::vector<uint8_t>& bytes,
    const ChunkInfo& dnerChunk,
    const ChunkInfo& ecafChunk,
    const D3drInfo& d3drInfo,
    std::vector<ParsedGeometry::RenderBlock>& outBlocks,
    size_t& outBlockCount)
{
    // Bone DNER records in IGI 1 are consistently 32 bytes for models using ECAF.
    const size_t kBoneRecordSize = 32;
    const size_t blockCount      = d3drInfo.numMeshes;

    if (dnerChunk.size < blockCount * kBoneRecordSize) {
        return {};
    }

    std::vector<std::array<uint32_t, 3>> triangles;
    const size_t totalIndicesInECAF = ecafChunk.size / sizeof(uint16_t);

    for (size_t block = 0; block < blockCount; ++block) {
        const size_t base = dnerChunk.data + block * kBoneRecordSize;

        // IGI 1 Bone DNER Layout:
        //  0-11: floats
        // 12-13: numFace (embedded indices count, usually 0)
        // 14-15: nextOffs
        // 16-17: indexOffset (into ECAF chunk, in indices)
        // 18-19: triangleCount
        // 20-21: vertsOffset
        // 22-23: vertsCount

        const uint16_t u12         = ReadValue<uint16_t>(bytes, base + 12);
        const uint16_t u14         = ReadValue<uint16_t>(bytes, base + 14);
        const uint16_t indexOffset = ReadValue<uint16_t>(bytes, base + 16);
        const uint16_t u18         = ReadValue<uint16_t>(bytes, base + 18);
        const uint16_t vertsOffset = ReadValue<uint16_t>(bytes, base + 20);

        // IGI 1 bone models have inconsistent field usage for face/triangle counts.
        size_t triangleCount = 0;
        if (u12 > 0)      triangleCount = u12 / 3;
        else if (u18 > 0) triangleCount = u18;
        else if (u14 > 0) triangleCount = u14;

        const size_t blockTriangleStart = triangles.size();

        for (size_t tri = 0; tri < triangleCount; ++tri) {
            const size_t firstIndex = static_cast<size_t>(indexOffset) + tri * 3;
            if (firstIndex + 2 >= totalIndicesInECAF) break;

            const size_t indexBase = ecafChunk.data + firstIndex * sizeof(uint16_t);
            const uint16_t a = ReadValue<uint16_t>(bytes, indexBase + 0);
            const uint16_t b = ReadValue<uint16_t>(bytes, indexBase + 2);
            const uint16_t c = ReadValue<uint16_t>(bytes, indexBase + 4);

            // Winding order: IGI 1 bone models use CW winding in raw data.
            // Swap to {a, c, b} to match reference dconv OBJ export and OpenGL CCW expectation.
            triangles.push_back({
                static_cast<uint32_t>(a + vertsOffset),
                static_cast<uint32_t>(c + vertsOffset),
                static_cast<uint32_t>(b + vertsOffset)
            });
        }

        if (triangles.size() > blockTriangleStart) {
            outBlocks.push_back({ blockTriangleStart, triangles.size() - blockTriangleStart, static_cast<int>(block) });
        }
    }

    outBlockCount = outBlocks.size();
    return triangles;
}

// ---------------------------------------------------------------------------
// Collision geometry fallback
// ---------------------------------------------------------------------------

ParsedGeometry ParseCollisionGeometry(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    ParsedGeometry geometry;

    const ChunkInfo* xtvc = FindChunk(chunks, "XTVC");
    const ChunkInfo* ecfc = FindChunk(chunks, "ECFC");
    if (!xtvc || !ecfc) {
        return geometry;
    }

    const size_t vertexCount = xtvc->size / 16;
    geometry.vertices.resize(vertexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        const size_t base = xtvc->data + i * 16;
        const float x = ReadValue<float>(bytes, base + 0);
        const float y = ReadValue<float>(bytes, base + 4);
        const float z = ReadValue<float>(bytes, base + 8);
        geometry.vertices[i].pos = glm::vec3(x, y, z) * kMefNativeScale;
        geometry.vertices[i].uv  = glm::vec2(x * 0.1f, z * 0.1f);
    }

    const size_t faceCount = ecfc->size / 8;
    geometry.triangles.reserve(faceCount);
    for (size_t i = 0; i < faceCount; ++i) {
        const size_t base = ecfc->data + i * 8;
        geometry.triangles.push_back({
            ReadValue<uint16_t>(bytes, base + 0),
            ReadValue<uint16_t>(bytes, base + 2),
            ReadValue<uint16_t>(bytes, base + 4)
        });
    }

    geometry.collisionVertexCount = vertexCount;
    geometry.collisionFaceCount   = faceCount;

    return geometry;
}

// ---------------------------------------------------------------------------
// REIH bone hierarchy — returns vector<BoneInfo>
// ---------------------------------------------------------------------------

std::vector<BoneInfo> ParseBoneHierarchy(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    const ChunkInfo* reih = FindChunk(chunks, "REIH");
    if (!reih || reih->size < 1) return {};

    const size_t sz       = reih->size;
    const size_t align    = sz % 13;
    const size_t numBones = (sz - align) / 13;
    const size_t p2Offset = reih->data + numBones + align;

    if (numBones == 0 || p2Offset + numBones * 12 > bytes.size()) return {};

    std::vector<BoneInfo> bones(numBones);
    std::vector<uint8_t> numChildArr(numBones);
    for (size_t i = 0; i < numBones; ++i) {
        numChildArr[i]      = bytes[reih->data + i];
        bones[i].numChild   = numChildArr[i];
        bones[i].pivot.x    = ReadValue<float>(bytes, p2Offset + i * 12 + 0);
        bones[i].pivot.y    = ReadValue<float>(bytes, p2Offset + i * 12 + 4);
        bones[i].pivot.z    = ReadValue<float>(bytes, p2Offset + i * 12 + 8);
    }

    // Reconstruct parent indices using breadth-first queue logic:
    // Bones are stored in level-order (BFS) traversal. Each node's children are
    // assigned to the next consecutive bone IDs in the flat array.
    bones[0].parent = -1;
    {
        // queue stores (boneId, remainingChildren)
        std::vector<std::pair<int,int>> bfsQueue;
        bfsQueue.reserve(numBones);
        bfsQueue.push_back({ 0, static_cast<int>(numChildArr[0]) });
        size_t head = 0;
        size_t nextBone = 1;
        while (head < bfsQueue.size() && nextBone < numBones) {
            auto& [parentId, remaining] = bfsQueue[head];
            if (remaining == 0) {
                ++head;
                continue;
            }
            --remaining;
            bones[nextBone].parent = parentId;
            bfsQueue.push_back({ static_cast<int>(nextBone),
                                  static_cast<int>(numChildArr[nextBone]) });
            ++nextBone;
        }
    }

    return bones;
}

// ---------------------------------------------------------------------------
// Helper: compute world positions from BoneInfo (for vertex baking)
// ---------------------------------------------------------------------------

std::vector<glm::vec3> ComputeBoneWorldPositions(const std::vector<BoneInfo>& bones) {
    const size_t numBones = bones.size();
    std::vector<glm::vec3> worldPos(numBones);
    for (size_t i = 0; i < numBones; ++i) {
        glm::vec3 acc = bones[i].pivot;
        int p = bones[i].parent;
        while (p != -1 && p < static_cast<int>(numBones)) { acc += bones[p].pivot; p = bones[p].parent; }
        worldPos[i] = acc;
    }
    return worldPos;
}

// ---------------------------------------------------------------------------
// MANB bone names
// ---------------------------------------------------------------------------

std::vector<std::string> ParseManbNames(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    const ChunkInfo* manb = FindChunk(chunks, "MANB");
    if (!manb || manb->size == 0) return {};

    const size_t count = manb->size / 16;
    std::vector<std::string> names(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t base = manb->data + i * 16;
        size_t len = 0;
        while (len < 16 && bytes[base + len] != 0) ++len;
        names[i] = std::string(reinterpret_cast<const char*>(bytes.data() + base), len);
    }
    return names;
}

// ---------------------------------------------------------------------------
// ATTA attachment records (72 bytes each)
// ---------------------------------------------------------------------------

std::vector<Attachment> ParseAttachments(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    const ChunkInfo* atta = FindChunk(chunks, "ATTA");
    if (!atta || atta->size == 0) return {};

    const size_t stride = 72;
    const size_t count  = atta->size / stride;
    std::vector<Attachment> result(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t base = atta->data + i * stride;

        size_t len = 0;
        while (len < 16 && bytes[base + len] != 0) ++len;
        result[i].name = std::string(reinterpret_cast<const char*>(bytes.data() + base), len);

        result[i].pos.x  = ReadValue<float>  (bytes, base + 16);
        result[i].pos.y  = ReadValue<float>  (bytes, base + 20);
        result[i].pos.z  = ReadValue<float>  (bytes, base + 24);
        result[i].boneId = ReadValue<int32_t>(bytes, base + 68);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Main geometry orchestrator
// ---------------------------------------------------------------------------

ParsedGeometry ParseMefGeometry(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    ParsedGeometry geometry;
    const uint32_t modelType = ReadModelType(bytes, chunks);
    geometry.modelType = modelType;
    const D3drInfo d3drInfo = ReadD3drInfo(bytes, chunks, modelType);

    const ChunkInfo* xtrv = FindChunk(chunks, "XTRV");
    const ChunkInfo* dner = FindChunk(chunks, "DNER");
    const ChunkInfo* ecaf = FindChunk(chunks, "ECAF");

    if (xtrv && dner) {
        geometry.vertices = ParseRenderVertices(bytes, *xtrv, modelType);

        if (modelType == 1) {
            geometry.bones = ParseBoneHierarchy(bytes, chunks);
            const std::vector<std::string> names = ParseManbNames(bytes, chunks);
            for (size_t i = 0; i < geometry.bones.size() && i < names.size(); ++i) {
                geometry.bones[i].name = names[i];
            }
            geometry.attachments = ParseAttachments(bytes, chunks);

            // Bake world-space bone offsets into vertex positions
            const std::vector<glm::vec3> boneWorldPos = ComputeBoneWorldPositions(geometry.bones);
            if (!boneWorldPos.empty()) {
                for (auto& v : geometry.vertices) {
                    if (v.boneIndex < static_cast<uint16_t>(boneWorldPos.size())) {
                        v.pos += boneWorldPos[v.boneIndex] * kMefNativeScale;
                    }
                }
            }
        }

        if (modelType == 1 && ecaf && d3drInfo.valid && d3drInfo.numMeshes > 0) {
            geometry.triangles = ParseSplitBoneTriangles(bytes, *dner, *ecaf, d3drInfo, geometry.renderBlocks, geometry.renderBlockCount);
            geometry.renderLayout = "type1 split ECAF/DNER";
            if (geometry.triangles.empty()) {
                // Split path failed, fall back to packed DNER parsing
                geometry.triangles = ParsePackedRenderTriangles(bytes, *dner, modelType, geometry.renderBlocks, geometry.renderBlockCount);
                geometry.renderLayout = "type1 packed DNER (split fallback)";
            }
        } else {
            geometry.triangles = ParsePackedRenderTriangles(bytes, *dner, modelType, geometry.renderBlocks, geometry.renderBlockCount);
            geometry.renderLayout = (modelType == 1) ? "type1 packed DNER" : "packed DNER";
        }

        geometry.fromRenderMesh = !geometry.vertices.empty() && !geometry.triangles.empty();
    }

    if (!geometry.fromRenderMesh) {
        geometry = ParseCollisionGeometry(bytes, chunks);
        geometry.renderLayout = "collision fallback";
    }

    return geometry;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

ParsedGeometry ParseMefFile(const std::string& filepath) {
    const std::vector<uint8_t> bytes  = ReadWholeFile(filepath);
    const std::vector<ChunkInfo> chunks = ParseIlffChunks(bytes, filepath);
    return ParseMefGeometry(bytes, chunks);
}
