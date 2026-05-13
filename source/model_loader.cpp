#include "model_loader.h"
#include <GL/glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "logger.h"
#include "renderer/wic_loader.h"

ModelData ModelLoader::Load(const std::string& fbxPath) {
    ModelData model;

#ifdef ASSIMP_AVAILABLE
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(fbxPath,
        aiProcess_Triangulate    |
        aiProcess_FlipUVs        |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::Get().Log(LogLevel::ERROR, "Assimp load failed: " + std::string(importer.GetErrorString()));
        return model;
    }

    std::string dir = fbxPath.substr(0, fbxPath.find_last_of("/\\"));
    ProcessNode(scene->mRootNode, scene, model, dir);
    model.loaded = true;
    Logger::Get().Log(LogLevel::INFO, "FBX loaded: " + fbxPath + " (" + std::to_string(model.meshes.size()) + " meshes)");
#else
    Logger::Get().Log(LogLevel::WARNING, "Assimp not available - cannot load FBX: " + fbxPath);
    // Fallback to OBJ loading could be implemented here
#endif

    return model;
}

#ifdef ASSIMP_AVAILABLE
void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene, ModelData& model, const std::string& dir) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model.meshes.push_back(ProcessMesh(mesh, scene, dir));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++)
        ProcessNode(node->mChildren[i], scene, model, dir);
}

MeshData ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& dir) {
    MeshData data;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex v;
        v.x  = mesh->mVertices[i].x;
        v.y  = mesh->mVertices[i].y;
        v.z  = mesh->mVertices[i].z;
        v.nx = mesh->mNormals ? mesh->mNormals[i].x : 0.f;
        v.ny = mesh->mNormals ? mesh->mNormals[i].y : 0.f;
        v.nz = mesh->mNormals ? mesh->mNormals[i].z : 1.f;
        if (mesh->mTextureCoords[0]) {
            v.u = mesh->mTextureCoords[0][i].x;
            v.v = mesh->mTextureCoords[0][i].y;
        }
        data.vertices.push_back(v);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        for (unsigned int j = 0; j < mesh->mFaces[i].mNumIndices; j++)
            data.indices.push_back(mesh->mFaces[i].mIndices[j]);

    // Load embedded or external texture
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.C_Str());
            if (embedded)
                data.textureID = LoadEmbeddedTexture(embedded);
            else
                data.textureID = LoadTextureFromFile(dir + "/" + texPath.C_Str());
        }
    }
    return data;
}

unsigned int ModelLoader::LoadEmbeddedTexture(const aiTexture* tex) {
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    int w, h, ch;
    unsigned char* data = nullptr;
    if (tex->mHeight == 0) // compressed format (PNG/JPG)
        data = stbi_load_from_memory(
            reinterpret_cast<unsigned char*>(tex->pcData),
            tex->mWidth, &w, &h, &ch, 0);
    else {
        w = tex->mWidth; h = tex->mHeight; ch = 4;
        data = reinterpret_cast<unsigned char*>(tex->pcData);
    }
    GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (tex->mHeight == 0) stbi_image_free(data);
    return id;
}

unsigned int ModelLoader::LoadEmbeddedTexture(const aiTexture* tex) {
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    int w, h, ch;
    unsigned char* data = nullptr;
    if (tex->mHeight == 0) // compressed format (PNG/JPG)
        data = stbi_load_from_memory(
            reinterpret_cast<unsigned char*>(tex->pcData),
            tex->mWidth, &w, &h, &ch, 0);
    else {
        w = tex->mWidth; h = tex->mHeight; ch = 4;
        data = reinterpret_cast<unsigned char*>(tex->pcData);
    }
    GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (tex->mHeight == 0) stbi_image_free(data);
    return id;
}

unsigned int ModelLoader::LoadTextureFromFile(const std::string& path) {
    // Use existing WIC loader for consistency with the rest of the codebase
    pic_s pic = {0};
    if (WIC_LoadImage(path.c_str(), pic)) {
        unsigned int id = GL_RegisterTexture(&pic, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, true);
        MEM_FREE_(pic.pixels_);
        Logger::Get().Log(LogLevel::INFO, "Loaded external texture: " + path + " (ID: " + std::to_string(id) + ")");
        return id;
    } else {
        Logger::Get().Log(LogLevel::WARNING, "Texture not found: " + path);
        return 0;
    }
}

#endif // ASSIMP_AVAILABLE