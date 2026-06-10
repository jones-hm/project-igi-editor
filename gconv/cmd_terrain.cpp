#include "pch.h"
#include "cmd_terrain.h"
#include "terrain_files.h"
#include "common.h"   // pics_s, pic_s, Pic_FreePics

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  gconv terrain export-lmp <input.lmp> -o <output.pgm>\n"
        "  gconv terrain export-ctr <input.ctr> -o <output.json>\n"
        "  gconv terrain info <input.lmp|.ctr>\n";
}

// Return value of named option, or nullptr.
static const char* opt_val(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc - 1; ++i)
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return nullptr;
}

// Write a 16-bit binary PGM (P5) from a single 8-bit greyscale pic_s.
// The LMP format stores luma in channel 0 of each RGBA pixel.
static bool write_pgm16(const char* out_path, const pic_s* pic)
{
    std::ofstream ofs(out_path, std::ios::binary);
    if (!ofs)
        return false;

    int w = pic->width_;
    int h = pic->height_;

    // PGM P5 header
    ofs << "P5\n" << w << " " << h << "\n65535\n";

    // Each pixel: luma value from channel 0, scaled 8-bit -> 16-bit (big-endian)
    for (int i = 0; i < w * h; ++i)
    {
        uint8_t luma = pic->pixels_[i * 4];             // R == G == B == luma
        uint16_t val16 = static_cast<uint16_t>(luma) * 257u; // 0x00->0x0000, 0xFF->0xFFFF
        uint8_t hi = static_cast<uint8_t>(val16 >> 8);
        uint8_t lo = static_cast<uint8_t>(val16 & 0xFF);
        ofs.put(static_cast<char>(hi));
        ofs.put(static_cast<char>(lo));
    }
    return ofs.good();
}

// Write CTR items as a JSON array.
static bool write_ctr_json(const char* out_path, const ctr_s& ctr)
{
    std::ofstream ofs(out_path);
    if (!ofs)
        return false;

    if (ctr.num_item_ == 0)
    {
        ofs << "[]\n";
        return ofs.good();
    }

    ofs << "[\n";
    for (int i = 0; i < ctr.num_item_; ++i)
    {
        const ctr_item_s& item = ctr.head_[i];
        if (i) ofs << ",\n";
        ofs << "  {\n";
        ofs << "    \"index\": " << i << ",\n";

        // children array (8 entries)
        ofs << "    \"children\": [";
        for (int c = 0; c < 8; ++c)
        {
            if (c) ofs << ",";
            ofs << item.children_[c];
        }
        ofs << "],\n";

        // cmd_transform array (8 entries)
        ofs << "    \"cmd_transform\": [";
        for (int c = 0; c < 8; ++c)
        {
            if (c) ofs << ",";
            ofs << static_cast<int>(item.cmd_transform_[c]);
        }
        ofs << "],\n";

        ofs << "    \"children_mask\": " << static_cast<unsigned>(item.children_mask_) << ",\n";
        ofs << "    \"cmd_offset\": "    << item.cmd_offset_ << "\n";
        ofs << "  }";
    }
    ofs << "\n]\n";
    return ofs.good();
}

