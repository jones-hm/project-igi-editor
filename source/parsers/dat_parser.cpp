/******************************************************************************
 * @file    dat_parser.cpp
 * @brief   IGI level DAT model-texture mapping parser
 *****************************************************************************/

#include "dat_parser.h"
#include "../logger.h"
#include "../utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>

// Maximum plausible material count for a single model.
// The DAT has a texture-manifest section after all model entries; its header
// is the total texture count (e.g. "393"), which stoi() would parse as a huge
// number from the first texture name ("207_01_1" → 207). Stopping here
// prevents the parser from drifting into the manifest section.
static constexpr int kMaxMaterialsPerModel = 64;

DATFile DAT_Parse(const std::string& filepath) {
    DATFile result;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        result.error = "Cannot open file: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[DAT] " + result.error);
        return result;
    }

    // Tokenise: skip blank lines and *** comment lines
    std::vector<std::string> tokens;
    std::string line;
    while (std::getline(file, line)) {
        line = Utils::Trim(line);
        if (line.empty() || line.rfind("***", 0) == 0)
            continue;
        tokens.push_back(line);
    }

    if (tokens.empty()) {
        result.error = "Empty file: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[DAT] " + result.error);
        return result;
    }

    // token[0] = declared total model count
    try {
        result.declaredModelCount = std::stoi(tokens[0]);
    } catch (...) {
        result.error = "Failed to parse model count token: " + tokens[0];
        Logger::Get().Log(LogLevel::ERR, "[DAT] " + result.error);
        return result;
    }

    // --- Model section ---
    size_t cursor = 1;
    while (cursor + 1 < tokens.size()) {
        const std::string modelName = tokens[cursor++];

        int matCount = 0;
        try {
            matCount = std::stoi(tokens[cursor++]);
        } catch (...) {
            Logger::Get().Log(LogLevel::WARNING,
                "[DAT] Non-numeric material count after model '" + modelName +
                "' — stopping model parse");
            break;
        }

        // Count > kMaxMaterialsPerModel means we have overshot into the
        // texture-manifest section (e.g. "393" model → stoi("207_01_1")=207).
        if (matCount > kMaxMaterialsPerModel) {
            Logger::Get().Log(LogLevel::INFO,
                "[DAT] Reached texture-manifest section at model='" + modelName +
                "' count=" + std::to_string(matCount) + " — stopping model parse");
            // The texture manifest total count is in tokens[cursor-2] (modelName
            // before this iteration was actually the count token). Back up and
            // parse the texture section from here.
            // modelName here IS the texture manifest count token.
            try {
                result.declaredTextureCount = std::stoi(modelName);
            } catch (...) {}
            cursor -= 2; // rewind: we haven't consumed any textures yet
            break;
        }

        DATModelEntry entry;
        entry.modelName = modelName;
        for (int i = 0; i < matCount && cursor < tokens.size(); ++i)
            entry.textures.push_back(tokens[cursor++]);

        result.models.push_back(std::move(entry));
    }

    // --- Texture manifest section ---
    // After the model section the DAT has:
    //   <total_texture_count>
    //   <tex_1>
    //   ...
    // The break above may have left cursor pointing just before the count token
    // OR the count may already be in result.declaredTextureCount.
    if (result.declaredTextureCount == 0 && cursor < tokens.size()) {
        try {
            result.declaredTextureCount = std::stoi(tokens[cursor++]);
        } catch (...) {
            cursor++; // skip malformed token
        }
    } else {
        cursor++; // skip the count token we already identified
    }

    while (cursor < tokens.size())
        result.allTextures.push_back(tokens[cursor++]);

    Logger::Get().Log(LogLevel::INFO,
        "[DAT] Parsed " + filepath +
        " | models=" + std::to_string(result.models.size()) +
        " (declared=" + std::to_string(result.declaredModelCount) + ")" +
        " | textures=" + std::to_string(result.allTextures.size()) +
        " (declared=" + std::to_string(result.declaredTextureCount) + ")");

    result.valid = true;
    return result;
}

bool DAT_WriteNative(const DATFile& dat, const std::string& outPath, std::string& err) {
    // Open binary so we control CRLF exactly (no platform newline translation).
    std::ofstream f(outPath, std::ios::binary);
    if (!f.is_open()) {
        err = "Cannot open output file: " + outPath;
        Logger::Get().Log(LogLevel::ERR, "[DAT] " + err);
        return false;
    }

    auto emit = [&f](const std::string& s) { f << s << "\r\n"; };

    // Machine-generated header (two *** lines + a blank line — all skipped on read).
    emit("*** This file is machine generated");
    emit("*** DO NOT EDIT!");
    emit("");

    // Total model count (DAT_Parse reads this as tokens[0]).
    emit(std::to_string(dat.declaredModelCount));

    // Per-model: name, material count, then each texture. The trailing `waypoint`/`0`
    // entry is stored in dat.models like any other model (0 textures), so emitting
    // every model reproduces it verbatim.
    for (const auto& m : dat.models) {
        emit(m.modelName);
        emit(std::to_string(m.textures.size()));
        for (const auto& t : m.textures)
            emit(t);
    }

    // Texture manifest: total count, then each texture name.
    emit(std::to_string(dat.declaredTextureCount));
    for (const auto& t : dat.allTextures)
        emit(t);

    if (!f.good()) {
        err = "Write error to: " + outPath;
        Logger::Get().Log(LogLevel::ERR, "[DAT] " + err);
        return false;
    }
    Logger::Get().Log(LogLevel::INFO, "[DAT] Wrote native DAT to: " + outPath);
    return true;
}

