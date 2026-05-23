#include "pch.h"
#include "config.h"
#include "logger.h"
#include "utils.h"
#include "parsers/qsc_lexer.h"
#include "parsers/qsc_parser.h"
#include "parsers/qvm_compiler.h"
#include "parsers/qvm_parser.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>

ConfigData Config::data_;

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

    if (upper.find("CTRL") != std::string::npos) kb.ctrl = true;
    if (upper.find("SHIFT") != std::string::npos) kb.shift = true;
    if (upper.find("ALT") != std::string::npos) kb.alt = true;

    size_t lastPlus = upper.rfind('+');
    std::string keyPart = (lastPlus != std::string::npos) ? upper.substr(lastPlus + 1) : upper;

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
    else if (keyPart == "INSERT") kb.vkCode = VK_INSERT;
    else if (keyPart == "DELETE") kb.vkCode = VK_DELETE;
    else if (keyPart == "HOME") kb.vkCode = VK_HOME;
    else if (keyPart == "SPACE") kb.vkCode = VK_SPACE;
    else if (keyPart.length() == 1) {
        kb.vkCode = VkKeyScanA(keyPart[0]) & 0xFF;
    }

    return kb;
}

std::string Config::GetConfigPath() {
    return Utils::GetExeDirectory() + "\\content\\qed\\qedconfig.txt";
}

std::string Config::GetKeybindingsPath() {
    return Utils::GetExeDirectory() + "\\content\\qed\\qedkeybindings.txt";
}

void Config::CreateDefault() {
    // igiPath is same as editor current path, set that to it as default
    data_.igiPath = Utils::GetExeDirectory();
    data_.level = 1;
    data_.editorPath = Utils::GetExeDirectory();

    std::string qeditorPath = Utils::GetExeDirectory() + "\\content\\qed";
    data_.qEditorPath = qeditorPath;
    data_.aiPath = qeditorPath + "\\AIFiles";
    data_.filesPath = qeditorPath + "\\QFiles";
    data_.graphsPath = qeditorPath + "\\QGraphs";

    data_.keySnapGround = 'S';
    data_.keySnapObject = 'O';
    data_.keyRotateAlpha = 'A';
    data_.keyRotateBeta = 'B';
    data_.keyRotateGamma = 'G';
    data_.keyResetOri = ' '; // Space
    data_.keyResetPos = 'P';

    data_.teleportToMarker = 0; // F11 is handled specially
    data_.resetMarkerToPlayer = 'H';

    data_.keyMoveForward = 101; // GLUT_KEY_UP
    data_.keyMoveBackward = 103; // GLUT_KEY_DOWN
    data_.keyMoveLeft = 100; // GLUT_KEY_LEFT
    data_.keyMoveRight = 102; // GLUT_KEY_RIGHT

    data_.fontSize = 12.0f;
    data_.fontColorR = 255;
    data_.fontColorG = 255;
    data_.fontColorB = 255;

    data_.keySave = {0x53, true, false, false};    
    data_.keyResetLevel = {0x52, true, false, false}; 
    data_.keyDebug = {0x44, true, false, false};   
    data_.keyQuit = {0x51, true, false, false};    
    data_.keyHelp = {0x48, false, false, false};   
    data_.keyResetScript = {0x52, false, true, false}; 
    data_.keyClipMode = {VK_F3, false, false, false}; 

    data_.enableLogging = true;
    data_.debugLogging = false;
}

