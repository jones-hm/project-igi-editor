/******************************************************************************
 * @file    tex_parser.cpp
 * @brief   TEX (LOOP) texture parser for IGI-1 .tex / .spr / .pic files
 *
 * Binary layout derived from the reference Python parser:
 *   tools/dconv/format/tex.py  (IGI MEF CONV project)
 *
 * All multi-byte integers are little-endian.
 *****************************************************************************/

#include "tex_parser.h"
#include "../logger.h"

#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Read a plain value from a binary stream. Returns false on short read.
template<typename T>
bool ReadValue(std::ifstream& f, T& out) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&out), sizeof(T)));
}

// Read a raw block of bytes. Returns false on short read.
bool ReadBytes(std::ifstream& f, void* dst, std::size_t n) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(n)));
}

// Bytes-per-pixel for a given mode.
uint32_t DepthForMode(uint32_t mode) {
    switch (mode) {
        case 2:  return 2; // RGB565
        case 3:  return 4; // ARGB8888
        case 67: return 4; // ARGB8888
        default: return 0;
    }
}

// ---------------------------------------------------------------------------
// Pixel decoding
// ---------------------------------------------------------------------------

// Decode a raw bitmap into BGRA8888 pixels.
// rawData  : pointer to raw pixel bytes
// rawSize  : number of raw bytes (must equal w*h*depth)
// mode     : pixel mode (2, 3, or 67)
// Returns a vector of BGRA bytes (4 * w * h bytes), or empty on error.
std::vector<uint8_t> DecodeBitmap(const uint8_t* rawData, std::size_t rawSize,
                                  uint32_t w, uint32_t h, uint32_t mode) {
    const uint32_t depth = DepthForMode(mode);
    if (depth == 0) return {};
    if (rawSize < static_cast<std::size_t>(w) * h * depth) return {};

    std::vector<uint8_t> bgra(static_cast<std::size_t>(w) * h * 4);
    const uint8_t* src = rawData;
    uint8_t*       dst = bgra.data();

    const std::size_t pixelCount = static_cast<std::size_t>(w) * h;

    if (mode == 2) {
        // RGB565 → BGRA8888
        for (std::size_t i = 0; i < pixelCount; ++i) {
            uint16_t px = static_cast<uint16_t>(src[0]) |
                          (static_cast<uint16_t>(src[1]) << 8);
            src += 2;

            uint8_t r = static_cast<uint8_t>((px >> 11) << 3);
            uint8_t g = static_cast<uint8_t>(((px >> 5) & 0x3Fu) << 2);
            uint8_t b = static_cast<uint8_t>((px & 0x1Fu) << 3);

            dst[0] = b;
            dst[1] = g;
            dst[2] = r;
            dst[3] = 255;
            dst += 4;
        }
    } else {
        // ARGB8888 stored as [B, G, R, A] in file → BGRA output (already in order)
        // Python reference reads bytes directly; the channel order in the file is
        // interpreted as ARGB when viewed as a 32-bit LE integer, which means the
        // byte layout on disk is: B G R A  (index 0..3).
        // Our output format is also BGRA, so we can copy straight through.
        std::memcpy(dst, src, pixelCount * 4);
    }

    return bgra;
}

// ---------------------------------------------------------------------------
// Low-level structure readers
// ---------------------------------------------------------------------------

// Loop06 header: signature(4) version(4) _0(2) _1(2) _2(2) _3(2) count_x(4) count_y(4) = 24 bytes
// Followed by count_x*count_y Item06 records of 16 bytes each.
// We only need to know how many bytes to skip.
bool SkipLoop06(std::ifstream& f) {
    // Read the fixed 24-byte Loop06 header
    uint8_t hdr[24];
    if (!ReadBytes(f, hdr, 24)) return false;

    // Verify signature
    if (std::memcmp(hdr, "LOOP", 4) != 0) return false;

    // count_x is at bytes 16-19, count_y at bytes 20-23 (both uint32 LE)
    uint32_t count_x, count_y;
    std::memcpy(&count_x, hdr + 16, 4);
    std::memcpy(&count_y, hdr + 20, 4);

    // Each Item06 = 16 bytes (4 uint32s)
    const std::size_t skip = static_cast<std::size_t>(count_x) * count_y * 16;
    f.seekg(static_cast<std::streamoff>(skip), std::ios::cur);
    return f.good();
}

