#include "pch.h"
#include "config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>

ConfigData Config::data_;

void Config::Init() {
    std::string path = GetConfigPath();
    if (!std::filesystem::exists(path)) {
        CreateDefault();
        Save();
    } else {
        Load();
    }
}

ConfigData& Config::Get() {
    return data_;
}

std::string Config::GetConfigPath() {
    return "config.ini";
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


    
    data_.moveForward = 'I';
    data_.moveBackward = 'K';
    data_.moveLeft = 'J';
    data_.moveRight = 'L';
    data_.moveUp = 'U';
    data_.moveDown = 'O';
    
    data_.rotateYawCCW = '[';
    data_.rotateYawCW = ']';
    data_.rotatePitchUp = ';';
    data_.rotatePitchDown = '\'';
    data_.rotateRollLeft = '.';
    data_.rotateRollRight = '/';
    
    data_.teleportToMarker = 'M';
    data_.resetMarkerToPlayer = 'H';
}

static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
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
            } else if (section == "Marker") {


                char c = (val.empty() ? 0 : val[0]);
                if (key == "MoveForward") data_.moveForward = c;
                else if (key == "MoveBackward") data_.moveBackward = c;
                else if (key == "MoveLeft") data_.moveLeft = c;
                else if (key == "MoveRight") data_.moveRight = c;
                else if (key == "MoveUp") data_.moveUp = c;
                else if (key == "MoveDown") data_.moveDown = c;
                else if (key == "RotateYawCCW") data_.rotateYawCCW = c;
                else if (key == "RotateYawCW") data_.rotateYawCW = c;
                else if (key == "RotatePitchUp") data_.rotatePitchUp = c;
                else if (key == "RotatePitchDown") data_.rotatePitchDown = c;
                else if (key == "RotateRollLeft") data_.rotateRollLeft = c;
                else if (key == "RotateRollRight") data_.rotateRollRight = c;
                else if (key == "TeleportToMarker") data_.teleportToMarker = c;
                else if (key == "ResetMarkerToPlayer") data_.resetMarkerToPlayer = c;
            }
        }
    }
}

void Config::Save() {
    std::ofstream file(GetConfigPath());
    if (!file.is_open()) return;

    file << "; ================================================" << std::endl;
    file << "; PROJECT IGI TERRAIN EDITOR CONFIGURATION" << std::endl;
    file << "; ================================================" << std::endl;
    file << std::endl;

    file << "[GamePath]" << std::endl;
    file << "; The root directory of your Project IGI installation." << std::endl;
    file << "; Example: IGIPath=C:\\Games\\Project IGI" << std::endl;
    file << "IGIPath=" << data_.igiPath << std::endl;
    file << std::endl;

    file << "; The level number to load on startup (1-14)." << std::endl;
    file << "Level=" << data_.level << std::endl;
    file << std::endl;

    file << "; Path to the editor's data resources (usually current directory '.')" << std::endl;
    file << "EditorPath=" << data_.editorPath << std::endl;
    file << std::endl;

    file << "; Path to the QEditor AppData folder (where IGIModels.json and QCompiler are located)." << std::endl;
    file << "; Example: QEditorPath=C:\\Users\\YourName\\AppData\\Roaming\\QEditor" << std::endl;
    file << "QEditorPath=" << data_.qEditorPath << std::endl;
    file << std::endl;

    file << "[Paths]" << std::endl;
    file << "; Granular paths for QEditor components (derived from QEditorPath by default)" << std::endl;
    file << "AIFiles=" << data_.aiPath << std::endl;
    file << "QCompiler=" << data_.compilerPath << std::endl;
    file << "QFiles=" << data_.filesPath << std::endl;
    file << "QGraphs=" << data_.graphsPath << std::endl;
    file << std::endl;

    file << "[Marker]" << std::endl;

    file << "; ------------------------------------------------" << std::endl;
    file << "; Object Manipulation Key Bindings" << std::endl;
    file << "; (Use capital letters for single keys)" << std::endl;
    file << "; ------------------------------------------------" << std::endl;
    file << std::endl;

    file << "; Marker Movement (World Space)" << std::endl;
    file << "MoveForward=" << data_.moveForward << std::endl;
    file << "MoveBackward=" << data_.moveBackward << std::endl;
    file << "MoveLeft=" << data_.moveLeft << std::endl;
    file << "MoveRight=" << data_.moveRight << std::endl;
    file << "MoveUp=" << data_.moveUp << std::endl;
    file << "MoveDown=" << data_.moveDown << std::endl;
    file << std::endl;

    file << "; Marker Rotation (Radians)" << std::endl;
    file << "RotateYawCCW=" << data_.rotateYawCCW << std::endl;
    file << "RotateYawCW=" << data_.rotateYawCW << std::endl;
    file << "RotatePitchUp=" << data_.rotatePitchUp << std::endl;
    file << "RotatePitchDown=" << data_.rotatePitchDown << std::endl;
    file << "RotateRollLeft=" << data_.rotateRollLeft << std::endl;
    file << "RotateRollRight=" << data_.rotateRollRight << std::endl;
    file << std::endl;

    file << "; Utility Shortcuts" << std::endl;
    file << "; TeleportToMarker: Moles camera to object" << std::endl;
    file << "TeleportToMarker=" << data_.teleportToMarker << std::endl;
    file << "; ResetMarkerToPlayer: Brings object to current camera position" << std::endl;
    file << "ResetMarkerToPlayer=" << data_.resetMarkerToPlayer << std::endl;
}

