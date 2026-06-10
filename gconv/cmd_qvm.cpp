#include "pch.h"
#include "cmd_qvm.h"
#include "qvm_parser.h"
#include "qvm_decompiler.h"

static void print_help_qvm()
{
    std::cout <<
        "Usage: gconv qvm <subcommand> [options]\n"
        "\n"
        "Subcommands:\n"
        "  decompile <input.qvm> -o <output.qsc>   Decompile QVM bytecode to QSC source\n"
        "  disasm    <input.qvm> [-o <output.txt>]  Disassemble QVM to opcode listing\n"
        "  info      <input.qvm>                    Print QVM header information\n"
        "\n"
        "Options:\n"
        "  -o <file>   Output file path (stdout if omitted for disasm)\n"
        "  --help      Show this help\n";
}

static int do_decompile(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "gconv qvm decompile: missing input file\n";
        std::cerr << "Usage: gconv qvm decompile <input.qvm> -o <output.qsc>\n";
        return 1;
    }

    std::string input = argv[1];

    std::string output;
    for (int i = 2; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-o") {
            output = argv[i + 1];
            break;
        }
    }

    if (output.empty()) {
        std::cerr << "gconv qvm decompile: missing -o <output.qsc>\n";
        return 1;
    }

    QVMFile qvm = QVM_Parse(input);
    if (!qvm.valid) {
        std::cerr << "gconv qvm decompile: failed to parse '" << input << "'";
        if (!qvm.error.empty()) std::cerr << ": " << qvm.error;
        std::cerr << "\n";
        return (qvm.error.find("not found") != std::string::npos ||
                qvm.error.find("open") != std::string::npos) ? 2 : 3;
    }

    if (!QVM_Decompile(qvm, output)) {
        std::cerr << "gconv qvm decompile: failed to write '" << output << "'\n";
        return 4;
    }

    std::cout << "gconv qvm: decompiled '" << input << "' -> '" << output << "'\n";
    return 0;
}

static int do_disasm(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "gconv qvm disasm: missing input file\n";
        std::cerr << "Usage: gconv qvm disasm <input.qvm> [-o <output.txt>]\n";
        return 1;
    }

    std::string input = argv[1];

    std::string output;
    for (int i = 2; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-o") {
            output = argv[i + 1];
            break;
        }
    }

    QVMFile qvm = QVM_Parse(input);
    if (!qvm.valid) {
        std::cerr << "gconv qvm disasm: failed to parse '" << input << "'";
        if (!qvm.error.empty()) std::cerr << ": " << qvm.error;
        std::cerr << "\n";
        return (qvm.error.find("not found") != std::string::npos ||
                qvm.error.find("open") != std::string::npos) ? 2 : 3;
    }

    // Build disassembly text
    std::ostringstream oss;
    char buf[128];
    for (const auto& instr : qvm.instructions) {
        // Base: <offset> <opcode>
        const char* opname = QVM_OpName(instr.type);

        switch (instr.type) {
        case QVMOpType::PUSHF:
            snprintf(buf, sizeof(buf), "%08X  %-10s %g\n",
                     instr.address, opname, instr.operand_float);
            break;
        case QVMOpType::CALL:
            snprintf(buf, sizeof(buf), "%08X  %-10s", instr.address, opname);
            oss << buf;
            for (size_t k = 0; k < instr.call_targets.size(); ++k) {
                if (k) oss << ", ";
                else   oss << " ";
                oss << instr.call_targets[k];
            }
            oss << "\n";
            continue; // already written
        case QVMOpType::PUSH:
        case QVMOpType::PUSHB:
        case QVMOpType::PUSHW:
        case QVMOpType::PUSHA:
        case QVMOpType::PUSHS:
        case QVMOpType::PUSHSI:
        case QVMOpType::PUSHSIB:
        case QVMOpType::PUSHSIW:
        case QVMOpType::PUSHI:
        case QVMOpType::PUSHII:
        case QVMOpType::PUSHIIB:
        case QVMOpType::PUSHIIW:
        case QVMOpType::BRA:
        case QVMOpType::BF:
        case QVMOpType::BT:
        case QVMOpType::JSR:
            snprintf(buf, sizeof(buf), "%08X  %-10s %u\n",
                     instr.address, opname, instr.operand);
            break;
        default:
            snprintf(buf, sizeof(buf), "%08X  %s\n",
                     instr.address, opname);
            break;
        }
        oss << buf;
    }

    std::string text = oss.str();

    if (output.empty()) {
        std::cout << text;
    } else {
        std::ofstream f(output, std::ios::binary);
        if (!f.is_open()) {
            std::cerr << "gconv qvm disasm: cannot write '" << output << "'\n";
            return 4;
        }
        f << text;
        if (!f) {
            std::cerr << "gconv qvm disasm: write error on '" << output << "'\n";
            return 4;
        }
        std::cout << "gconv qvm: disasm '" << input << "' -> '" << output << "'\n";
    }

    return 0;
}

static int do_info(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "gconv qvm info: missing input file\n";
        std::cerr << "Usage: gconv qvm info <input.qvm>\n";
        return 1;
    }

    std::string input = argv[1];

    QVMFile qvm = QVM_Parse(input);
    if (!qvm.valid) {
        std::cerr << "gconv qvm info: failed to parse '" << input << "'";
        if (!qvm.error.empty()) std::cerr << ": " << qvm.error;
        std::cerr << "\n";
        return (qvm.error.find("not found") != std::string::npos ||
                qvm.error.find("open") != std::string::npos) ? 2 : 3;
    }

    const QVMHeader& h = qvm.header;
    std::cout << "File            : " << input << "\n"
              << "Signature       : " << h.signature[0] << h.signature[1]
                                      << h.signature[2] << h.signature[3] << "\n"
              << "Version         : " << h.ver_major << "." << h.ver_minor << "\n"
              << "Instructions    : " << qvm.totalInstructions() << "\n"
              << "Identifiers     : " << qvm.identifierCount() << "\n"
              << "Strings         : " << qvm.stringCount() << "\n"
              << "itable offset   : 0x" << std::hex << h.of_itable
              << "  size: " << std::dec << h.sz_itable << " bytes\n"
              << "ivalue offset   : 0x" << std::hex << h.of_ivalue
              << "  size: " << std::dec << h.sz_ivalue << " bytes\n"
              << "stable offset   : 0x" << std::hex << h.of_stable
              << "  size: " << std::dec << h.sz_stable << " bytes\n"
              << "svalue offset   : 0x" << std::hex << h.of_svalue
              << "  size: " << std::dec << h.sz_svalue << " bytes\n"
              << "ctable offset   : 0x" << std::hex << h.of_ctable
              << "  size: " << std::dec << h.sz_ctable << " bytes\n";

    return 0;
}

int cmd_qvm(int argc, char** argv)
{
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        print_help_qvm();
        return (argc < 2) ? 1 : 0;
    }

    std::string sub = argv[1];
    int sub_argc = argc - 1;
    char** sub_argv = argv + 1;

    if (sub == "decompile") return do_decompile(sub_argc, sub_argv);
    if (sub == "disasm")    return do_disasm(sub_argc, sub_argv);
    if (sub == "info")      return do_info(sub_argc, sub_argv);

    std::cerr << "gconv qvm: unknown subcommand '" << sub << "'\n";
    std::cerr << "Run 'gconv qvm --help' for usage.\n";
    return 1;
}