// ---------------------------------------------------------------------------
// Version parsers
// ---------------------------------------------------------------------------

// Loop02 layout (total header = 24 bytes, file starts at offset 0):
//   signature  4s   "LOOP"
//   version    I    2
//   _1         I
//   _2         I
//   width_line H
//   width      H
//   height     H
//   mode       H
// Then: width * height * depth raw pixel bytes
bool ParseVersion2(std::ifstream& f, TEXFile& out) {
    // We are at offset 0; read the whole 24-byte header.
    uint8_t hdr[24];
    if (!ReadBytes(f, hdr, 24)) {
        out.error = "v2: short read on header";
        return false;
    }

    uint16_t width, height, mode;
    std::memcpy(&width,  hdr + 16, 2);
    std::memcpy(&height, hdr + 18, 2);
    std::memcpy(&mode,   hdr + 22, 2);

    const uint32_t depth = DepthForMode(mode);
    if (depth == 0) {
        out.error = "v2: unknown pixel mode " + std::to_string(mode);
        return false;
    }

    const std::size_t rawSize = static_cast<std::size_t>(width) * height * depth;
    std::vector<uint8_t> raw(rawSize);
    if (!ReadBytes(f, raw.data(), rawSize)) {
        out.error = "v2: short read on pixel data";
        return false;
    }

    TEXImage img;
    img.width  = width;
    img.height = height;
    img.mode   = mode;
    img.pixels = DecodeBitmap(raw.data(), rawSize, width, height, mode);
    if (img.pixels.empty()) {
        out.error = "v2: pixel decode failed";
        return false;
    }

    out.images.push_back(std::move(img));
    return true;
}

// Loop07 / Loop09 share the same 52-byte outer header layout:
//   signature  4s   "LOOP"
//   version    I    7 or 9
//   _0..._4    5I
//   offset     I
//   count      I
//   _5         I
//   width      I    (top-level, used per-bitmap)
//   height     I
//   mode       I
// Total: 4+12*4 = 52 bytes
//
// Then:
//   v7: count * Item07 (40 bytes each)
//   v9: count * Item09 (32 bytes each)
//
// Then: count raw bitmaps of (width * height * depth) bytes each
// Then: Loop06 block (skipped)
bool ParseVersion7or9(std::ifstream& f, uint32_t version, TEXFile& out) {
    uint8_t hdr[52];
    if (!ReadBytes(f, hdr, 52)) {
        out.error = "v" + std::to_string(version) + ": short read on outer header";
        return false;
    }

    // Fields at fixed offsets within the 52-byte header (all uint32 LE):
    // [0-3]   signature
    // [4-7]   version
    // [8-11]  _0
    // [12-15] _1
    // [16-19] _2
    // [20-23] _3
    // [24-27] _4
    // [28-31] offset
    // [32-35] count
    // [36-39] _5
    // [40-43] width
    // [44-47] height
    // [48-51] mode

    uint32_t count, width, height, mode;
    std::memcpy(&count,  hdr + 32, 4);
    std::memcpy(&width,  hdr + 40, 4);
    std::memcpy(&height, hdr + 44, 4);
    std::memcpy(&mode,   hdr + 48, 4);

    if (count == 0 || count > 4096) {
        out.error = "v" + std::to_string(version) + ": implausible count=" + std::to_string(count);
        return false;
    }

    const uint32_t depth = DepthForMode(mode);
    if (depth == 0) {
        out.error = "v" + std::to_string(version) + ": unknown pixel mode=" + std::to_string(mode);
        return false;
    }

    // Skip item table: v7=40 bytes/item, v9=32 bytes/item
    const std::size_t itemSize   = (version == 7) ? 40u : 32u;
    const std::size_t tableBytes = itemSize * count;
    f.seekg(static_cast<std::streamoff>(tableBytes), std::ios::cur);
    if (!f.good()) {
        out.error = "v" + std::to_string(version) + ": seek past item table failed";
        return false;
    }

    // Read count bitmaps
    const std::size_t rawSize = static_cast<std::size_t>(width) * height * depth;
    for (uint32_t i = 0; i < count; ++i) {
        std::vector<uint8_t> raw(rawSize);
        if (!ReadBytes(f, raw.data(), rawSize)) {
            out.error = "v" + std::to_string(version) + ": short read on bitmap " + std::to_string(i);
            return false;
        }

        TEXImage img;
        img.width  = width;
        img.height = height;
        img.mode   = mode;
        img.pixels = DecodeBitmap(raw.data(), rawSize, width, height, mode);
        if (img.pixels.empty()) {
            out.error = "v" + std::to_string(version) + ": pixel decode failed for bitmap " + std::to_string(i);
            return false;
        }

        out.images.push_back(std::move(img));
    }

    // Skip the trailing Loop06 block
    SkipLoop06(f);

    return true;
}

