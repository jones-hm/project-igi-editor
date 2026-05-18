#include "pch.h"
#include "cli_handler.h"
#include "utils.h"
#include "common.h"
#include "logger.h"
#include "renderer/mef_parser.h"
#include "level/mtp_parser.h"
#include "renderer/qvm_parser.h"
#include "compiler.h"
#include "decompiler.h"
#include "level/res_parser.h"
#include "level/terrain_files.h"

#include <iostream>

bool CLIHandler::IsCLICommand(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "--mef" || arg == "--qsc" || 
            arg == "--qvm" || arg == "--res" || arg == "--mtp" || arg == "--terrain") {
            return true;
        }
    }
    return false;
}

void CLIHandler::PrintHelp() {
    std::cout << "IGI Editor v1.0.0 - CLI & Headless Tool\n\n"
              << "GUI Editor Mode Options:\n"
              << "  -level <num>                           Load specific level (1-14)\n"
              << "  -w <width> -h <height>                 Set window dimensions\n"
              << "  -draw_parts <bitmask>                  Selective loading (1=terrain, 2=sky, 4=objects, 8=flat_sky, 16=buildings, 32=props, 64=AI)\n"
              << "  -stick_to_ground                       Enable ground sticking on load\n\n"
              << "Editor Keyboard Controls:\n"
              << "  W/S/A/D       : Movement (Forward/Backward/Left/Right)\n"
              << "  Q/Z           : Vertical Movement (Up/Down)\n"
              << "  F4            : Toggle Edit Cursor (Global Edit Mode)\n"
              << "  F3            : Toggle Collision / Clipping\n"
              << "  F2            : Toggle Terrain Painting Mode\n"
              << "  PageUp/Dn     : Adjust Movement Speed\n"
              << "  Alt+Enter     : Toggle fullscreen mode\n"
              << "  Left/Right    : Change roll\n\n"
              << "CLI Parsing & Testing Modes (Headless):\n"
              << "  --help                                 Show this message\n"
              << "  --mef <file.mef>                       Parse and print MEF model details\n"
              << "  --qsc <file.qsc> --compile <out.qvm>   Compile QSC script to QVM\n"
              << "  --qvm <file.qvm> --decompile <out.qsc> Decompile QVM back to QSC\n"
              << "  --qvm <file.qvm>                       Parse QVM bytecode details\n"
              << "  --res <file.res>                       List RES archive contents\n"
              << "  --res <file.res> --extract <name> <out> Extract specific resource\n"
              << "  --mtp <file.mtp>                       Parse MTP texture mappings\n"
              << "  --terrain <file.lmp/.ctr>              Parse terrain structure\n";
}

int CLIHandler::Process(int argc, char** argv) {
    std::string argsStr;
    for (int i = 0; i < argc; ++i) {
        if (i > 0) argsStr += " ";
        argsStr += argv[i];
    }
    Logger::Get().Log(LogLevel::INFO, "[CLI] Process called with arguments: " + argsStr);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            Logger::Get().Log(LogLevel::INFO, "[CLI] Help command requested.");
            PrintHelp();
            return 0;
        } else if (arg == "--mef" && i + 1 < argc) {
            return ParseMEF(argv[++i]);
        } else if (arg == "--mtp" && i + 1 < argc) {
            return ParseMTP(argv[++i]);
        } else if (arg == "--qsc" && i + 1 < argc) {
            std::string inpath = argv[++i];
            if (i + 1 < argc && std::string(argv[i+1]) == "--compile" && i + 2 < argc) {
                return CompileQSC(inpath, argv[i+2]);
            }
        } else if (arg == "--qvm" && i + 1 < argc) {
            std::string inpath = argv[++i];
            if (i + 1 < argc && std::string(argv[i+1]) == "--decompile" && i + 2 < argc) {
                return ParseQVM(inpath, true, argv[i+2]);
            } else {
                return ParseQVM(inpath, false, "");
            }
        } else if (arg == "--res" && i + 1 < argc) {
            std::string inpath = argv[++i];
            if (i + 1 < argc && std::string(argv[i+1]) == "--extract" && i + 3 < argc) {
                return ParseRES(inpath, argv[i+2], argv[i+3]);
            } else {
                return ParseRES(inpath, "", "");
            }
        } else if (arg == "--terrain" && i + 1 < argc) {
            return ParseTerrain(argv[++i]);
        }
    }
    Logger::Get().Log(LogLevel::WARNING, "[CLI] Unknown or invalid arguments processed.");
    PrintHelp();
    return 1;
}

int CLIHandler::ParseMEF(const std::string& filepath) {
    Logger::Get().Log(LogLevel::INFO, "[CLI] Parsing MEF file: " + filepath);
    MEFParser parser;
    auto objects = parser.parse_file(filepath);
    if (!objects.empty()) {
        Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully parsed MEF. Found " + std::to_string(objects.size()) + " objects.");
        for (const auto& obj : objects) {
            Logger::Get().Log(LogLevel::INFO, "[CLI] Object: " + obj.name);
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Vertices: " + std::to_string(obj.vertices.size()));
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Faces: " + std::to_string(obj.faces.size()));
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Materials: " + std::to_string(obj.materials.size()));
        }
        return 0;
    } else {
        Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to parse MEF or file is empty: " + filepath);
        return 1;
    }
}

