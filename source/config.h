#pragma once
#include <string>

struct ConfigData {
    // [GamePath]
    std::string igiPath;
    int level;
    std::string editorPath;     // Path to this editor
    std::string qEditorPath;    // Path to %APPDATA%\QEditor
    
    // [Paths]
    std::string aiPath;
    std::string compilerPath;
    std::string filesPath;
    std::string graphsPath;

    
    // [Marker] - Movement
    char moveForward;
    char moveBackward;
    char moveLeft;
    char moveRight;
    char moveUp;
    char moveDown;
    
    // [Marker] - Rotation
    char rotateYawCCW;
    char rotateYawCW;
    char rotatePitchUp;
    char rotatePitchDown;
    char rotateRollLeft;
    char rotateRollRight;
    
    // [Marker] - Shortcuts
    char teleportToMarker;
    char resetMarkerToPlayer;
};

class Config {
public:
    static void Init();
    static ConfigData& Get();
    static void Save();
    
private:
    static void Load();
    static void CreateDefault();
    static std::string GetConfigPath();
    
    static ConfigData data_;
};