// Loop11 layout (32-byte header):
//   signature  4s   "LOOP"
//   version    I    11
//   mode       I
//   multi      I
//   _0         H
//   _1         H
//   _2         H
//   _3         H
//   width      H
//   height     H
//   depth      H    (unused by us; mipmap count comes from reading until EOF)
// Total: 4+4*4+6*2 = 32 bytes
//
// Then: up to 10 mipmap bitmaps (each half the previous dimensions).
// Each bitmap size = (width>>i) * (height>>i) * depth_bytes.
// Reading stops when a read returns 0 bytes.
bool ParseVersion11(std::ifstream& f, TEXFile& out) {
    uint8_t hdr[32];
    if (!ReadBytes(f, hdr, 32)) {
        out.error = "v11: short read on header";
        return false;
    }

    // Layout:
    // [0-3]   signature
    // [4-7]   version
    // [8-11]  mode   (uint32)
    // [12-15] multi  (uint32)
    // [16-17] _0     (uint16)
    // [18-19] _1     (uint16)
    // [20-21] _2     (uint16)
    // [22-23] _3     (uint16)
    // [24-25] width  (uint16)
    // [26-27] height (uint16)
    // [28-29] depth  (uint16)  -- field name in Python, not bytes-per-pixel
    // [30-31] padding

    uint32_t mode;
    uint16_t width, height;
    std::memcpy(&mode,   hdr + 8,  4);
    std::memcpy(&width,  hdr + 24, 2);
    std::memcpy(&height, hdr + 26, 2);

    const uint32_t depthBytes = DepthForMode(mode);
    if (depthBytes == 0) {
        out.error = "v11: unknown pixel mode=" + std::to_string(mode);
        return false;
    }

    for (int i = 0; i < 10; ++i) {
        const uint32_t mw = (width  > 0) ? (width  >> i) : 0;
        const uint32_t mh = (height > 0) ? (height >> i) : 0;
        if (mw == 0 || mh == 0) break;

        const std::size_t rawSize = static_cast<std::size_t>(mw) * mh * depthBytes;
        std::vector<uint8_t> raw(rawSize);

        // Python: if len(d) == 0: break  — so a zero-length read ends the loop
        const std::streamsize bytesRead = f.read(reinterpret_cast<char*>(raw.data()),
                                                  static_cast<std::streamsize>(rawSize)).gcount();
        if (bytesRead == 0) break;

        // Partial reads are treated as a complete mipmap (match Python behaviour).
        const std::size_t actualSize = static_cast<std::size_t>(bytesRead);

        TEXImage img;
        img.width  = mw;
        img.height = mh;
        img.mode   = mode;
        img.pixels = DecodeBitmap(raw.data(), actualSize, mw, mh, mode);
        if (img.pixels.empty()) {
            // Non-fatal: just stop here
            break;
        }

        out.images.push_back(std::move(img));
    }

    return !out.images.empty();
}

// ---------------------------------------------------------------------------
// TGA writer
// ---------------------------------------------------------------------------

