#pragma once
#include <string>
#include "../pch.h"

struct Mesh {
    unsigned int VAO, VBO;
    unsigned int textureID; // Added texture support
    int vertexCount;
    float* vertexData; // For client-side array rendering
    glm::vec3 halfExtents; // Half-size of the bounding box
    float zOffset;         // Offset to the bottom of the mesh
};

Mesh  loadObjModel(const std::string& filepath, const std::string& texturePath = "");
void  renderModel(const Mesh& mesh);
void  destroyModel(Mesh& mesh);
