#include "pch.h"
#include "config.h"
#include "logger.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>

ConfigData Config::data_;

void Config::Init() {
    std::string path = GetConfigPath();
    Logger::Get().Log(LogLevel::INFO, "[Config] Config file path: " + path);
    if (!std::filesystem::exists(path)) {
        Logger::Get().Log(LogLevel::INFO, "[Config] Config file not found, creating default");
        CreateDefault();
        Save();
        Logger::Get().Log(LogLevel::INFO, "[Config] Default config created with IGIPath: " + data_.igiPath);
    } else {
        Logger::Get().Log(LogLevel::INFO, "[Config] Config file found, loading...");
        Load();
        Logger::Get().Log(LogLevel::INFO, "[Config] Config loaded - IGIPath: " + data_.igiPath);
    }
}

ConfigData& Config::Get() {
    return data_;
}

std::string Config::GetConfigPath() {
    // Get the directory where the executable is located
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    return exeDir + "\\config.ini";
}

void Config::CreateDefault() {
    data_.igiPath = "D:\\IGI1";
    data_.level = 1;
    data_.editorPath = ".";
    
    char appData[1024];
    GetEnvironmentVariableA("APPDATA", appData, 1024);
    data_.qEditorPath = std::string(appData) + "\\QEditor";

    data_.aiPath = data_.qEditorPath + "\\AIFiles";
    data_.compilerPath = data_.qEditorPath + "\\QCompiler";
    data_.filesPath = data_.qEditorPath + "\\QFiles";
    data_.graphsPath = data_.qEditorPath + "\\QGraphs";
    data_.texturesPath = data_.qEditorPath + "\\3DEditor\\textures";


    
    data_.keySnapGround = 'S';
    data_.keySnapObject = 'O';
    data_.keyRotateAlpha = 'A';
    data_.keyRotateBeta = 'B';
    data_.keyRotateGamma = 'G';
    data_.keyResetOri = ' '; // Space
    
    data_.teleportToMarker = 0; // F11 is handled specially
    data_.resetMarkerToPlayer = 'H';

    // Movement keys (arrow keys) - using GLUT special key codes
    data_.keyMoveForward = 101; // GLUT_KEY_UP
    data_.keyMoveBackward = 103; // GLUT_KEY_DOWN
    data_.keyMoveLeft = 100; // GLUT_KEY_LEFT
    data_.keyMoveRight = 102; // GLUT_KEY_RIGHT

    // Font and UI settings
    data_.fontSize = 12.0f;
    data_.fontColorR = 255;
    data_.fontColorG = 255;
    data_.fontColorB = 255;

    // Default keybindings
    data_.keySave = {0x53, true, false, false};    // CTRL+S (0x53 = 'S')
    data_.keyResetLevel = {0x52, true, false, false}; // CTRL+R (0x52 = 'R')
    data_.keyDebug = {0x44, true, false, false};   // CTRL+D (0x44 = 'D')
    data_.keyQuit = {0x51, true, false, false};    // CTRL+Q (0x51 = 'Q')
    data_.keyHelp = {0x48, false, false, false};   // H (0x48 = 'H')
    data_.keyResetScript = {0x52, false, true, false}; // SHIFT+R (0x52 = 'R')
}

static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static KeyBinding ParseKeyBinding(const std::string& binding) {
    KeyBinding kb = {0, false, false, false};

    std::string upper = binding;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Parse modifiers
    if (upper.find("CTRL") != std::string::npos) kb.ctrl = true;
    if (upper.find("SHIFT") != std::string::npos) kb.shift = true;
    if (upper.find("ALT") != std::string::npos) kb.alt = true;

    // Extract the key part (after + or the only part)
    size_t lastPlus = upper.rfind('+');
    std::string keyPart = (lastPlus != std::string::npos) ? upper.substr(lastPlus + 1) : upper;

    // Map to Windows virtual key codes
    if (keyPart == "F1") kb.vkCode = VK_F1;
    else if (keyPart == "F2") kb.vkCode = VK_F2;
    else if (keyPart == "F3") kb.vkCode = VK_F3;
    else if (keyPart == "F4") kb.vkCode = VK_F4;
    else if (keyPart == "F5") kb.vkCode = VK_F5;
    else if (keyPart == "F6") kb.vkCode = VK_F6;
    else if (keyPart == "F7") kb.vkCode = VK_F7;
    else if (keyPart == "F8") kb.vkCode = VK_F8;
    else if (keyPart == "F9") kb.vkCode = VK_F9;
    else if (keyPart == "F10") kb.vkCode = VK_F10;
    else if (keyPart == "F11") kb.vkCode = VK_F11;
    else if (keyPart == "F12") kb.vkCode = VK_F12;
    else if (keyPart.length() == 1) {
        // Single character key
        kb.vkCode = VkKeyScanA(keyPart[0]) & 0xFF;
    }

    return kb;
}

