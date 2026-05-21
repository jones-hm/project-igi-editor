/******************************************************************************
 * @file    mtp_parser.h
 * @brief   MTP (FORM-based model-texture package) parser
 *****************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct MTPModelTexture {
    std::string modelName;
    std::vector<std::string> textureNames;
};

struct MTPFile {
    std::vector<std::string> animations;    // BANM
    std::vector<std::string> shadows;       // SVOL
    std::vector<std::string> models;        // MODS
    std::vector<std::string> textures;      // TEXF
    std::vector<MTPModelTexture> mappings;  // INST resolved
    bool valid = false;
    std::string error;
};

// Parse an MTP file
MTPFile MTP_Parse(const std::string& filepath);
