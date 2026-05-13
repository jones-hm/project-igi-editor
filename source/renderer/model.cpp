#include "model.h"
#include "model_loader.h"
#include "wic_loader.h"
#include "../pch.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <stdexcept>
#include <iostream>

Mesh loadObjModel(const std::string& filepath, const std::string& texturePath) {
    // For FBX migration, we'll ignore the separate texture path since textures are embedded
    std::string fbxPath = filepath;
    
    // Replace .obj extension with .fbx if needed
    if (fbxPath.substr(fbxPath.length() - 4) == ".obj") {
        fbxPath = fbxPath.substr(0, fbxPath.length() - 4) + ".fbx";
    }
    
    ModelData modelData = ModelLoader::Load(fbxPath);
    
    if (!modelData.loaded) {
        throw std::runtime_error("Failed to load FBX model: " + fbxPath);
    }
    
    // For now, we'll just use the first mesh. In the future, we might want to handle multiple meshes
    if (modelData.meshes.empty()) {
        throw std::runtime_error("No meshes found in FBX model: " + fbxPath);
    }
    
    const MeshData& meshData = modelData.meshes[0];
    
    Mesh mesh;
    mesh.textureID = meshData.textureID;
    mesh.vertexCount = static_cast<int>(meshData.vertices.size());
    
    // Convert Vertex struct to flat float array (position + normal + UV)
    std::vector<float> vertices;
    vertices.reserve(meshData.vertices.size() * 8); // 8 floats per vertex
    
    // Compute bounds for halfExtents and zOffset
    glm::vec3 minPos(1e10f), maxPos(-1e10f);
    
    for (const auto& vertex : meshData.vertices) {
        // Position
        vertices.push_back(vertex.x);
        vertices.push_back(vertex.y);
        vertices.push_back(vertex.z);
        
        // Update bounds
        minPos.x = std::min(minPos.x, vertex.x);
        minPos.y = std::min(minPos.y, vertex.y);
        minPos.z = std::min(minPos.z, vertex.z);
        maxPos.x = std::max(maxPos.x, vertex.x);
        maxPos.y = std::max(maxPos.y, vertex.y);
        maxPos.z = std::max(maxPos.z, vertex.z);
        
        // Normal
        vertices.push_back(vertex.nx);
        vertices.push_back(vertex.ny);
        vertices.push_back(vertex.nz);
        
        // UV
        vertices.push_back(vertex.u);
        vertices.push_back(vertex.v);
    }
    
    mesh.halfExtents = (maxPos - minPos) * 0.5f;
    mesh.zOffset = -minPos.y; // Y-up models rotated 90deg, so Y becomes Z in world space
    
    // Store vertex data in the mesh for client-side rendering
    mesh.vertexData = new float[vertices.size()];
    memcpy(mesh.vertexData, vertices.data(), vertices.size() * sizeof(float));
    
    // Create OpenGL buffers
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    
    glBindVertexArray(mesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // attrib 0 = position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // attrib 1 = normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // attrib 2 = uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
    
    std::cout << "[FBX] Loaded: " << fbxPath << " | Vertices: " << mesh.vertexCount << "\n";
    return mesh;
}

void renderModel(const Mesh& mesh) {
    if (mesh.VAO == 0) return;

    // Draw
    glBindVertexArray(mesh.VAO);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
    glBindVertexArray(0);
}

void destroyModel(Mesh& mesh) {
    glDeleteBuffers(1,      &mesh.VBO);
    glDeleteVertexArrays(1, &mesh.VAO);

    // Free client-side vertex data
    if (mesh.vertexData) {
        delete[] mesh.vertexData;
        mesh.vertexData = nullptr;
    }

    mesh.VAO = mesh.VBO = 0;
    mesh.vertexCount = 0;
}