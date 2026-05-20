#include "mef_exporter.h"
#include "../logger.h"
#include <fstream>
#include <iomanip>
#include <set>

namespace MefExporter {

bool ExportToObj(const ParsedGeometry& geometry, const std::string& outpath) {
    std::ofstream f(outpath);
    if (!f.is_open()) {
        Logger::Get().Log(LogLevel::ERR, "[MefExporter] Failed to open output file: " + outpath);
        return false;
    }

    f << "# IGI Editor MEF -> OBJ Export\n";
    f << "# Vertices: " << geometry.vertices.size() << "\n";
    f << "# Triangles: " << geometry.triangles.size() << "\n";
    f << "# Layout: " << geometry.renderLayout << "\n\n";

    // Write vertices (Z-up coordinate space from game)
    for (const auto& v : geometry.vertices) {
        f << "v " << std::fixed << std::setprecision(6) << v.pos.x << " " << v.pos.y << " " << v.pos.z << "\n";
    }

    // Write UVs
    for (const auto& v : geometry.vertices) {
        // v.uv.y is flipped for OpenGL/OBJ expectation
        f << "vt " << std::fixed << std::setprecision(6) << v.uv.x << " " << (1.0f - v.uv.y) << "\n";
    }

    // Write object name
    f << "\no model_mesh\n";

    // Write faces (1-indexed)
    for (const auto& tri : geometry.triangles) {
        uint32_t a = tri[0] + 1;
        uint32_t b = tri[1] + 1;
        uint32_t c = tri[2] + 1;
        f << "f " << a << "/" << a << " " << b << "/" << b << " " << c << "/" << c << "\n";
    }

    f.close();
    Logger::Get().Log(LogLevel::INFO, "[MefExporter] Successfully exported to: " + outpath);
    return true;
}

bool ExportToMefAscii(const ParsedGeometry& geometry, const std::string& outpath) {
    std::ofstream f(outpath);
    if (!f.is_open()) {
        Logger::Get().Log(LogLevel::ERR, "[MefExporter] Failed to open output file: " + outpath);
        return false;
    }

    f << "// IGI Editor MEF -> MEF ASCII Export\n";
    f << "NewObject(\"model_mesh\");\n\n";

    // Emit one Material() + MaterialShininess() per entry in geometry.tamcRecords (or slot 0 if empty)
    if (!geometry.tamcRecords.empty()) {
        for (size_t i = 0; i < geometry.tamcRecords.size(); ++i) {
            float opacity = geometry.tamcRecords[i].opacity;
            f << std::fixed << std::setprecision(4);
            f << "Material(" << i << ", \"mat_" << i << "\", "
              << opacity << ", 0.0, 0.0, 0.1, 0.1, 0.1, 0.9, 0.9, 0.9, 0.0, 0.0, 0.0, 1);\n";
            f << "MaterialShininess(" << i << ", 0.0);\n";
        }
    } else {
        f << "Material(0, \"mat_0\", 1.0, 0.0, 0.0, 0.1, 0.1, 0.1, 0.9, 0.9, 0.9, 0.0, 0.0, 0.0, 1);\n";
        f << "MaterialShininess(0, 0.0);\n";
    }
    f << "\n";

    const size_t numVerts = geometry.vertices.size();

    // Check if rawPos is populated (i.e. at least one vertex has a non-zero rawPos)
    bool hasRawPos = false;
    for (const auto& v : geometry.vertices) {
        if (v.rawPos.x != 0.f || v.rawPos.y != 0.f || v.rawPos.z != 0.f) {
            hasRawPos = true;
            break;
        }
    }

    // Vertices (rawPos keeps original game-unit coordinates if available)
    for (size_t i = 0; i < numVerts; ++i) {
        const auto& v = geometry.vertices[i];
        glm::vec3 p = hasRawPos ? v.rawPos : (v.pos * 40.96f);
        f << "Vertex(" << i << ", " << p.x << ", " << p.y << ", " << p.z << ");\n";
    }

    // Normals
    for (size_t i = 0; i < numVerts; ++i) {
        const auto& v = geometry.vertices[i];
        f << "Normal(" << i << ", " << v.normal.x << ", " << v.normal.y << ", " << v.normal.z << ");\n";
    }

    // UVs
    for (size_t i = 0; i < numVerts; ++i) {
        const auto& v = geometry.vertices[i];
        f << "UV(" << i << ", " << v.uv.x << ", " << v.uv.y << ");\n";
    }

    f << "\n";

    // Faces: clamp normal indices to valid range
    const int maxNormIdx = numVerts > 0 ? static_cast<int>(numVerts) - 1 : 0;

    auto clampIdx = [&](uint32_t idx) -> int {
        return std::min(static_cast<int>(idx), maxNormIdx);
    };

    if (!geometry.renderBlocks.empty()) {
        for (const auto& block : geometry.renderBlocks) {
            for (size_t i = 0; i < block.triangleCount; ++i) {
                const auto& tri = geometry.triangles[block.triangleStart + i];
                size_t faceIdx = block.triangleStart + i;
                f << "Face(" << faceIdx
                  << ", " << tri[0] << ", " << tri[1] << ", " << tri[2]
                  << ", " << clampIdx(tri[0]) << ", " << clampIdx(tri[1]) << ", " << clampIdx(tri[2])
                  << ", " << block.materialSlot << ");\n";
            }
        }
    } else {
        for (size_t i = 0; i < geometry.triangles.size(); ++i) {
            const auto& tri = geometry.triangles[i];
            f << "Face(" << i
              << ", " << tri[0] << ", " << tri[1] << ", " << tri[2]
              << ", " << clampIdx(tri[0]) << ", " << clampIdx(tri[1]) << ", " << clampIdx(tri[2])
              << ", 0);\n";
        }
    }

    f.close();
    Logger::Get().Log(LogLevel::INFO, "[MefExporter] Successfully exported ASCII MEF to: " + outpath);
    return true;
}

} // namespace MefExporter