bool WriteTGA(const std::string& path, const TEXImage& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    const uint16_t w = static_cast<uint16_t>(img.width);
    const uint16_t h = static_cast<uint16_t>(img.height);

    // 18-byte TGA header
    uint8_t hdr[18] = {};
    hdr[0]  = 0;    // ID length
    hdr[1]  = 0;    // no colormap
    hdr[2]  = 2;    // uncompressed true-color
    // hdr[3..7]: colormap spec, all zero
    // hdr[8..9]: x-origin = 0
    // hdr[10..11]: y-origin = 0
    hdr[12] = static_cast<uint8_t>(w & 0xFF);
    hdr[13] = static_cast<uint8_t>((w >> 8) & 0xFF);
    hdr[14] = static_cast<uint8_t>(h & 0xFF);
    hdr[15] = static_cast<uint8_t>((h >> 8) & 0xFF);
    hdr[16] = 32;   // bits per pixel (always BGRA 32-bit)
    hdr[17] = 0x20; // image descriptor: top-left origin

    f.write(reinterpret_cast<const char*>(hdr), 18);
    f.write(reinterpret_cast<const char*>(img.pixels.data()),
            static_cast<std::streamsize>(img.pixels.size()));

    return f.good();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

TEXFile TEX_Parse(const std::string& filepath) {
    TEXFile result;

    std::ifstream f(filepath, std::ios::binary);
    if (!f) {
        result.error = "Cannot open file: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[TEX] " + result.error);
        return result;
    }

    // Read 8-byte preamble: signature(4) + version(4)
    uint8_t preamble[8];
    if (!ReadBytes(f, preamble, 8)) {
        result.error = "File too small to contain LOOP header: " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[TEX] " + result.error);
        return result;
    }

    if (std::memcmp(preamble, "LOOP", 4) != 0) {
        result.error = "Not a LOOP/TEX file (bad signature): " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[TEX] " + result.error);
        return result;
    }

    uint32_t version;
    std::memcpy(&version, preamble + 4, 4);
    result.version = version;

    if (version != 2 && version != 7 && version != 9 && version != 11) {
        result.error = "Unsupported TEX version " + std::to_string(version) + ": " + filepath;
        Logger::Get().Log(LogLevel::ERR, "[TEX] " + result.error);
        return result;
    }

    // Rewind to start (each version parser reads from offset 0)
    f.seekg(0, std::ios::beg);

    bool ok = false;
    switch (version) {
        case 2:  ok = ParseVersion2(f, result);             break;
        case 7:  ok = ParseVersion7or9(f, 7, result);       break;
        case 9:  ok = ParseVersion7or9(f, 9, result);       break;
        case 11: ok = ParseVersion11(f, result);            break;
    }

    if (!ok) {
        Logger::Get().Log(LogLevel::ERR, "[TEX] Parse failed for " + filepath + ": " + result.error);
        return result;
    }

    result.valid = true;
    Logger::Get().Log(LogLevel::INFO,
        "[TEX] Parsed v" + std::to_string(version) +
        " (" + std::to_string(result.images.size()) + " image(s)): " + filepath);
    return result;
}

int TEX_ExportTGA(const TEXFile& tex, const std::string& filepath, const std::string& outdir) {
    if (!tex.valid || tex.images.empty()) {
        Logger::Get().Log(LogLevel::WARNING, "[TEX] ExportTGA called on invalid/empty TEXFile");
        return 0;
    }

    // Derive base name from filepath (no extension)
    std::filesystem::path src(filepath);
    const std::string base = src.stem().string();

    // Ensure output directory exists
    std::error_code ec;
    std::filesystem::create_directories(outdir, ec);
    if (ec) {
        Logger::Get().Log(LogLevel::ERR, "[TEX] Cannot create output directory: " + outdir);
        return 0;
    }

    const bool multipleImages = tex.images.size() > 1;
    int written = 0;

    for (std::size_t i = 0; i < tex.images.size(); ++i) {
        std::string filename;
        if (multipleImages) {
            char suffix[8];
            std::snprintf(suffix, sizeof(suffix), "_%02zu", i);
            filename = base + suffix + ".tga";
        } else {
            filename = base + "_00.tga";
        }

        const std::string outpath = (std::filesystem::path(outdir) / filename).string();

        if (WriteTGA(outpath, tex.images[i])) {
            ++written;
            Logger::Get().Log(LogLevel::INFO, "[TEX] Exported: " + outpath);
        } else {
            Logger::Get().Log(LogLevel::ERR, "[TEX] Failed to write: " + outpath);
        }
    }

    return written;
}
