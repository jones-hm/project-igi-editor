#include "model.h"
#include "../pch.h"
#include "../logger.h"
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

struct RenderVertex {
    glm::vec3 pos{0.0f};
    glm::vec2 uv{0.0f};
};

struct ParsedGeometry {
    std::vector<RenderVertex> vertices;
    std::vector<std::array<uint32_t, 3>> triangles;
    bool fromRenderMesh = false;
    uint32_t modelType = 0;
    size_t renderBlockCount = 0;
    size_t collisionVertexCount = 0;
    size_t collisionFaceCount = 0;
};

struct ChunkInfo {
    std::string name;
    uint32_t size = 0;
    uint32_t align = 0;
    uint32_t skip = 0;
    size_t start = 0;
    size_t data = 0;
};

constexpr uint32_t kIlffHeaderSize = 20;
constexpr uint32_t kChunkHeaderSize = 16;
constexpr float kNativeMefImportScale = 1.0f / 40.96f;

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

std::vector<ChunkInfo> ParseIlffChunks(const std::vector<uint8_t>& bytes, const std::string& filepath) {
    if (bytes.size() < kIlffHeaderSize) {
        throw std::runtime_error("MEF file too small for ILFF header: " + filepath);
    }

    if (std::memcmp(bytes.data(), "ILFF", 4) != 0) {
        throw std::runtime_error("Invalid MEF magic header (expected ILFF): " + filepath);
    }

    const uint32_t declaredSize = ReadValue<uint32_t>(bytes, 4);
    const uint32_t align = ReadValue<uint32_t>(bytes, 8);
    const uint32_t skip = ReadValue<uint32_t>(bytes, 12);

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
        chunk.size = ReadValue<uint32_t>(bytes, pos + 4);
        chunk.align = ReadValue<uint32_t>(bytes, pos + 8);
        chunk.skip = ReadValue<uint32_t>(bytes, pos + 12);
        chunk.data = pos + kChunkHeaderSize;

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

uint32_t ReadModelType(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    const ChunkInfo* hsem = FindChunk(chunks, "HSEM");
    if (!hsem || hsem->size < 36) {
        return 0;
    }
    return ReadValue<uint32_t>(bytes, hsem->data + 32);
}

std::vector<RenderVertex> ParseRenderVertices(const std::vector<uint8_t>& bytes, const ChunkInfo& chunk, uint32_t modelType) {
    uint32_t vertexSize = 0;
    switch (modelType) {
    case 0:
        vertexSize = 32;
        break;
    case 1:
    case 3:
        vertexSize = 40;
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
        ) * kNativeMefImportScale;
        vertices[i].uv = glm::vec2(
            ReadValue<float>(bytes, base + 24),
            ReadValue<float>(bytes, base + 28)
        );
    }

    return vertices;
}

std::vector<std::array<uint32_t, 3>> ParseRenderTriangles(const std::vector<uint8_t>& bytes, const ChunkInfo& chunk, uint32_t modelType, size_t& outBlockCount) {
    const size_t headerSize = (modelType == 3) ? 32 : 28;
    std::vector<std::array<uint32_t, 3>> triangles;
    size_t cursor = 0;
    size_t blockCount = 0;

    while (cursor + headerSize <= chunk.size) {
        const size_t base = chunk.data + cursor;
        const int16_t indexCount = ReadValue<int16_t>(bytes, base + 12);
        const int16_t nextoffs = ReadValue<int16_t>(bytes, base + 14);
        const uint16_t vertsOffset = (modelType == 3)
            ? ReadValue<uint16_t>(bytes, base + 20)
            : ReadValue<uint16_t>(bytes, base + 18);
        const uint16_t vertsCount = (modelType == 3)
            ? ReadValue<uint16_t>(bytes, base + 22)
            : ReadValue<uint16_t>(bytes, base + 20);
        const size_t indexBytes = (indexCount > 0) ? static_cast<size_t>(indexCount) * sizeof(uint16_t) : 0;

        if (cursor + headerSize + indexBytes > chunk.size) {
            break;
        }

        const size_t facesBase = base + headerSize;
        uint16_t minLocalIndex = std::numeric_limits<uint16_t>::max();
        uint16_t maxLocalIndex = 0;
        for (int16_t i = 0; i + 2 < indexCount; i += 3) {
            const uint16_t a = ReadValue<uint16_t>(bytes, facesBase + static_cast<size_t>(i + 0) * sizeof(uint16_t));
            const uint16_t b = ReadValue<uint16_t>(bytes, facesBase + static_cast<size_t>(i + 1) * sizeof(uint16_t));
            const uint16_t c = ReadValue<uint16_t>(bytes, facesBase + static_cast<size_t>(i + 2) * sizeof(uint16_t));
            minLocalIndex = std::min<uint16_t>(minLocalIndex, std::min(a, std::min(b, c)));
            maxLocalIndex = std::max<uint16_t>(maxLocalIndex, std::max(a, std::max(b, c)));
            triangles.push_back({
                static_cast<uint32_t>(vertsOffset) + a,
                static_cast<uint32_t>(vertsOffset) + b,
                static_cast<uint32_t>(vertsOffset) + c
            });
        }

        Logger::Get().Log(
            LogLevel::INFO,
            "[MEF Binary Native] DNER block=" + std::to_string(blockCount) +
            " indexCount=" + std::to_string(indexCount) +
            " nextoffs=" + std::to_string(nextoffs) +
            " vertsOffset=" + std::to_string(vertsOffset) +
            " vertsCount=" + std::to_string(vertsCount) +
            " localIndexMin=" + std::to_string(indexCount > 0 ? minLocalIndex : 0) +
            " localIndexMax=" + std::to_string(indexCount > 0 ? maxLocalIndex : 0));

        ++blockCount;
        cursor += headerSize + indexBytes;
        if (nextoffs == -1) {
            break;
        }
    }

    outBlockCount = blockCount;
    Logger::Get().Log(
        LogLevel::INFO,
        "[MEF Binary Native] DNER parsed blocks=" + std::to_string(blockCount) +
        " triangles=" + std::to_string(triangles.size()) +
        " headerSize=" + std::to_string(headerSize) +
        " chunkSize=" + std::to_string(chunk.size));

    return triangles;
}

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
        geometry.vertices[i].pos = glm::vec3(x, y, z) * kNativeMefImportScale;
        geometry.vertices[i].uv = glm::vec2(x * 0.1f, z * 0.1f);
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
    geometry.collisionFaceCount = faceCount;

    return geometry;
}

