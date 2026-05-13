#pragma once

// Try to include Assimp headers, but provide fallback if not available
#ifdef __has_include
    #if __has_include(<assimp/Importer.hpp>)
        #include <assimp/Importer.hpp>
        #include <assimp/scene.h>
        #include <assimp/postprocess.h>
        #define ASSIMP_AVAILABLE 1
    #endif
#endif

#include <string>
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    float x, y, z;     // position
    float nx, ny, nz;  // normal
    float u, v;        // UV coords
};

struct MeshData {
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    unsigned int              textureID = 0; // OpenGL texture handle
};

struct ModelData {
    std::vector<MeshData> meshes;
    bool                  loaded = false;
};

class ModelLoader {
public:
    static ModelData Load(const std::string& fbxPath);
private:
#ifdef ASSIMP_AVAILABLE
    static void      ProcessNode(aiNode* node, const aiScene* scene, ModelData& model, const std::string& dir);
    static MeshData  ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& dir);
    static unsigned int LoadEmbeddedTexture(const aiTexture* tex);
#endif
    static unsigned int LoadTextureFromFile(const std::string& path);
};