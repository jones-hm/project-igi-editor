#pragma once
#include "../pch.h"
#include "level_common.h"
#include <vector>
#include <string>
#include <map>

struct LevelObject {
    std::string name;
    std::string modelId;
    glm::vec3 pos;
    glm::vec3 rot;
    bool isBuilding;
};

struct qtask_object_s : qtask_s {
    char model_id_[32];
    double script_defined_pos_[3];
    float rot_z_;
};

class LevelObjects {
public:
    static constexpr int32_t TASK_TYPE_BUILDING = 200;
    static constexpr int32_t TASK_TYPE_EDIT_RIGID_OBJ = 201;

    void Load(ILevelDynCube* level_dyn_cube, const QSC* qsc_objects);
    void Unload();
    void LoadModelNames();
    std::string GetModelName(const std::string& modelId);


    const std::vector<LevelObject>& GetObjects() const { return objects_; }
    std::vector<LevelObject>& GetObjects() { return objects_; }


private:
    std::vector<LevelObject> objects_;
    std::vector<qtask_object_s> qtasks_;
    std::map<std::string, std::string> modelNames_;
};
