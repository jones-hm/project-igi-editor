#include "pch.h"
#include "cmd_qsc.h"
#include "qsc_lexer.h"
#include "qsc_parser.h"
#include "qvm_compiler.h"

static void print_help_qsc()
{
    std::cout <<
        "Usage: gconv qsc <subcommand> [options]\n"
        "\n"
        "Subcommands:\n"
        "  compile <input.qsc> -o <output.qvm>   Compile QSC script to QVM bytecode\n"
        "  validate <input.qsc>                   Parse and validate QSC script\n"
        "\n"
        "Options:\n"
        "  -o <file>   Output file path\n"
        "  --help      Show this help\n";
}

// Read entire file to string; returns false and prints to stderr on failure.
static bool read_file(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "gconv qsc: cannot open '" << path << "'\n";
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// Lex + parse a QSC source string; returns 0 on success, 3 on error.
static int parse_qsc(const std::string& source, const std::string& path, qsc::ParseResult& result)
{
    qsc::LexResult lex = qsc::Lex(source);
    if (!lex.ok) {
        std::cerr << "gconv qsc: lex error in '" << path << "': " << lex.error << "\n";
        return 3;
    }

    result = qsc::Parse(lex.tokens);
    if (!result.ok) {
        std::cerr << "gconv qsc: parse error in '" << path << "': " << result.error << "\n";
        return 3;
    }

    return 0;
}

static int do_compile(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "gconv qsc compile: missing input file\n";
        std::cerr << "Usage: gconv qsc compile <input.qsc> -o <output.qvm>\n";
        return 1;
    }

    std::string input = argv[1];

    // Find -o
    std::string output;
    for (int i = 2; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-o") {
            output = argv[i + 1];
            break;
        }
    }

    if (output.empty()) {
        std::cerr << "gconv qsc compile: missing -o <output.qvm>\n";
        return 1;
    }

    // Check input exists
    std::string source;
    if (!read_file(input, source)) {
        return 2;
    }

    // Parse
    qsc::ParseResult parsed;
    int rc = parse_qsc(source, input, parsed);
    if (rc != 0) return rc;

    // Compile
    std::string compile_error;
    if (!qvm::CompileToFile(*parsed.program, output, &compile_error)) {
        if (!compile_error.empty())
            std::cerr << "gconv qsc compile: " << compile_error << "\n";
        else
            std::cerr << "gconv qsc compile: failed to write '" << output << "'\n";
        return 4;
    }

    std::cout << "gconv qsc: compiled '" << input << "' -> '" << output << "'\n";
    return 0;
}

static int do_validate(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "gconv qsc validate: missing input file\n";
        std::cerr << "Usage: gconv qsc validate <input.qsc>\n";
        return 1;
    }

    std::string input = argv[1];

    std::string source;
    if (!read_file(input, source)) {
        return 2;
    }

    qsc::ParseResult parsed;
    int rc = parse_qsc(source, input, parsed);
    if (rc != 0) return rc;

    std::cout << "gconv qsc: '" << input << "' is valid"
              << " (" << parsed.call_count << " calls, " << parsed.arg_count << " args)\n";
    return 0;
}

int cmd_qsc(int argc, char** argv)
{
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        print_help_qsc();
        return (argc < 2) ? 1 : 0;
    }

    std::string sub = argv[1];
    int sub_argc = argc - 1;
    char** sub_argv = argv + 1;

    if (sub == "compile")  return do_compile(sub_argc, sub_argv);
    if (sub == "validate") return do_validate(sub_argc, sub_argv);

    std::cerr << "gconv qsc: unknown subcommand '" << sub << "'\n";
    std::cerr << "Run 'gconv qsc --help' for usage.\n";
    return 1;
}
