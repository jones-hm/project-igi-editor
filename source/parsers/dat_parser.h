/******************************************************************************
 * @file    dat_parser.h
 * @brief   IGI level DAT model-texture mapping parser
 *
 * DAT format (text, CRLF):
 *   *** comment lines (skipped)
 *   <blank lines skipped>
 *   <total_model_count>
 *   <model_name>          -- repeated for each model
 *   <material_count>
 *   <tex_name_1>
 *   ...
 *   <tex_name_N>
 *   waypoint              -- last model entry (0 materials)
 *   0
 *   <total_texture_count> -- start of texture manifest (not parsed as model)
 *   <tex_name_1>
 *   ...
 *****************************************************************************/

#pragma once
#include <string>
#include <vector>

struct DATModelEntry {
    std::string modelName;
    std::vector<std::string> textures;  // material/texture names in order
};

struct DATFile {
    std::vector<DATModelEntry> models;  // all model entries from model section
    std::vector<std::string>   allTextures;  // texture manifest section
    int  declaredModelCount   = 0;
    int  declaredTextureCount = 0;
    bool valid = false;
    std::string error;
};

// Parse a level DAT file (e.g. level1.dat).
// Returns structured model-texture mappings for all models.
DATFile DAT_Parse(const std::string& filepath);

// Format the parsed DAT as a JSON string.
// If modelFilter is non-empty, only entries whose modelName contains it are included.
std::string DAT_FormatJSON(const DATFile& dat, const std::string& modelFilter = "");

// Write the JSON report to a file. Returns true on success.
bool DAT_WriteJSON(const DATFile& dat, const std::string& outPath,
                   const std::string& modelFilter = "");

// Format the parsed DAT as a human-readable report string.
std::string DAT_FormatReport(const DATFile& dat, const std::string& modelFilter = "");

// Write the plain-text report to a file. Returns true on success.
bool DAT_WriteReport(const DATFile& dat, const std::string& outPath,
                     const std::string& modelFilter = "");
