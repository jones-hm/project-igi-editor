#include "pch.h"
#include "cmd_tex.h"
#include "cmd_mef.h"
#include "cmd_qsc.h"
#include "cmd_qvm.h"
#include "cmd_res.h"
#include "cmd_mtp.h"
#include "cmd_terrain.h"
#include "cmd_graph.h"

// Exit codes:
//   0 = success
//   1 = bad args
//   2 = file not found
//   3 = parse error
//   4 = write error

static void print_help()
{
    std::cout <<
        "gconv v1.0 \xe2\x80\x94 IGI Game Asset Converter\n"
        "\n"
        "Usage: gconv <command> [options]\n"
        "\n"
        "Commands:\n"
        "  tex      TEX/SPR/PIC texture operations (decode, info)\n"
        "  mef      MEF 3D mesh operations (export to OBJ, dump, info)\n"
        "  qsc      QSC quest script (compile to QVM, validate)\n"
        "  qvm      QVM bytecode (decompile to QSC, disasm, info)\n"
        "  res      RES archive (list, extract)\n"
        "  mtp      MTP terrain properties (dump to JSON, info)\n"
        "  terrain  Terrain height/cube data (export-lmp, export-ctr, info)\n"
        "  graph    AI navigation graph (export to JSON, info)\n"
        "\n"
        "Run 'gconv <command> --help' for command-specific help.\n";
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        print_help();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "--help" || cmd == "-h")
    {
        print_help();
        return 0;
    }

    if (cmd == "--version" || cmd == "-v")
    {
        std::cout << "gconv version 1.0\n";
        return 0;
    }

    // Shift argv so each handler receives its own argc/argv starting at argv[0] = command name
    int sub_argc = argc - 1;
    char** sub_argv = argv + 1;

    if (cmd == "tex")     return cmd_tex(sub_argc, sub_argv);
    if (cmd == "mef")     return cmd_mef(sub_argc, sub_argv);
    if (cmd == "qsc")     return cmd_qsc(sub_argc, sub_argv);
    if (cmd == "qvm")     return cmd_qvm(sub_argc, sub_argv);
    if (cmd == "res")     return cmd_res(sub_argc, sub_argv);
    if (cmd == "mtp")     return cmd_mtp(sub_argc, sub_argv);
    if (cmd == "terrain") return cmd_terrain(sub_argc, sub_argv);
    if (cmd == "graph")   return cmd_graph(sub_argc, sub_argv);

    std::cerr << "gconv: unknown command '" << cmd << "'\n";
    std::cerr << "Run 'gconv --help' for usage.\n";
    return 1;
}