void DAT_AddModel(DATFile& dat, const std::string& modelName,
                  const std::vector<std::string>& textureNames, bool& alreadyPresent) {
    alreadyPresent = false;

    // Idempotency check (ignore the `waypoint` sentinel).
    for (const auto& m : dat.models) {
        if (m.modelName == "waypoint")
            continue;
        if (m.modelName == modelName) {
            alreadyPresent = true;
            return;
        }
    }

    DATModelEntry entry;
    entry.modelName = modelName;
    entry.textures  = textureNames;

    // Insert before the trailing `waypoint` entry so the writer emits the new model
    // ahead of waypoint/0 (preserving the manifest-boundary structure DAT_Parse needs).
    auto wp = std::find_if(dat.models.begin(), dat.models.end(),
                           [](const DATModelEntry& m) { return m.modelName == "waypoint"; });
    dat.models.insert(wp, std::move(entry));
    dat.declaredModelCount += 1;

    // Add any new textures to the manifest, bumping the declared count per addition.
    for (const auto& t : textureNames) {
        if (std::find(dat.allTextures.begin(), dat.allTextures.end(), t) == dat.allTextures.end()) {
            dat.allTextures.push_back(t);
            dat.declaredTextureCount += 1;
        }
    }
}

std::string DAT_FormatReport(const DATFile& dat, const std::string& modelFilter) {
    std::ostringstream out;

    out << "=== IGI DAT Model-Texture Map ===\n"
        << "Models declared : " << dat.declaredModelCount  << "\n"
        << "Models parsed   : " << dat.models.size()       << "\n"
        << "Textures declared: " << dat.declaredTextureCount << "\n"
        << "Textures in manifest: " << dat.allTextures.size() << "\n"
        << "\n";

    for (const auto& m : dat.models) {
        if (!modelFilter.empty() &&
            m.modelName.find(modelFilter) == std::string::npos)
            continue;

        out << m.modelName << "\n"
            << "  materials: " << m.textures.size() << "\n";
        for (size_t i = 0; i < m.textures.size(); ++i)
            out << "  [" << i << "] " << m.textures[i] << "\n";
    }

    if (!modelFilter.empty())
        return out.str();

    // Append texture manifest
    out << "\n=== Texture Manifest (" << dat.allTextures.size() << " entries) ===\n";
    for (size_t i = 0; i < dat.allTextures.size(); ++i)
        out << "  [" << i << "] " << dat.allTextures[i] << "\n";

    return out.str();
}

bool DAT_WriteReport(const DATFile& dat, const std::string& outPath,
                     const std::string& modelFilter) {
    std::ofstream f(outPath);
    if (!f.is_open()) {
        Logger::Get().Log(LogLevel::ERR, "[DAT] Cannot write report to: " + outPath);
        return false;
    }
    f << DAT_FormatReport(dat, modelFilter);
    Logger::Get().Log(LogLevel::INFO, "[DAT] Report written to: " + outPath);
    return true;
}

// Escape a string for JSON (handles backslashes and double-quotes only;
// all DAT content is plain ASCII so no unicode escaping is needed).
static std::string JsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    out += '"';
    return out;
}

std::string DAT_FormatJSON(const DATFile& dat, const std::string& modelFilter) {
    std::ostringstream out;
    out << "{\n"
        << "  \"declaredModelCount\": "   << dat.declaredModelCount   << ",\n"
        << "  \"parsedModelCount\": "     << dat.models.size()        << ",\n"
        << "  \"declaredTextureCount\": " << dat.declaredTextureCount << ",\n"
        << "  \"manifestTextureCount\": " << dat.allTextures.size()   << ",\n"
        << "  \"models\": [\n";

    bool firstModel = true;
    for (const auto& m : dat.models) {
        if (!modelFilter.empty() &&
            m.modelName.find(modelFilter) == std::string::npos)
            continue;

        if (!firstModel) out << ",\n";
        firstModel = false;

        out << "    {\n"
            << "      \"modelName\": " << JsonStr(m.modelName) << ",\n"
            << "      \"materialCount\": " << m.textures.size() << ",\n"
            << "      \"textures\": [";

        for (size_t i = 0; i < m.textures.size(); ++i) {
            if (i > 0) out << ", ";
            out << JsonStr(m.textures[i]);
        }
        out << "]\n"
            << "    }";
    }
    out << "\n  ]";

    // Include texture manifest only when not filtering
    if (modelFilter.empty()) {
        out << ",\n  \"textureManifest\": [";
        for (size_t i = 0; i < dat.allTextures.size(); ++i) {
            if (i > 0) out << ", ";
            out << JsonStr(dat.allTextures[i]);
        }
        out << "]";
    }

    out << "\n}\n";
    return out.str();
}

bool DAT_WriteJSON(const DATFile& dat, const std::string& outPath,
                   const std::string& modelFilter) {
    std::ofstream f(outPath);
    if (!f.is_open()) {
        Logger::Get().Log(LogLevel::ERR, "[DAT] Cannot write JSON to: " + outPath);
        return false;
    }
    f << DAT_FormatJSON(dat, modelFilter);
    Logger::Get().Log(LogLevel::INFO, "[DAT] JSON written to: " + outPath);
    return true;
}