void Config::Load() {
    std::ifstream file(GetConfigPath());
    if (!file.is_open()) return;

    std::string line, section;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';') continue;

        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = Trim(line.substr(0, eqPos));
            std::string val = Trim(line.substr(eqPos + 1));

            if (section == "GamePath") {
                if (key == "IGIPath") data_.igiPath = val;
                else if (key == "Level") data_.level = std::stoi(val);
                else if (key == "EditorPath") data_.editorPath = val;
                else if (key == "QEditorPath") data_.qEditorPath = val;
            } else if (section == "Paths") {
                if (key == "AIFiles") data_.aiPath = val;
                else if (key == "QCompiler") data_.compilerPath = val;
                else if (key == "QFiles") data_.filesPath = val;
                else if (key == "QGraphs") data_.graphsPath = val;
                else if (key == "Textures") data_.texturesPath = val;
            } else if (section == "Marker") {


                char c = (val.empty() ? 0 : val[0]);
                if (key == "ManipulatePositionSnapToGround") data_.keySnapGround = c;
                else if (key == "ManipulatePositionSnapToObject") data_.keySnapObject = c;
                else if (key == "MoveForward") {
                    if (val == "Up") data_.keyMoveForward = 101; // GLUT_KEY_UP
                    else data_.keyMoveForward = c;
                }
                else if (key == "MoveBackward") {
                    if (val == "Down") data_.keyMoveBackward = 103; // GLUT_KEY_DOWN
                    else data_.keyMoveBackward = c;
                }
                else if (key == "MoveLeft") {
                    if (val == "Left") data_.keyMoveLeft = 100; // GLUT_KEY_LEFT
                    else data_.keyMoveLeft = c;
                }
                else if (key == "MoveRight") {
                    if (val == "Right") data_.keyMoveRight = 102; // GLUT_KEY_RIGHT
                    else data_.keyMoveRight = c;
                }
                else if (key == "ManipulateOrientationAlpha") data_.keyRotateAlpha = c;
                else if (key == "ManipulateOrientationBeta") data_.keyRotateBeta = c;
                else if (key == "ManipulateOrientationGamma") data_.keyRotateGamma = c;
                else if (key == "ManipulateOrientationReset") data_.keyResetOri = ' ';
                else if (key == "TeleportToMarker") data_.teleportToMarker = c;
                else if (key == "ResetMarkerToPlayer") data_.resetMarkerToPlayer = c;
            } else if (section == "UI") {
                if (key == "FontSize") data_.fontSize = std::stof(val);
                else if (key == "FontColorR") data_.fontColorR = std::stoi(val);
                else if (key == "FontColorG") data_.fontColorG = std::stoi(val);
                else if (key == "FontColorB") data_.fontColorB = std::stoi(val);
            } else if (section == "Keybindings") {
                if (key == "Save") data_.keySave = ParseKeyBinding(val);
                else if (key == "ResetLevel") data_.keyResetLevel = ParseKeyBinding(val);
                else if (key == "Debug") data_.keyDebug = ParseKeyBinding(val);
                else if (key == "Quit") data_.keyQuit = ParseKeyBinding(val);
                else if (key == "Help") data_.keyHelp = ParseKeyBinding(val);
                else if (key == "ResetScript") data_.keyResetScript = ParseKeyBinding(val);
            }
        }
    }
}

void Config::Save() {
    std::ofstream file(GetConfigPath());
    if (!file.is_open()) return;

    file << "; ================================================" << std::endl;
    file << "; IGI EDITOR CONFIGURATION" << std::endl;
    file << "; ================================================" << std::endl;
    file << std::endl;

    file << "[GamePath]" << std::endl;
    file << "IGIPath=" << data_.igiPath << std::endl;
    file << "Level=" << data_.level << std::endl;
    file << "EditorPath=" << data_.editorPath << std::endl;
    file << "QEditorPath=" << data_.qEditorPath << std::endl;
    file << std::endl;

    file << "[Paths]" << std::endl;
    file << "AIFiles=" << data_.aiPath << std::endl;
    file << std::endl;
    file << "; Movement Keys (Arrow keys for camera movement)" << std::endl;
    file << "MoveForward=Up" << std::endl;
    file << "MoveBackward=Down" << std::endl;
    file << "MoveLeft=Left" << std::endl;
    file << "MoveRight=Right" << std::endl;
    file << "QCompiler=" << data_.compilerPath << std::endl;
    file << "Textures=" << data_.texturesPath << std::endl;
    file << "QFiles=" << data_.filesPath << std::endl;
    file << "QGraphs=" << data_.graphsPath << std::endl;
    file << std::endl;

    file << "[Marker]" << std::endl;
    file << "; IGI 2 Editor-style object manipulation bindings" << std::endl;
    file << "Manipulate=LeftMouseButton" << std::endl;
    file << "ManipulatePositionXY=Shift" << std::endl;
    file << "ManipulatePositionXZ=Ctrl" << std::endl;
    file << "ManipulatePositionXZAlt=Shift+Ctrl" << std::endl;
    file << "ManipulatePositionSnapToGround=" << data_.keySnapGround << std::endl;
    file << "ManipulatePositionSnapToObject=" << data_.keySnapObject << std::endl;
    file << "ManipulateOrientationAlpha=" << data_.keyRotateAlpha << std::endl;
    file << "ManipulateOrientationBeta=" << data_.keyRotateBeta << std::endl;
    file << "ManipulateOrientationGamma=" << data_.keyRotateGamma << std::endl;
    file << "ManipulateOrientationReset=Space" << std::endl;
    file << std::endl;

    file << "TeleportToMarker=F11" << std::endl;
    file << "ResetMarkerToPlayer=" << data_.resetMarkerToPlayer << std::endl;
    file << std::endl;

    file << "[UI]" << std::endl;
    file << "; Font and UI settings" << std::endl;
    file << "FontSize=" << data_.fontSize << std::endl;
    file << "FontColorR=" << data_.fontColorR << std::endl;
    file << "FontColorG=" << data_.fontColorG << std::endl;
    file << "FontColorB=" << data_.fontColorB << std::endl;
}