ParsedGeometry ParseMefGeometry(const std::vector<uint8_t>& bytes, const std::vector<ChunkInfo>& chunks) {
    ParsedGeometry geometry;
    const uint32_t modelType = ReadModelType(bytes, chunks);
    geometry.modelType = modelType;

    const ChunkInfo* xtrv = FindChunk(chunks, "XTRV");
    const ChunkInfo* dner = FindChunk(chunks, "DNER");

    if (xtrv && dner) {
        geometry.vertices = ParseRenderVertices(bytes, *xtrv, modelType);
        geometry.triangles = ParseRenderTriangles(bytes, *dner, modelType, geometry.renderBlockCount);
        geometry.fromRenderMesh = !geometry.vertices.empty() && !geometry.triangles.empty();
    }

    if (!geometry.fromRenderMesh) {
        geometry = ParseCollisionGeometry(bytes, chunks);
    }

    return geometry;
}

Mesh BuildMeshFromGeometry(const ParsedGeometry& geometry, const std::string& filepath) {
    if (geometry.vertices.empty() || geometry.triangles.empty()) {
        throw std::runtime_error("MEF file contains no geometry: " + filepath);
    }

    std::vector<float> verts;
    verts.reserve(geometry.triangles.size() * 3 * 8);

    glm::vec3 min_p(std::numeric_limits<float>::max());
    glm::vec3 max_p(std::numeric_limits<float>::lowest());

    for (const auto& tri : geometry.triangles) {
        if (tri[0] >= geometry.vertices.size() ||
            tri[1] >= geometry.vertices.size() ||
            tri[2] >= geometry.vertices.size()) {
            continue;
        }

        const glm::vec3 p0 = geometry.vertices[tri[0]].pos;
        const glm::vec3 p1 = geometry.vertices[tri[1]].pos;
        const glm::vec3 p2 = geometry.vertices[tri[2]].pos;

        glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
        const float len = glm::length(normal);
        if (len > 1e-6f) {
            normal /= len;
        } else {
            normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        auto addVertex = [&](uint32_t index) {
            const RenderVertex& src = geometry.vertices[index];
            verts.push_back(src.pos.x);
            verts.push_back(src.pos.y);
            verts.push_back(src.pos.z);

            min_p.x = std::min(min_p.x, src.pos.x);
            min_p.y = std::min(min_p.y, src.pos.y);
            min_p.z = std::min(min_p.z, src.pos.z);

            max_p.x = std::max(max_p.x, src.pos.x);
            max_p.y = std::max(max_p.y, src.pos.y);
            max_p.z = std::max(max_p.z, src.pos.z);

            verts.push_back(normal.x);
            verts.push_back(normal.y);
            verts.push_back(normal.z);

            verts.push_back(src.uv.x);
            verts.push_back(1.0f - src.uv.y);
        };

        addVertex(tri[0]);
        addVertex(tri[1]);
        addVertex(tri[2]);
    }

    if (verts.empty()) {
        throw std::runtime_error("MEF file resulted in no valid vertices: " + filepath);
    }

    Mesh mesh;
    SubMesh sub;
    sub.textureID = 0;
    sub.alphaMode = 0;
    sub.vertexCount = static_cast<int>(verts.size() / 8);
    sub.baseColorFactor = glm::vec4(1.0f);

    glGenVertexArrays(1, &sub.VAO);
    glGenBuffers(1, &sub.VBO);
    glBindVertexArray(sub.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, sub.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    mesh.subMeshes.push_back(sub);
    mesh.VAO = sub.VAO;
    mesh.VBO = sub.VBO;
    mesh.textureID = 0;
    mesh.vertexCount = sub.vertexCount;
    mesh.halfExtents = (max_p - min_p) * 0.5f;
    mesh.center = (max_p + min_p) * 0.5f;
    // Native MEF now stays in the game's coordinate space, where Z is up.
    // Ground snapping must therefore use the mesh's lowest Z, not lowest Y.
    mesh.zOffset = -min_p.z;
    mesh.mainZOffset = mesh.zOffset;
    mesh.vertexData = nullptr;

    return mesh;
}

} // namespace

Mesh loadObjModel(const std::string& filepath, const std::string& /*unused*/) {
    const std::vector<uint8_t> bytes = ReadWholeFile(filepath);
    const std::vector<ChunkInfo> chunks = ParseIlffChunks(bytes, filepath);
    Logger::Get().Log(
        LogLevel::INFO,
        "[MEF Binary Native] Open file=" + filepath +
        " bytes=" + std::to_string(bytes.size()) +
        " chunks=" + std::to_string(chunks.size()));
    Logger::Get().Log(
        LogLevel::INFO,
        "[MEF Binary Native] Chunk layout: " + DescribeChunks(chunks));

    const ParsedGeometry geometry = ParseMefGeometry(bytes, chunks);
    Logger::Get().Log(
        LogLevel::INFO,
        "[MEF Binary Native] Geometry source=" + std::string(geometry.fromRenderMesh ? "XTRV/DNER" : "XTVC/ECFC fallback") +
        " modelType=" + std::to_string(geometry.modelType) +
        " vertexCount=" + std::to_string(geometry.vertices.size()) +
        " triangleCount=" + std::to_string(geometry.triangles.size()) +
        " renderBlocks=" + std::to_string(geometry.renderBlockCount) +
        " collisionVerts=" + std::to_string(geometry.collisionVertexCount) +
        " collisionFaces=" + std::to_string(geometry.collisionFaceCount) +
        " importScale=" + std::to_string(kNativeMefImportScale));
    Mesh mesh = BuildMeshFromGeometry(geometry, filepath);
    Logger::Get().Log(
        LogLevel::INFO,
        "[MEF Binary Native] Mesh bounds center=(" +
            std::to_string(mesh.center.x) + "," +
            std::to_string(mesh.center.y) + "," +
            std::to_string(mesh.center.z) + ")" +
        " halfExtents=(" +
            std::to_string(mesh.halfExtents.x) + "," +
            std::to_string(mesh.halfExtents.y) + "," +
            std::to_string(mesh.halfExtents.z) + ")" +
        " zOffset=" + std::to_string(mesh.zOffset) +
        " mainZOffset=" + std::to_string(mesh.mainZOffset) +
        " gpuVertices=" + std::to_string(mesh.vertexCount));

    std::cout << "[MEF Binary Native] Loaded: " << filepath
              << " | Vertices: " << mesh.vertexCount << "\n";
    Logger::Get().Log(LogLevel::INFO,
        "[MEF Binary Native] Loaded: " + filepath + " | Vertices: " + std::to_string(mesh.vertexCount));
    return mesh;
}

void renderModel(const Mesh& mesh) {
    if (mesh.subMeshes.empty() && mesh.VAO == 0) return;

    if (!mesh.subMeshes.empty()) {
        for (const auto& sub : mesh.subMeshes) {
            if (sub.VAO == 0 || sub.vertexCount == 0) continue;
            glBindVertexArray(sub.VAO);
            glDrawArrays(GL_TRIANGLES, 0, sub.vertexCount);
        }
        glBindVertexArray(0);
    } else {
        glBindVertexArray(mesh.VAO);
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        glBindVertexArray(0);
    }
}

void destroyModel(Mesh& mesh) {
    for (auto& sub : mesh.subMeshes) {
        if (sub.VBO) glDeleteBuffers(1, &sub.VBO);
        if (sub.VAO) glDeleteVertexArrays(1, &sub.VAO);
        if (sub.textureID) glDeleteTextures(1, &sub.textureID);
        sub.VAO = sub.VBO = sub.textureID = 0;
    }
    mesh.subMeshes.clear();

    if (mesh.vertexData) {
        delete[] mesh.vertexData;
        mesh.vertexData = nullptr;
    }
    mesh.VAO = mesh.VBO = 0;
    mesh.vertexCount = 0;
}