int cmd_terrain(int argc, char** argv)
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];

    // ── export-lmp ────────────────────────────────────────────────────────────
    if (sub == "export-lmp")
    {
        if (argc < 3)
        {
            std::cerr << "terrain export-lmp: missing <input.lmp>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = opt_val(argc, argv, "-o");
        if (!out_path)
        {
            std::cerr << "terrain export-lmp: missing -o <output.pgm>\n";
            return 1;
        }

        if (!std::filesystem::exists(path))
        {
            std::cerr << "terrain export-lmp: file not found: " << path << "\n";
            return 2;
        }

        pics_s pics = {};
        if (!LMP_Load(path.c_str(), pics))
        {
            std::cerr << "terrain export-lmp: failed to load " << path << "\n";
            return 3;
        }

        if (pics.num_pic_ == 0 || !pics.pics_)
        {
            std::cerr << "terrain export-lmp: no pictures in file\n";
            Pic_FreePics(pics);
            return 3;
        }

        // Export all pics; if more than one, suffix the filename.
        bool any_fail = false;
        for (int i = 0; i < pics.num_pic_; ++i)
        {
            const pic_s* pic = &pics.pics_[i];
            if (!pic->pixels_) continue;

            std::string actual_path = out_path;
            if (pics.num_pic_ > 1)
            {
                // Insert index before extension: e.g. "out.pgm" -> "out_0.pgm"
                std::filesystem::path p(out_path);
                std::string stem = p.stem().string() + "_" + std::to_string(i);
                actual_path = (p.parent_path() / (stem + p.extension().string())).string();
            }

            if (!write_pgm16(actual_path.c_str(), pic))
            {
                std::cerr << "terrain export-lmp: write failed: " << actual_path << "\n";
                any_fail = true;
            }
            else
            {
                std::cout << "Wrote " << actual_path
                          << " (" << pic->width_ << "x" << pic->height_ << ")\n";
            }
        }

        Pic_FreePics(pics);
        return any_fail ? 4 : 0;
    }

    // ── export-ctr ────────────────────────────────────────────────────────────
    if (sub == "export-ctr")
    {
        if (argc < 3)
        {
            std::cerr << "terrain export-ctr: missing <input.ctr>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = opt_val(argc, argv, "-o");
        if (!out_path)
        {
            std::cerr << "terrain export-ctr: missing -o <output.json>\n";
            return 1;
        }

        if (!std::filesystem::exists(path))
        {
            std::cerr << "terrain export-ctr: file not found: " << path << "\n";
            return 2;
        }

        ctr_s ctr = {};
        if (!CTR_Load(path.c_str(), ctr))
        {
            std::cerr << "terrain export-ctr: failed to load " << path << "\n";
            return 3;
        }

        if (!write_ctr_json(out_path, ctr))
        {
            std::cerr << "terrain export-ctr: write failed: " << out_path << "\n";
            CTR_Free(ctr);
            return 4;
        }

        std::cout << "Wrote " << out_path << " (" << ctr.num_item_ << " items)\n";
        CTR_Free(ctr);
        return 0;
    }

    // ── info ──────────────────────────────────────────────────────────────────
    if (sub == "info")
    {
        if (argc < 3)
        {
            std::cerr << "terrain info: missing <input>\n";
            return 1;
        }
        std::string path = argv[2];

        if (!std::filesystem::exists(path))
        {
            std::cerr << "terrain info: file not found: " << path << "\n";
            return 2;
        }

        std::string ext = std::filesystem::path(path).extension().string();
        // Normalise to lowercase
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (ext == ".lmp")
        {
            pics_s pics = {};
            if (!LMP_Load(path.c_str(), pics))
            {
                std::cerr << "terrain info: failed to load " << path << "\n";
                return 3;
            }
            std::cout << "File:    " << path << "\n";
            std::cout << "Type:    LMP (lightmap)\n";
            std::cout << "Pics:    " << pics.num_pic_ << "\n";
            for (int i = 0; i < pics.num_pic_; ++i)
            {
                const pic_s* p = &pics.pics_[i];
                std::cout << "  [" << i << "] " << p->width_ << "x" << p->height_ << "\n";
            }
            Pic_FreePics(pics);
        }
        else if (ext == ".ctr")
        {
            ctr_s ctr = {};
            if (!CTR_Load(path.c_str(), ctr))
            {
                std::cerr << "terrain info: failed to load " << path << "\n";
                return 3;
            }
            std::cout << "File:    " << path << "\n";
            std::cout << "Type:    CTR (quad-tree)\n";
            std::cout << "Items:   " << ctr.num_item_ << "\n";
            CTR_Free(ctr);
        }
        else
        {
            std::cerr << "terrain info: unrecognised extension '" << ext
                      << "' (expected .lmp or .ctr)\n";
            return 1;
        }
        return 0;
    }

    std::cerr << "terrain: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