void Config::Init() {
    CreateDefault();

    std::string qedDir = Utils::GetExeDirectory() + "\\content\\qed";
    std::filesystem::create_directories(qedDir);

    std::string txtPath = GetConfigPath();
    std::string kbPath = GetKeybindingsPath();

    if (!std::filesystem::exists(txtPath) || !std::filesystem::exists(kbPath)) {
        Logger::Get().Log(LogLevel::INFO, "[Config] Config files not found, creating defaults");
        Save();
    } else {
        Logger::Get().Log(LogLevel::INFO, "[Config] Config files found, loading...");
        Load();
    }

    std::string qscPath = qedDir + "\\qedconfig.qsc";
    std::string qvmPath = qedDir + "\\qedconfig.qvm";
    
    std::ifstream txtIn(txtPath);
    std::ofstream qscOut(qscPath);
    if (txtIn && qscOut) {
        std::string line;
        while (std::getline(txtIn, line)) {
            size_t commentPos1 = line.find("//");
            size_t commentPos2 = line.find(";");
            size_t pos = std::min(commentPos1, commentPos2);
            if (pos != std::string::npos) {
                line = line.substr(0, pos);
            }
            
            std::string clean = Trim(line);
            if (clean.empty() || clean.front() == '[') continue;
            
            auto eqPos = clean.find('=');
            if (eqPos != std::string::npos) {
                std::string key = Trim(clean.substr(0, eqPos));
                std::string val = Trim(clean.substr(eqPos + 1));
                
                bool isNum = !val.empty() && val.find_first_not_of("0123456789.-") == std::string::npos;
                if (isNum || val == "true" || val == "false" || val == "TRUE" || val == "FALSE") {
                    qscOut << key << "(" << val << ");\n";
                } else {
                    std::string escVal = val;
                    size_t p = 0;
                    while((p = escVal.find('"', p)) != std::string::npos) {
                        escVal.replace(p, 1, "\\\"");
                        p += 2;
                    }
                    qscOut << key << "(\"" << escVal << "\");\n";
                }
            }
        }
    }
    txtIn.close();
    qscOut.close();

    std::string compileErr;
    std::ifstream qscIn(qscPath);
    std::string qscSrc((std::istreambuf_iterator<char>(qscIn)), std::istreambuf_iterator<char>());
    auto lexResult  = qsc::Lex(qscSrc);
    auto parseResult = lexResult.ok ? qsc::Parse(lexResult.tokens) : qsc::ParseResult{};
    if (lexResult.ok && parseResult.ok) {
        if (qvm::CompileToFile(*parseResult.program, qvmPath, &compileErr)) {
            Logger::Get().Log(LogLevel::INFO, "[Config] Successfully compiled qedconfig.qsc to qedconfig.qvm");
        } else {
            Logger::Get().Log(LogLevel::ERR, "[Config] Failed to compile QVM: " + compileErr);
        }
    }

    QVMFile qvm = QVM_Parse(qvmPath);
    if (qvm.valid) {
        Logger::Get().Log(LogLevel::INFO, "[Config] Read config from .qvm (" + std::to_string(qvm.identifierCount()) + " keys)");
    }

    data_.igiPath = Utils::GetExeDirectory();
}

ConfigData& Config::Get() {
    return data_;
}

