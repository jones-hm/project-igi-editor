#include "pch.h"
#include "cmd_res.h"
#include "res_parser.h"
#include <filesystem>

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  gconv res list <input.res>\n"
        "  gconv res extract <input.res> -o <output_dir>\n"
        "  gconv res extract <input.res> --file <name> -o <output_dir>\n";
}

// Return the value of a named option (e.g. "-o", "--file"), or nullptr if absent.
static const char* opt_val(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc - 1; ++i)
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return nullptr;
}

int cmd_res(int argc, char** argv)
{
    // argv[0] = "res", argv[1] = subcommand
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];

    // ── list ──────────────────────────────────────────────────────────────────
    if (sub == "list")
    {
        if (argc < 3)
        {
            std::cerr << "res list: missing <input.res>\n";
            return 1;
        }
        std::string path = argv[2];
        std::string err;
        bool ok = RES_ForEachEntry(path, [](const std::string& name, const uint8_t*, size_t) {
            std::cout << name << "\n";
        }, err);

        if (!ok)
        {
            std::cerr << "res list: " << err << "\n";
            // Distinguish file-not-found from parse error
            if (!std::filesystem::exists(path))
                return 2;
            return 3;
        }
        return 0;
    }

    // ── extract ───────────────────────────────────────────────────────────────
    if (sub == "extract")
    {
        if (argc < 3)
        {
            std::cerr << "res extract: missing <input.res>\n";
            return 1;
        }
        std::string path = argv[2];

        const char* out_dir  = opt_val(argc, argv, "-o");
        const char* only_file = opt_val(argc, argv, "--file");

        if (!out_dir)
        {
            std::cerr << "res extract: missing -o <output_dir>\n";
            return 1;
        }

        if (!std::filesystem::exists(path))
        {
            std::cerr << "res extract: file not found: " << path << "\n";
            return 2;
        }

        // Create output directory if it doesn't exist
        std::error_code ec;
        std::filesystem::create_directories(out_dir, ec);
        if (ec)
        {
            std::cerr << "res extract: cannot create output dir: " << ec.message() << "\n";
            return 4;
        }

        int extracted = 0;
        std::string err;
        bool ok = RES_ForEachEntry(path,
            [&](const std::string& name, const uint8_t* data, size_t size) {
                if (only_file && name != only_file)
                    return;

                // Use the base name of the entry so nested paths don't create
                // unexpected subdirectories in the output dir.
                std::filesystem::path entry_path(name);
                std::filesystem::path out_path =
                    std::filesystem::path(out_dir) / entry_path.filename();

                std::ofstream ofs(out_path, std::ios::binary);
                if (!ofs)
                {
                    std::cerr << "res extract: cannot write: " << out_path << "\n";
                    return;
                }
                ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
                ++extracted;
            }, err);

        if (!ok)
        {
            std::cerr << "res extract: " << err << "\n";
            return 3;
        }

        std::cout << "Extracted " << extracted << " file(s) to " << out_dir << "\n";
        return 0;
    }

    std::cerr << "res: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
