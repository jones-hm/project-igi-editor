/******************************************************************************
 * @file    res_parser.cpp
 * @brief   RES (ILFF-based resource archive) parser
 *
 * RES files are ILFF containers with format ID "IRES".
 * Structure:
 *   ILFF Header (20 bytes): "ILFF" + uint32 size + uint32 align + uint32 skip + "IRES"
 *   Then repeated pairs of chunks:
 *     NAME chunk: 4-byte fourcc "NAME" + uint32 size + null-terminated filename
 *     BODY chunk: 4-byte fourcc "BODY" + uint32 size + raw binary data
 *****************************************************************************/

#include "res_parser.h"
#include "../logger.h"
#include <fstream>
#include <cstring>

static const uint32_t FOURCC_ILFF = 0x46464C49; // "ILFF" little-endian
static const uint32_t FOURCC_IRES = 0x53455249; // "IRES" little-endian
static const uint32_t FOURCC_NAME = 0x454D414E; // "NAME" little-endian
static const uint32_t FOURCC_BODY = 0x59444F42; // "BODY" little-endian
static const uint32_t FOURCC_CSTR = 0x52545343; // "CSTR" little-endian

static uint32_t ReadU32LE(const uint8_t* p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint32_t ReadFourCC(const uint8_t* p) {
    return ReadU32LE(p);
}

RESFile RES_Parse(const std::string& filepath) {
    RESFile result;

    // Read entire file into memory
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.error = "Could not open file: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[RES] " + result.error);
        return result;
    }

    std::streampos pos = file.tellg();
    if (pos == std::streampos(-1)) {
        result.error = "Could not determine file size (tellg failed): " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[RES] " + result.error);
        return result;
    }

    size_t fileSize = static_cast<size_t>(pos);
    if (fileSize < 20) {
        result.error = "File too small for ILFF header: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[RES] " + result.error);
        return result;
    }

    // Protect against insanely large files or failed reads giving large values
    if (fileSize > 1024 * 1024 * 1024) {
        result.error = "File is unreasonably large or size read failed (" + std::to_string(fileSize) + " bytes): " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[RES] " + result.error);
        return result;
    }

    std::vector<uint8_t> buf;
    try {
        buf.resize(fileSize);
    } catch (const std::bad_alloc&) {
        result.error = "Memory allocation failed for file size " + std::to_string(fileSize) + " bytes: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[RES] " + result.error);
        return result;
    }

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buf.data()), fileSize);
    file.close();

    const uint8_t* data = buf.data();

    // Validate ILFF header
    if (ReadFourCC(data) != FOURCC_ILFF) {
        result.error = "Not an ILFF file (missing ILFF magic): " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[RES] " + result.error);
        return result;
    }

    // Validate format ID "IRES" at offset 16
    if (ReadFourCC(data + 16) != FOURCC_IRES) {
        result.error = "Not an IRES resource file (wrong format ID): " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[RES] " + result.error);
        return result;
    }

    // Iterate chunks starting after the 20-byte ILFF header
    size_t offset = 20;
    std::string pendingName;
    bool haveName = false;

    while (offset + 16 <= fileSize) {
        uint32_t chunkFourCC = ReadFourCC(data + offset);
        uint32_t chunkSize = ReadU32LE(data + offset + 4);
        uint32_t chunkSkip = ReadU32LE(data + offset + 12);
        
        size_t dataOffset = offset + 16;
        if (chunkSize > fileSize || dataOffset > fileSize - chunkSize) {
            Logger::Get().Log(LogLevel::WARNING, "[RES] Chunk extends beyond file end at offset " +
                std::to_string(offset));
            break;
        }

        if (chunkFourCC == FOURCC_NAME) {
            // Read null-terminated string from chunk data
            const char* str = reinterpret_cast<const char*>(data + dataOffset);
            size_t maxLen = chunkSize;
            size_t len = 0;
            while (len < maxLen && str[len] != '\0') {
                len++;
            }
            pendingName = std::string(str, len);
            haveName = true;
        }
        else if (chunkFourCC == FOURCC_BODY || chunkFourCC == FOURCC_CSTR) {
            RESEntry entry;
            if (haveName) {
                entry.name = pendingName;
                haveName = false;
            }
            else {
                entry.name = "<unnamed_" + std::to_string(result.entries.size()) + ">";
            }
            try {
                entry.data.assign(data + dataOffset, data + dataOffset + chunkSize);
                result.entries.push_back(std::move(entry));
            } catch (const std::bad_alloc&) {
                Logger::Get().Log(LogLevel::ERR, "[RES] Memory allocation failed while parsing chunk '" + entry.name + "' of size " + std::to_string(chunkSize) + " at offset " + std::to_string(offset));
                break; // Stop parsing further chunks if memory is exhausted
            }
        }

        if (chunkSkip == 0) break;
        offset += chunkSkip;
    }

    result.valid = true;
    Logger::Get().Log(LogLevel::INFO, "[RES] Parsed " + filepath + " | Entries: " +
        std::to_string(result.entries.size()));

    return result;
}

std::vector<uint8_t> RES_Extract(const std::string& filepath, const std::string& resourceName) {
    RESFile res = RES_Parse(filepath);
    if (!res.valid) {
        return {};
    }

    for (const auto& entry : res.entries) {
        if (entry.name == resourceName) {
            return entry.data;
        }
    }

    Logger::Get().Log(LogLevel::WARNING, "[RES] Resource not found: " + resourceName + " in " + filepath);
    return {};
}
