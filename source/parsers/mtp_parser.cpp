/******************************************************************************
 * @file    mtp_parser.cpp
 * @brief   MTP (FORM-based model-texture package) parser
 *
 * MTP files use a FORM/IFF container (big-endian sizes) containing
 * model-texture mappings.
 * Structure:
 *   Header: "FORM" (4 bytes) + big-endian size (4 bytes)
 *   Body starts with: "MTP " (4 bytes, with trailing space)
 *   Then chunks in order: BANM, SNDS, SVOL, MODS, VNAM, INST, TEXF, PALF, GTT
 *   Each chunk: 4-byte fourcc + 4-byte big-endian size + data
 *   String-array chunks: first uint32 (LE) is count, then count null-terminated strings
 *   Chunks aligned to 2-byte boundaries (standard IFF)
 *****************************************************************************/

#include "mtp_parser.h"
#include "../logger.h"
#include <fstream>
#include <cstring>

static uint32_t ReadU32BE(const uint8_t* p) {
    return ((uint32_t)p[0] << 24)
        | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)
        | (uint32_t)p[3];
}

static uint32_t ReadU32LE(const uint8_t* p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint16_t ReadU16LE(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool MatchFourCC(const uint8_t* p, const char* tag) {
    return p[0] == (uint8_t)tag[0]
        && p[1] == (uint8_t)tag[1]
        && p[2] == (uint8_t)tag[2]
        && p[3] == (uint8_t)tag[3];
}

// Parse a string array from chunk data: first uint32 (LE) is count, then
// count null-terminated strings packed sequentially
static std::vector<std::string> ParseStringArray(const uint8_t* data, uint32_t dataSize) {
    std::vector<std::string> strings;

    if (dataSize < 4) {
        return strings;
    }

    uint32_t count = ReadU32LE(data);
    size_t offset = 4;

    for (uint32_t i = 0; i < count && offset < dataSize; i++) {
        const char* str = reinterpret_cast<const char*>(data + offset);
        size_t maxLen = dataSize - offset;
        size_t len = 0;
        while (len < maxLen && str[len] != '\0') {
            len++;
        }
        strings.emplace_back(str, len);
        offset += len + 1; // skip past null terminator
    }

    return strings;
}

// Parse INST chunk: model-texture instance mappings
static std::vector<MTPModelTexture> ParseInstChunk(const uint8_t* data, uint32_t dataSize,
    const std::vector<std::string>& models, const std::vector<std::string>& textures) {
    std::vector<MTPModelTexture> mappings;

    if (dataSize < 4) {
        return mappings;
    }

    uint32_t count = ReadU32LE(data);
    size_t offset = 4;

    for (uint32_t i = 0; i < count && offset + 4 <= dataSize; i++) {
        uint16_t modelIdx = ReadU16LE(data + offset);
        offset += 2;
        uint16_t texCount = ReadU16LE(data + offset);
        offset += 2;

        MTPModelTexture mapping;
        if (modelIdx < models.size()) {
            mapping.modelName = models[modelIdx];
        }
        else {
            mapping.modelName = "<model_" + std::to_string(modelIdx) + ">";
        }

        for (uint16_t t = 0; t < texCount && offset + 2 <= dataSize; t++) {
            uint16_t texIdx = ReadU16LE(data + offset);
            offset += 2;

            if (texIdx < textures.size()) {
                mapping.textureNames.push_back(textures[texIdx]);
            }
            else {
                mapping.textureNames.push_back("<texture_" + std::to_string(texIdx) + ">");
            }
        }

        mappings.push_back(std::move(mapping));
    }

    return mappings;
}

MTPFile MTP_Parse(const std::string& filepath) {
    MTPFile result;

    // Read entire file into memory
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.error = "Could not open file: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[MTP] " + result.error);
        return result;
    }

    size_t fileSize = (size_t)file.tellg();
    if (fileSize < 12) {
        result.error = "File too small for FORM header: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[MTP] " + result.error);
        return result;
    }

    std::vector<uint8_t> buf(fileSize);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buf.data()), fileSize);
    file.close();

    const uint8_t* data = buf.data();

    // Validate FORM header
    if (!MatchFourCC(data, "FORM")) {
        result.error = "Not a FORM/IFF file (missing FORM magic): " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[MTP] " + result.error);
        return result;
    }

    uint32_t formSize = ReadU32BE(data + 4);
    (void)formSize; // informational; we use fileSize for bounds

    // Validate "MTP " format identifier at offset 8
    if (!MatchFourCC(data + 8, "MTP ")) {
        result.error = "Not an MTP file (wrong format ID): " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[MTP] " + result.error);
        return result;
    }

    // Parse chunks starting after "FORM" + size + "MTP " (12 bytes)
    size_t offset = 12;

    while (offset + 8 <= fileSize) {
        char fourcc[5] = {};
        memcpy(fourcc, data + offset, 4);
        uint32_t chunkSize = ReadU32BE(data + offset + 4);
        offset += 8;

        if (offset + chunkSize > fileSize) {
            Logger::Get().Log(LogLevel::WARNING, "[MTP] Chunk '" + std::string(fourcc) +
                "' extends beyond file end at offset " + std::to_string(offset - 8));
            break;
        }

        const uint8_t* chunkData = data + offset;

        if (MatchFourCC(reinterpret_cast<const uint8_t*>(fourcc), "BANM")) {
            result.animations = ParseStringArray(chunkData, chunkSize);
        }
        else if (MatchFourCC(reinterpret_cast<const uint8_t*>(fourcc), "SVOL")) {
            result.shadows = ParseStringArray(chunkData, chunkSize);
        }
        else if (MatchFourCC(reinterpret_cast<const uint8_t*>(fourcc), "MODS")) {
            result.models = ParseStringArray(chunkData, chunkSize);
        }
        else if (MatchFourCC(reinterpret_cast<const uint8_t*>(fourcc), "TEXF")) {
            result.textures = ParseStringArray(chunkData, chunkSize);
        }
        else if (MatchFourCC(reinterpret_cast<const uint8_t*>(fourcc), "INST")) {
            result.mappings = ParseInstChunk(chunkData, chunkSize,
                result.models, result.textures);
        }

        offset += chunkSize;

        // IFF chunks are aligned to 2-byte boundaries
        if (offset % 2 != 0) {
            offset++;
        }
    }

    result.valid = true;
    Logger::Get().Log(LogLevel::INFO, "[MTP] Parsed " + filepath +
        " | Models: " + std::to_string(result.models.size()) +
        " | Textures: " + std::to_string(result.textures.size()) +
        " | Mappings: " + std::to_string(result.mappings.size()));

    return result;
}