void Config::Load() {
    std::string qedDir = Utils::GetExeDirectory() + "\\content\\qed";
    std::string qvmPath = qedDir + "\\qedconfig.qvm";
    std::string qvmKbPath = qedDir + "\\qedkeybindings.qvm";

    auto loadFromTxt = [&]() {
        auto parseFile = [&](const std::string& path) {
            std::ifstream file(path);
            if (!file.is_open()) return;
            std::string line, section;
            while (std::getline(file, line)) {
                size_t cPos1 = line.find("//");
                size_t cPos2 = line.find(";");
                if (cPos1 != std::string::npos || cPos2 != std::string::npos) {
                    line = line.substr(0, std::min(cPos1, cPos2));
                }
                line = Trim(line);
                if (line.empty()) continue;
                if (line[0] == '[' && line.back() == ']') {
                    section = line.substr(1, line.size() - 2);
                    continue;
                }
                
                // Try parsing QED SetEventBinding
                if (line.find("SetEventBinding(") == 0) {
                    size_t comma = line.find(',');
                    size_t endP = line.find(')');
                    if (comma != std::string::npos && endP != std::string::npos) {
                        std::string key = line.substr(16, comma - 16);
                        std::string val = line.substr(comma + 1, endP - comma - 1);
                        
                        // Clean quotes
                        key.erase(std::remove(key.begin(), key.end(), '"'), key.end());
                        val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
                        key = Trim(key);
                        val = Trim(val);
                        
                        // Replace < and > with + to match ParseKeyBinding expectations
                        std::string parseVal = val;
                        std::replace(parseVal.begin(), parseVal.end(), '<', ' ');
                        std::replace(parseVal.begin(), parseVal.end(), '>', '+');
                        // Remove trailing plus
                        if (!parseVal.empty() && parseVal.back() == '+') parseVal.pop_back();
                        // Clean spaces
                        parseVal.erase(std::remove(parseVal.begin(), parseVal.end(), ' '), parseVal.end());
                        
                        if (key == "SaveObjectFile") data_.keySave = ParseKeyBinding(parseVal);
                        else if (key == "ReloadSettings") data_.keyReloadSettings = ParseKeyBinding(parseVal);
                        else if (key == "Undo") data_.keyUndo = ParseKeyBinding(parseVal);
                        else if (key == "Redo") data_.keyRedo = ParseKeyBinding(parseVal);
                                                                        else if (key == "CameraEnable") data_.keyEnableCamera = ParseKeyBinding(parseVal);
                        else if (key == "CameraMoveForward") { data_.keyMoveCameraForward = ParseKeyBinding(parseVal); }
                        else if (key == "CameraMoveBackward") { data_.keyMoveCameraBackward = ParseKeyBinding(parseVal); }
                        else if (key == "CameraStrafeLeft") { }
                        else if (key == "CameraStrafeRight") { }
                        else if (key == "CameraAdjustRadius") data_.keyAdjustCameraRadius = ParseKeyBinding(parseVal);
                        else if (key == "CameraLookDown") data_.keyLookDown = ParseKeyBinding(parseVal);
                        else if (key == "CameraSnapToObject") data_.keySnapToObject = ParseKeyBinding(parseVal);
                        else if (key == "CameraSnapToGround") data_.keySnapToGround = ParseKeyBinding(parseVal);
                        else if (key == "ToggleDisplay") data_.keyClipMode = ParseKeyBinding(parseVal);
                        else if (key == "TaskNew") data_.keyCreateNewTask = ParseKeyBinding(parseVal);
                        else if (key == "TaskCopy") data_.keyCopyTask = ParseKeyBinding(parseVal);
                        else if (key == "TaskPaste") data_.keyPasteTask = ParseKeyBinding(parseVal);
                        else if (key == "TaskSetID") data_.keyAssignTaskID = ParseKeyBinding(parseVal);
                        else if (key == "AnimTaskStartRecording") data_.keyStartRecording = ParseKeyBinding(parseVal);
                        else if (key == "AnimTaskGoToCursor") data_.keyGoToCursor = ParseKeyBinding(parseVal);
                        else if (key == "AnimTaskToggleSyncPlayback") data_.keySyncPlayback = ParseKeyBinding(parseVal);
                        else if (key == "Manipulate") {} // Not supported mapped this way yet
                        else if (key == "ManipulatePositionXY") {}
                        else if (key == "ManipulatePositionXZ") {}
                        else if (key == "ManipulatePositionSnapToGround") data_.keySnapGround = 'S'; // Fallback
                        else if (key == "ManipulatePositionSnapToObject") data_.keySnapObject = 'O'; // Fallback
                        else if (key == "ManipulateOrientationAlpha") data_.keyRotateAlpha = 'A'; // Fallback
                        else if (key == "ManipulateOrientationBeta") data_.keyRotateBeta = 'B'; // Fallback
                        else if (key == "ManipulateOrientationGamma") data_.keyRotateGamma = 'G'; // Fallback
                        else if (key == "ManipulateOrientationReset") data_.keyResetOri = ' '; // Fallback
                        else if (key == "ToggleConsole") data_.keyDebug = ParseKeyBinding(parseVal);
                    }
                    continue;
                }

                auto eqPos = line.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = Trim(line.substr(0, eqPos));
                    std::string val = Trim(line.substr(eqPos + 1));
                    
                    if (val.back() == ';') val.pop_back();

                    if (key == "IGIPath") data_.igiPath = val;
                    else if (key == "Level") data_.level = std::stoi(val);
                    else if (key == "EditorPath") data_.editorPath = val;
                    else if (key == "QEditorPath") data_.qEditorPath = val;
                    else if (key == "AIFiles") data_.aiPath = val;
                    else if (key == "QFiles") data_.filesPath = val;
                    else if (key == "QGraphs") data_.graphsPath = val;
                    else if (key == "FontSize") data_.fontSize = std::stof(val);
                    else if (key == "FontColorR") data_.fontColorR = std::stoi(val);
                    else if (key == "FontColorG") data_.fontColorG = std::stoi(val);
                    else if (key == "FontColorB") data_.fontColorB = std::stoi(val);
                    else if (key == "QEDSaveConfigOnExit" || key == "Enable") data_.enableLogging = (val == "TRUE" || val == "true" || val == "1");
                    else if (key == "Debug") data_.debugLogging = (val == "TRUE" || val == "true" || val == "1");
                }
            }
        };

        parseFile(GetConfigPath());
        parseFile(GetKeybindingsPath());
    };

    // If QVMs exist, we should theoretically parse them.
    // For now, since QVM_Parse only gives us opcodes and we need key value pairs,
    // we'll rely on the text fallback for config and keybindings mapping, but read it
    // understanding the QED syntax!
    loadFromTxt();
}

