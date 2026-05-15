#pragma once
#include <string>
#include <vector>

struct KeyBinding {
    int vkCode; // Windows virtual key code
    bool ctrl;
    bool shift;
    bool alt;
};

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

    // Movement Keys
    char keyMoveForward;
    char keyMoveBackward;
    char keyMoveLeft;
    char keyMoveRight;

    // [UI] - Font and UI settings
    float fontSize;
    int fontColorR;
    int fontColorG;
    int fontColorB;

    // [Keybindings] - Configurable shortcuts
    KeyBinding keySave;
    KeyBinding keyResetLevel;
    KeyBinding keyDebug;
    KeyBinding keyQuit;
    KeyBinding keyHelp;
    KeyBinding keyResetScript;
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
