#pragma once
#include <string>
#include "../pch.h"

struct Mesh {
    GLuint VAO, VBO, IBO;
    int vertexCount, indexCount;
    GLuint textureID;
    glm::vec3 halfExtents;
    float zOffset;
    float* vertexData;

    Mesh() : VAO(0), VBO(0), IBO(0), vertexCount(0), indexCount(0), textureID(0), zOffset(0.0f), vertexData(nullptr) {}
};

Mesh  loadObjModel(const std::string& filepath, const std::string& texturePath = "");
void  renderModel(const Mesh& mesh);
void  destroyModel(Mesh& mesh);