int CLIHandler::ParseQVM(const std::string& filepath, bool decompile, const std::string& outpath) {
    if (decompile) {
        Logger::Get().Log(LogLevel::INFO, "[CLI] Decompiling QVM: " + filepath + " -> " + outpath);
        Decompiler d;
        if (d.Decompile(filepath, outpath)) {
            Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully decompiled QVM to: " + outpath);
            return 0;
        }
        Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to decompile QVM: " + filepath);
        return 1;
    } else {
        Logger::Get().Log(LogLevel::INFO, "[CLI] Parsing QVM: " + filepath);
        QVMFile qvm = QVM_Parse(filepath);
        if (qvm.valid) {
            Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully parsed QVM header and strings.");
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Instructions: " + std::to_string(qvm.totalInstructions()));
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Identifiers: " + std::to_string(qvm.identifierCount()));
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Strings: " + std::to_string(qvm.stringCount()));
            return 0;
        }
        Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to parse QVM: " + qvm.error);
        return 1;
    }
}

int CLIHandler::CompileQSC(const std::string& inpath, const std::string& outpath) {
    Logger::Get().Log(LogLevel::INFO, "[CLI] Compiling QSC: " + inpath + " -> " + outpath);
    Compiler c;
    if (c.Compile(inpath, outpath)) {
        Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully compiled QSC to: " + outpath);
        return 0;
    }
    Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to compile QSC: " + inpath);
    return 1;
}

int CLIHandler::ParseRES(const std::string& filepath, const std::string& extract_name, const std::string& outpath) {
    if (!extract_name.empty()) {
        Logger::Get().Log(LogLevel::INFO, "[CLI] Extracting '" + extract_name + "' from " + filepath + " -> " + outpath);
        std::vector<uint8_t> data = RES_Extract(filepath, extract_name);
        if (!data.empty()) {
            if (File_SaveBinary(outpath.c_str(), data.data(), data.size())) {
                Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully extracted resource.");
                return 0;
            } else {
                Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to save extracted resource to file: " + outpath);
                return 1;
            }
        }
        Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to extract resource: block not found or empty in " + filepath);
        return 1;
    } else {
        Logger::Get().Log(LogLevel::INFO, "[CLI] Parsing RES archive: " + filepath);
        RESFile res = RES_Parse(filepath);
        if (res.valid) {
            Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully parsed RES archive. Found " + std::to_string(res.entries.size()) + " entries.");
            for (const auto& entry : res.entries) {
                Logger::Get().Log(LogLevel::INFO, "[CLI]   - " + entry.name + " (" + std::to_string(entry.data.size()) + " bytes)");
            }
            return 0;
        }
        Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to parse RES archive: " + res.error);
        return 1;
    }
}

int CLIHandler::ParseMTP(const std::string& filepath) {
    Logger::Get().Log(LogLevel::INFO, "[CLI] Parsing MTP file: " + filepath);
    MTPFile mtp = MTP_Parse(filepath);
    if (mtp.valid) {
        Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully parsed MTP.");
        Logger::Get().Log(LogLevel::INFO, "[CLI]   Animations: " + std::to_string(mtp.animations.size()));
        Logger::Get().Log(LogLevel::INFO, "[CLI]   Shadows: " + std::to_string(mtp.shadows.size()));
        Logger::Get().Log(LogLevel::INFO, "[CLI]   Models: " + std::to_string(mtp.models.size()));
        Logger::Get().Log(LogLevel::INFO, "[CLI]   Textures: " + std::to_string(mtp.textures.size()));
        Logger::Get().Log(LogLevel::INFO, "[CLI]   Mappings: " + std::to_string(mtp.mappings.size()));
        return 0;
    } else {
        Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to parse MTP: " + mtp.error);
        return 1;
    }
}

int CLIHandler::ParseTerrain(const std::string& filepath) {
    Logger::Get().Log(LogLevel::INFO, "[CLI] Parsing Terrain file: " + filepath);
    
    // Determine type by extension
    std::string ext = filepath.substr(filepath.find_last_of(".") + 1);
    if (ext == "lmp" || ext == "LMP") {
        pics_s pics;
        if (LMP_Load(filepath.c_str(), pics)) {
            Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully loaded Lightmap (LMP).");
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Textures: " + std::to_string(pics.num_pic_));
            Pic_FreePics(pics);
            return 0;
        }
    } else if (ext == "ctr" || ext == "CTR") {
        ctr_s ctr;
        if (CTR_Load(filepath.c_str(), ctr)) {
            Logger::Get().Log(LogLevel::INFO, "[CLI] Successfully loaded Quadtree (CTR).");
            Logger::Get().Log(LogLevel::INFO, "[CLI]   Items: " + std::to_string(ctr.num_item_));
            CTR_Free(ctr);
            return 0;
        }
    } else {
        Logger::Get().Log(LogLevel::ERR, "[CLI] Unknown terrain format: " + ext);
        return 1;
    }
    
    Logger::Get().Log(LogLevel::ERR, "[CLI] Failed to parse Terrain file: " + filepath);
    return 1;
}