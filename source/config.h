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

    
    // [Marker] - IGI 2 Style Manipulation
    char keySnapGround;
    char keySnapObject;
    char keyRotateAlpha;
    char keyRotateBeta;
    char keyRotateGamma;
    char keyResetOri;
    
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