void Config::Save() {
    std::ofstream file(GetConfigPath());
    if (file.is_open()) {
        file << "// ================================================\n";
        file << "// IGI EDITOR CONFIGURATION\n";
        file << "// ================================================\n\n";
        file << "[GamePath]\n";
        file << "Level=" << data_.level << "\n";
        file << "EditorPath=" << data_.editorPath << "\n";
        file << "QEditorPath=" << data_.qEditorPath << "\n\n";
        file << "[Paths]\n";
        file << "AIFiles=" << data_.aiPath << "\n";
        file << "QFiles=" << data_.filesPath << "\n";
        file << "QGraphs=" << data_.graphsPath << "\n\n";
        file << "[Marker]\n";
        file << "ManipulatePositionSnapToGround=" << data_.keySnapGround << "\n";
        file << "ManipulatePositionSnapToObject=" << data_.keySnapObject << "\n";
        file << "MoveForward=Up\n";
        file << "MoveBackward=Down\n";
        file << "MoveLeft=Left\n";
        file << "MoveRight=Right\n";
        file << "ManipulateOrientationAlpha=" << data_.keyRotateAlpha << "\n";
        file << "ManipulateOrientationBeta=" << data_.keyRotateBeta << "\n";
        file << "ManipulateOrientationGamma=" << data_.keyRotateGamma << "\n";
        file << "ManipulateOrientationReset=Space\n";
        file << "ManipulatePositionReset=" << data_.keyResetPos << "\n";
        file << "TeleportToMarker=F11\n";
        file << "ResetMarkerToPlayer=" << data_.resetMarkerToPlayer << "\n\n";
        file << "[UI]\n";
        file << "FontSize=" << data_.fontSize << "\n";
        file << "FontColorR=" << data_.fontColorR << "\n";
        file << "FontColorG=" << data_.fontColorG << "\n";
        file << "FontColorB=" << data_.fontColorB << "\n\n";
        file << "[Logging]\n";
        file << "Enable=" << (data_.enableLogging ? "true" : "false") << "\n";
        file << "Debug=" << (data_.debugLogging ? "true" : "false") << "\n\n";
    }

    std::ofstream kb(GetKeybindingsPath());
    if (kb.is_open()) {
        kb << "// ================================================\n";
        kb << "// IGI EDITOR KEYBINDINGS\n";
        kb << "// ================================================\n\n";
        kb << "[Keybindings]\n";
        auto KeyToString = [](const KeyBinding& k, const std::string& keyName) -> std::string {
            std::string s;
            if (k.ctrl) s += "CTRL+";
            if (k.shift) s += "SHIFT+";
            if (k.alt) s += "ALT+";
            if (k.vkCode == 0x2D) s += "INSERT";
            else if (k.vkCode == 0x2E) s += "DELETE";
            else if (k.vkCode == 0x24) s += "HOME";
            else if (k.vkCode == 0x20) s += "SPACE";
            else if (k.vkCode >= 0x70 && k.vkCode <= 0x7B) s += "F" + std::to_string(k.vkCode - 0x70 + 1);
            else if (k.vkCode >= 0x41 && k.vkCode <= 0x5A) s += (char)k.vkCode;
            else if (k.vkCode) s += (char)k.vkCode;
            return s;
        };
        kb << "Save=" << KeyToString(data_.keySave, "Save") << "\n";
        kb << "ResetLevel=" << KeyToString(data_.keyResetLevel, "ResetLevel") << "\n";
        kb << "Debug=" << KeyToString(data_.keyDebug, "Debug") << "\n";
        kb << "Quit=" << KeyToString(data_.keyQuit, "Quit") << "\n";
        kb << "Help=" << KeyToString(data_.keyHelp, "Help") << "\n";
        kb << "ResetScript=" << KeyToString(data_.keyResetScript, "ResetScript") << "\n";
        kb << "EnableCamera=" << KeyToString(data_.keyEnableCamera, "EnableCamera") << "\n";
        kb << "MoveCameraForward=" << KeyToString(data_.keyMoveCameraForward, "MoveCameraForward") << "\n";
        kb << "MoveCameraBackward=" << KeyToString(data_.keyMoveCameraBackward, "MoveCameraBackward") << "\n";
        kb << "AdjustCameraRadius=" << KeyToString(data_.keyAdjustCameraRadius, "AdjustCameraRadius") << "\n";
        kb << "LookDown=" << KeyToString(data_.keyLookDown, "LookDown") << "\n";
        kb << "SnapToObject=" << KeyToString(data_.keySnapToObject, "SnapToObject") << "\n";
        kb << "SnapToGround=" << KeyToString(data_.keySnapToGround, "SnapToGround") << "\n";
        kb << "ClipMode=" << KeyToString(data_.keyClipMode, "ClipMode") << "\n";
        kb << "CreateNewTask=" << KeyToString(data_.keyCreateNewTask, "CreateNewTask") << "\n";
        kb << "CopyTask=" << KeyToString(data_.keyCopyTask, "CopyTask") << "\n";
        kb << "PasteTask=" << KeyToString(data_.keyPasteTask, "PasteTask") << "\n";
        kb << "DeleteTask=" << KeyToString(data_.keyDeleteTask, "DeleteTask") << "\n";
        kb << "AssignTaskID=" << KeyToString(data_.keyAssignTaskID, "AssignTaskID") << "\n";
        kb << "StartRecording=" << KeyToString(data_.keyStartRecording, "StartRecording") << "\n";
        kb << "GoToCursor=" << KeyToString(data_.keyGoToCursor, "GoToCursor") << "\n";
        kb << "SyncPlayback=" << KeyToString(data_.keySyncPlayback, "SyncPlayback") << "\n";
        kb << "Undo=" << KeyToString(data_.keyUndo, "Undo") << "\n";
        kb << "Redo=" << KeyToString(data_.keyRedo, "Redo") << "\n";
        kb << "ReloadSettings=" << KeyToString(data_.keyReloadSettings, "ReloadSettings") << "\n";
    }
}
