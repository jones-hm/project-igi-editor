#pragma once
#include "../pch.h"
#include "model.h"
#include "../level/level_objects.h"
#include <map>
#include <string>

class Renderer_Objects {
public:
    Renderer_Objects();
    ~Renderer_Objects();

    bool Init();
    void Shutdown();

    void Draw(GLuint ubo_mats, bool overlay_wireframe, const std::vector<LevelObject>& objects);

private:
    std::map<std::string, Mesh> mesh_cache_;
    
    Mesh GetOrLoadMesh(const std::string& modelId);
    std::string FindModelFile(const std::string& modelId);
};
