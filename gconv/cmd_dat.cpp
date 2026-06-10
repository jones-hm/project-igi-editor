#include "pch.h"
#include "cmd_dat.h"
#include "dat_parser.h"
#include "mtp_tool.h"
#include <filesystem>

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  gconv1 dat info <file.dat>\n"
        "  gconv1 dat export <file.dat> [-o <out.json>] [--filter <model>] [--text]\n"
        "  gconv1 dat to-mtp <file.dat> [-o <out.mtp>]\n";
}

static const char* opt_val(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc - 1; ++i)
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return nullptr;
}

static bool has_flag(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], name) == 0)
            return true;
    return false;
}

int cmd_dat(int argc, char** argv)
{
    // argv[0] = "dat", argv[1] = subcommand
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
            std::cerr << "dat info: missing <file.dat>\n";
            return 1;
        }
        std::string path = argv[2];
        if (!std::filesystem::exists(path))
        {
            std::cerr << "dat info: file not found: " << path << "\n";
            return 2;
        }
        DATFile dat = DAT_Parse(path);
        if (!dat.valid)
        {
            std::cerr << "dat info: parse error: " << dat.error << "\n";
            return 3;
        }
        std::cout << "file:            " << path << "\n";
        std::cout << "models:          " << dat.models.size() << "\n";
        std::cout << "declared_models: " << dat.declaredModelCount << "\n";
        std::cout << "textures:        " << dat.allTextures.size() << "\n";
        std::cout << "declared_tex:    " << dat.declaredTextureCount << "\n";
        return 0;
    }

    // ── export ────────────────────────────────────────────────────────────────
    if (sub == "export")
    {
        if (argc < 3)
        {
            std::cerr << "dat export: missing <file.dat>\n";
            return 1;
        }
        std::string path = argv[2];
        if (!std::filesystem::exists(path))
        {
            std::cerr << "dat export: file not found: " << path << "\n";
            return 2;
        }
        DATFile dat = DAT_Parse(path);
        if (!dat.valid)
        {
            std::cerr << "dat export: parse error: " << dat.error << "\n";
            return 3;
        }

        const char* out_path   = opt_val(argc, argv, "-o");
        const char* filter_str = opt_val(argc, argv, "--filter");
        bool text_mode         = has_flag(argc, argv, "--text");

        std::string filter = filter_str ? filter_str : "";

        if (out_path)
        {
            bool ok = text_mode ? DAT_WriteReport(dat, out_path, filter)
                                : DAT_WriteJSON(dat, out_path, filter);
            if (!ok)
            {
                std::cerr << "dat export: failed to write: " << out_path << "\n";
                return 4;
            }
            std::cout << "Written to: " << out_path << "\n";
        }
        else
        {
            std::cout << (text_mode ? DAT_FormatReport(dat, filter)
                                    : DAT_FormatJSON(dat, filter));
        }
        return 0;
    }

    // ── to-mtp ────────────────────────────────────────────────────────────────
    if (sub == "to-mtp")
    {
        if (argc < 3)
        {
            std::cerr << "dat to-mtp: missing <file.dat>\n";
            return 1;
        }
        std::string dat_path = argv[2];
        if (!std::filesystem::exists(dat_path))
        {
            std::cerr << "dat to-mtp: file not found: " << dat_path << "\n";
            return 2;
        }

        // Locate mtp_decoder.exe relative to gconv1 executable
        // For gconv1, the exe is in bin/Release/content/tools/; mtp_decoder is a sibling.
        // Use the same directory as gconv1.
        std::string exe_dir;
#ifdef _WIN32
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        exe_dir = std::filesystem::path(buf).parent_path().string();
#endif
        std::string decoder_exe = exe_dir + "\\mtp_decoder.exe";
        if (!std::filesystem::exists(decoder_exe))
        {
            std::cerr << "dat to-mtp: mtp_decoder.exe not found at: " << decoder_exe << "\n";
            std::cerr << "  Place mtp_decoder.exe in the same directory as gconv1.exe\n";
            return 1;
        }

        // Determine output MTP path
        const char* out_arg = opt_val(argc, argv, "-o");
        std::string sibling_mtp = std::filesystem::path(dat_path).replace_extension(".mtp").string();
        std::string final_mtp   = out_arg ? std::string(out_arg) : sibling_mtp;

        std::string err;
        if (!RunMtpDecoder(decoder_exe, dat_path, sibling_mtp, err))
        {
            std::cerr << "dat to-mtp: mtp_decoder failed: " << err << "\n";
            return 3;
        }

        if (out_arg && std::string(out_arg) != sibling_mtp)
        {
            std::error_code ec;
            std::filesystem::copy_file(sibling_mtp, final_mtp,
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                std::cerr << "dat to-mtp: copy failed: " << ec.message() << "\n";
                return 4;
            }
        }
        std::cout << "Generated MTP: " << final_mtp << "\n";
        return 0;
    }

    std::cerr << "dat: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
