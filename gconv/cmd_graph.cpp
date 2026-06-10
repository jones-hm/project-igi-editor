#include "pch.h"
#include "cmd_graph.h"
#include "graph_parser.h"

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  gconv graph export <input.dat> -o <output.json>\n"
        "  gconv graph info <input.dat>\n";
}

// Return value of named option, or nullptr.
static const char* opt_val(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc - 1; ++i)
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return nullptr;
}

// Minimal JSON string escape
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

static void write_graph_json(std::ostream& os, const GraphFile& gf)
{
    if (!gf.valid || (gf.nodes.empty() && gf.edges.empty()))
    {
        os << "{\"nodes\":[],\"edges\":[]}\n";
        return;
    }

    os << "{\n";
    os << "  \"nodes\": [\n";
    for (size_t i = 0; i < gf.nodes.size(); ++i)
    {
        const GraphNode& n = gf.nodes[i];
        if (i) os << ",\n";
        os << "    {"
           << "\"id\": " << n.id
           << ", \"x\": " << n.x
           << ", \"y\": " << n.y
           << ", \"z\": " << n.z
           << ", \"criteria\": " << json_str(n.criteria)
           << "}";
    }
    os << "\n  ],\n";

    os << "  \"edges\": [\n";
    for (size_t i = 0; i < gf.edges.size(); ++i)
    {
        const GraphEdge& e = gf.edges[i];
        if (i) os << ",\n";
        os << "    {\"from\": " << e.node1 << ", \"to\": " << e.node2 << "}";
    }
    os << "\n  ]\n";
    os << "}\n";
}

int cmd_graph(int argc, char** argv)
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];

    // ── export ────────────────────────────────────────────────────────────────
    if (sub == "export")
    {
        if (argc < 3)
        {
            std::cerr << "graph export: missing <input.dat>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = opt_val(argc, argv, "-o");

        if (!std::filesystem::exists(path))
        {
            std::cerr << "graph export: file not found: " << path << "\n";
            return 2;
        }

        GraphFile gf = GRAPH_Parse(path);
        if (!gf.valid)
        {
            std::cerr << "graph export: parse error: " << gf.error << "\n";
            return 3;
        }

        if (out_path)
        {
            std::ofstream ofs(out_path);
            if (!ofs)
            {
                std::cerr << "graph export: cannot write: " << out_path << "\n";
                return 4;
            }
            write_graph_json(ofs, gf);
        }
        else
        {
            write_graph_json(std::cout, gf);
        }
        return 0;
    }

    // ── info ──────────────────────────────────────────────────────────────────
    if (sub == "info")
    {
        if (argc < 3)
        {
            std::cerr << "graph info: missing <input.dat>\n";
            return 1;
        }
        std::string path = argv[2];

        if (!std::filesystem::exists(path))
        {
            std::cerr << "graph info: file not found: " << path << "\n";
            return 2;
        }

        GraphFile gf = GRAPH_Parse(path);
        if (!gf.valid)
        {
            std::cerr << "graph info: parse error: " << gf.error << "\n";
            return 3;
        }

        std::cout << "File:       " << path << "\n";
        std::cout << "Max nodes:  " << gf.max_nodes << "\n";
        std::cout << "Nodes:      " << gf.nodes.size() << "\n";
        std::cout << "Edges:      " << gf.edges.size() << "\n";
        return 0;
    }

    std::cerr << "graph: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
