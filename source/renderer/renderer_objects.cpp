#include "pch.h"
#include "renderer_objects.h"
#include "logger.h"
#include "common.h"
#include "model_loader.h"
#include <filesystem>

// ─── GetOrLoadMesh ────────────────────────────────────────────────────────────
Mesh Renderer_Objects::GetOrLoadMesh(const std::string& modelId, bool isBuilding) {
    std::string cacheKey = (isBuilding ? "building:" : "object:") + modelId;

    // Return cached mesh if already loaded
    auto it = mesh_cache_.find(cacheKey);
    if (it != mesh_cache_.end())
        return it->second;

    // Find the file on disk
    std::string filepath = FindModelFile(modelId, isBuilding);
    if (filepath.empty()) {
        Logger::Get().Log(LogLevel::WARNING, "[Renderer_Objects] Model search FAILED for ID: " + modelId + ". Skipping render.");
        Mesh emptyMesh;
        emptyMesh.vertexCount = 0;
        mesh_cache_[cacheKey] = emptyMesh;
        return mesh_cache_[cacheKey];
    }

    // Load and cache
    try {
        // FBX files have embedded textures, so we don't need separate texture path logic
        Mesh mesh = loadObjModel(filepath, ""); // Empty texture path for embedded textures
        mesh_cache_[cacheKey] = mesh;
        Logger::Get().Log(LogLevel::INFO, "[Renderer_Objects] Success: Loaded model '" + modelId + "' from " + filepath + " (" + std::to_string(mesh.vertexCount) + " vertices)");
        return mesh;

    } catch (const std::exception& e) {
        Logger::Get().Log(LogLevel::ERR, "[Renderer_Objects] Load FAILED for " + modelId + ": " + std::string(e.what()));
        Mesh emptyMesh;
        mesh_cache_[cacheKey] = emptyMesh;
        return mesh_cache_[cacheKey];
    }

}