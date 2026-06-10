#include "pch.h"
#include "cmd_fnt.h"
#include "fnt_parser.h"
#include <filesystem>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/tinygltf/stb_image_write.h"

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  gconv fnt info <file.fnt>\n"
        "  gconv fnt export <file.fnt> -o <out.png>\n";
}

int cmd_fnt(int argc, char** argv)
{
    // argv[0] = "fnt", argv[1] = subcommand
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];

    // ── info ─────────────────────────────────────────────────────────────────
    if (sub == "info")
    {
        if (argc < 3)
        {
            std::cerr << "fnt info: missing <file.fnt>\n";
            return 1;
        }
        std::string path = argv[2];
        if (!std::filesystem::exists(path))
        {
            std::cerr << "fnt info: file not found: " << path << "\n";
            return 2;
        }
        FntFont font = FNT_Parse(path);
        if (!font.valid)
        {
            std::cerr << "fnt info: parse error (invalid or unsupported FNT)\n";
            return 3;
        }
        std::cout << "file:        " << path << "\n";
        std::cout << "line_height: " << font.lineHeight << "\n";
        std::cout << "tex_width:   " << font.texWidth << "\n";
        std::cout << "tex_height:  " << font.texHeight << "\n";
        std::cout << "glyphs:      " << font.glyphs.size() << "\n";
        return 0;
    }

    // ── export ────────────────────────────────────────────────────────────────
    if (sub == "export")
    {
        if (argc < 3)
        {
            std::cerr << "fnt export: missing <file.fnt>\n";
            return 1;
        }
        std::string path = argv[2];
        if (!std::filesystem::exists(path))
        {
            std::cerr << "fnt export: file not found: " << path << "\n";
            return 2;
        }

        // Find -o <out.png>
        const char* out_path = nullptr;
        for (int i = 3; i < argc - 1; ++i)
            if (strcmp(argv[i], "-o") == 0) { out_path = argv[i + 1]; break; }

        if (!out_path)
        {
            std::cerr << "fnt export: missing -o <out.png>\n";
            return 1;
        }

        FntFont font = FNT_Parse(path);
        if (!font.valid)
        {
            std::cerr << "fnt export: parse error (invalid or unsupported FNT)\n";
            return 3;
        }
        if (font.rgba.empty())
        {
            std::cerr << "fnt export: no atlas data in FNT\n";
            return 3;
        }

        int rc = stbi_write_png(out_path, font.texWidth, font.texHeight, 4,
                                font.rgba.data(), font.texWidth * 4);
        if (!rc)
        {
            std::cerr << "fnt export: failed to write PNG: " << out_path << "\n";
            return 4;
        }
        std::cout << "fnt: exported atlas to " << out_path << "\n";
        return 0;
    }

    std::cerr << "fnt: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
