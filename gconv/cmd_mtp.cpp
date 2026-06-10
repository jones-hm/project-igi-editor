#include "pch.h"
#include "cmd_mtp.h"
#include "mtp_parser.h"

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  gconv mtp dump <input.mtp> [-o <output.json>]\n"
        "  gconv mtp info <input.mtp>\n";
}

// Minimal JSON string escaping
static std::string json_str(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s)
    {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    out += '"';
    return out;
}

// Emit a JSON array of strings
static std::string json_str_array(const std::vector<std::string>& v)
{
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i) out += ",";
        out += json_str(v[i]);
    }
    out += "]";
    return out;
}

static void write_json(std::ostream& os, const MTPFile& mtp)
{
    if (!mtp.valid ||
        (mtp.animations.empty() && mtp.shadows.empty() &&
         mtp.models.empty()     && mtp.textures.empty() &&
         mtp.mappings.empty()))
    {
        os << "{}\n";
        return;
    }

    os << "{\n";
    os << "  \"animations\": " << json_str_array(mtp.animations) << ",\n";
    os << "  \"shadows\": "    << json_str_array(mtp.shadows)    << ",\n";
    os << "  \"models\": "     << json_str_array(mtp.models)     << ",\n";
    os << "  \"textures\": "   << json_str_array(mtp.textures)   << ",\n";

    os << "  \"mappings\": [";
    for (size_t i = 0; i < mtp.mappings.size(); ++i)
    {
        if (i) os << ",";
        os << "\n    {\"model\": " << json_str(mtp.mappings[i].modelName)
           << ", \"textures\": "  << json_str_array(mtp.mappings[i].textureNames) << "}";
    }
    os << "\n  ]\n";
    os << "}\n";
}

int cmd_mtp(int argc, char** argv)
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];

    // ── dump ──────────────────────────────────────────────────────────────────
    if (sub == "dump")
    {
        if (argc < 3)
        {
            std::cerr << "mtp dump: missing <input.mtp>\n";
            return 1;
        }
        std::string path = argv[2];

        const char* out_path = nullptr;
        for (int i = 3; i < argc - 1; ++i)
            if (strcmp(argv[i], "-o") == 0)
                out_path = argv[i + 1];

        if (!std::filesystem::exists(path))
        {
            std::cerr << "mtp dump: file not found: " << path << "\n";
            return 2;
        }

        MTPFile mtp = MTP_Parse(path);
        if (!mtp.valid)
        {
            std::cerr << "mtp dump: parse error: " << mtp.error << "\n";
            return 3;
        }

        if (out_path)
        {
            std::ofstream ofs(out_path);
            if (!ofs)
            {
                std::cerr << "mtp dump: cannot write: " << out_path << "\n";
                return 4;
            }
            write_json(ofs, mtp);
        }
        else
        {
            write_json(std::cout, mtp);
        }
        return 0;
    }

    // ── info ──────────────────────────────────────────────────────────────────
    if (sub == "info")
    {
        if (argc < 3)
        {
            std::cerr << "mtp info: missing <input.mtp>\n";
            return 1;
        }
        std::string path = argv[2];

        if (!std::filesystem::exists(path))
        {
            std::cerr << "mtp info: file not found: " << path << "\n";
            return 2;
        }

        MTPFile mtp = MTP_Parse(path);
        if (!mtp.valid)
        {
            std::cerr << "mtp info: parse error: " << mtp.error << "\n";
            return 3;
        }

        std::cout << "File:       " << path << "\n";
        std::cout << "Animations: " << mtp.animations.size() << "\n";
        std::cout << "Shadows:    " << mtp.shadows.size()    << "\n";
        std::cout << "Models:     " << mtp.models.size()     << "\n";
        std::cout << "Textures:   " << mtp.textures.size()   << "\n";
        std::cout << "Mappings:   " << mtp.mappings.size()   << "\n";

        if (!mtp.models.empty())
        {
            std::cout << "\nModel list:\n";
            for (const auto& m : mtp.models)
                std::cout << "  " << m << "\n";
        }
        if (!mtp.textures.empty())
        {
            std::cout << "\nTexture list:\n";
            for (const auto& t : mtp.textures)
                std::cout << "  " << t << "\n";
        }
        return 0;
    }

    std::cerr << "mtp: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
