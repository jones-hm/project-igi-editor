#include "qvm_decompiler.h"
#include "../logger.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::string FloatStr(float v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6) << v;
    return ss.str();
}

static std::string SafePop(std::vector<std::string>& stack) {
    if (stack.empty()) return "?";
    std::string top = stack.back();
    stack.pop_back();
    return top;
}

static void EmitLine(std::ofstream& out, int indent, const std::string& line) {
    for (int i = 0; i < indent; ++i) out << "    ";
    out << line << "\n";
}

// Return a safe identifier string for index idx.
static std::string SafeIdentifier(const QVMFile& qvm, int32_t idx) {
    if (idx >= 0 && static_cast<size_t>(idx) < qvm.identifiers.size())
        return qvm.identifiers[static_cast<size_t>(idx)];
    return "id_" + std::to_string(idx);
}

// Return a safe quoted string for index idx.
static std::string SafeString(const QVMFile& qvm, uint32_t idx) {
    if (static_cast<size_t>(idx) < qvm.strings.size())
        return "\"" + qvm.strings[static_cast<size_t>(idx)] + "\"";
    return "\"\"";
}

// ---------------------------------------------------------------------------
// Decompile
// ---------------------------------------------------------------------------

bool QVM_Decompile(const QVMFile& qvm, const std::string& outpath) {
    if (!qvm.valid) {
        Logger::Get().Log(LogLevel::ERR, "[QVM_Decompile] QVM is not valid: " + qvm.error);
        return false;
    }

    std::ofstream out(outpath);
    if (!out.is_open()) {
        Logger::Get().Log(LogLevel::ERR, "[QVM_Decompile] Cannot open output file: " + outpath);
        return false;
    }

    // -----------------------------------------------------------------------
    // Emit file header comment
    // -----------------------------------------------------------------------
    out << "// Decompiled by IGI Editor (QVM v0.5)\n";
    out << "// Identifiers: " << qvm.identifiers.size()
        << "  Strings: "      << qvm.strings.size()
        << "  Instructions: " << qvm.instructions.size()
        << "\n\n";

    if (qvm.instructions.empty()) {
        Logger::Get().Log(LogLevel::INFO, "[QVM_Decompile] No instructions to decompile.");
        return true;
    }

    // -----------------------------------------------------------------------
    // Build address -> instruction index map for branch-target resolution
    // -----------------------------------------------------------------------
    std::map<uint32_t, size_t> addrToIndex;
    for (size_t i = 0; i < qvm.instructions.size(); ++i) {
        addrToIndex[qvm.instructions[i].address] = i;
    }

    // -----------------------------------------------------------------------
    // Pass 1 — collect pending closures keyed by instruction address.
    //
    // Each entry is a pair<int, string>: the indent delta to apply BEFORE
    // emitting the text (negative = dedent), then the text to emit.
    // We store multiple actions per address in order.
    // -----------------------------------------------------------------------
    struct PendingAction {
        int indentDelta; // applied to 'indent' before emitting
        std::string text;
    };

    std::map<uint32_t, std::vector<PendingAction>> pendingAt;

    // We need a running indent to pre-compute what will be stored.
    // We track it separately here just for Pass-1 annotation; Pass-2
    // maintains the real value.
    // Actually the simplest correct approach: only record brace-close
    // actions at branch targets.  Pass 2 will apply them.

    for (const auto& instr : qvm.instructions) {
        uint32_t cur = instr.address;

        switch (instr.type) {

        case QVMOpType::BF: {
            // BF: branch if false (condition is false → skip body).
            // Emit "if (cond) {" and schedule close at target.
            uint32_t target = instr.operand;
            // Close the if-body when execution reaches target.
            pendingAt[target].push_back({-1, "}"});
            break;
        }

        case QVMOpType::BT: {
            // BT: branch if true — used for while loops.
            // The while body ends before this instruction and jumps back;
            // the BT at the top of the loop jumps past the body when false.
            // Schedule close at target (past body).
            uint32_t target = instr.operand;
            pendingAt[target].push_back({-1, "}"});
            break;
        }

        case QVMOpType::BRA: {
            uint32_t target = instr.operand;
            if (target > cur) {
                // Forward BRA: marks the end of an if-body (or else-body).
                //
                // The preceding BF placed {-1, "}"} at `fallthrough`
                // (the address of the instruction right after this BRA),
                // which is the start of the else-clause or the post-if code.
                //
                // Case A — plain if (no else):
                //   BF already covers the close; BRA just skips forward.
                //   Nothing extra to do here.
                //
                // Case B — if/else:
                //   The BF target is this BRA's fall-through address.
                //   We upgrade the BF-scheduled {-1,"}"} at fallthrough to
                //   {-1,"} else {"} (dedent, emit "} else {", re-indent),
                //   and add {-1,"}"} at the BRA target to close the else body.
                //
                // Distinguish the two cases: if there is already a pending
                // entry at `fallthrough` that was placed by a BF (text == "}"),
                // then this is an else branch.

                uint32_t fallthrough = cur + instr.size;
                auto& vec = pendingAt[fallthrough];

                bool upgradedToElse = false;
                for (auto& a : vec) {
                    if (a.indentDelta == -1 && a.text == "}") {
                        // Upgrade: dedent, emit "} else {", re-indent.
                        // We encode this as a single action with delta -1 and
                        // special text "} else {".  The Pass-2 emitter will
                        // re-indent after detecting the trailing '{'.
                        a.text = "} else {";
                        upgradedToElse = true;
                        break;
                    }
                }

                if (upgradedToElse) {
                    // Close the else-body at the BRA jump target.
                    pendingAt[target].push_back({-1, "}"});
                }
                // If no upgrade happened this is a lone forward BRA (e.g. an
                // unconditional jump with no preceding BF), which we leave as-is.
            }
            // Backward BRA: closing a while loop body.  The BT at the loop
            // head already scheduled {-1,"}"} past the body via its own target.
            // Nothing extra needed here; Pass 2 handles it via BRA opcode below.
            break;
        }

        default:
            break;
        }
    }

    // -----------------------------------------------------------------------
    // Pass 2 — linear walk: process instructions and emit QSC
    // -----------------------------------------------------------------------
    std::vector<std::string> stack; // expression stack
    int indent = 0;

    // Flush any call-expression results sitting on the stack as standalone
    // statements.  Call results end with ')'; anything else is a literal or
    // partial expression that we silently discard (it will be rebuilt).
    auto FlushCalls = [&]() {
        while (!stack.empty() && !stack.back().empty() &&
               stack.back().back() == ')') {
            EmitLine(out, indent, stack.back() + ";");
            stack.pop_back();
        }
    };

    for (size_t i = 0; i < qvm.instructions.size(); ++i) {
        const QVMInstruction& instr = qvm.instructions[i];
        uint32_t cur = instr.address;

        // Apply any pending actions scheduled for this address.
        auto it = pendingAt.find(cur);
        if (it != pendingAt.end()) {
            // Before closing a block, flush any orphaned call results.
            FlushCalls();
            for (auto& action : it->second) {
                // Apply the indent delta first (e.g. -1 to dedent before "}" or
                // "} else {").
                indent += action.indentDelta;
                if (indent < 0) indent = 0;

                if (!action.text.empty()) {
                    EmitLine(out, indent, action.text);
                    // If the action opens a new block (text ends with '{'),
                    // increment indent for the body that follows.
                    if (action.text.back() == '{') {
                        indent++;
                    }
                }
            }
        }

        // ------------------------------------------------------------------
        // Process the instruction
        // ------------------------------------------------------------------
        switch (instr.type) {

        // --- No-ops / meta ---
        case QVMOpType::BRK:
            FlushCalls();
            EmitLine(out, indent, "// BRK");
            break;

        case QVMOpType::NOP:
            break;

        case QVMOpType::BLK:
            out << "\n";
            break;

        case QVMOpType::JSR:
            // Internal subroutine linkage — skip.
            break;

        case QVMOpType::ILLEGAL:
            EmitLine(out, indent, "// ILLEGAL");
            break;

        // --- Push immediates ---
        case QVMOpType::PUSH:
            stack.push_back(std::to_string(instr.operand));
            break;

        case QVMOpType::PUSHB:
            stack.push_back(std::to_string(instr.operand));
            break;

        case QVMOpType::PUSHW:
            stack.push_back(std::to_string(instr.operand));
            break;

        case QVMOpType::PUSHF:
            stack.push_back(FloatStr(instr.operand_float));
            break;

        case QVMOpType::PUSH0:
            stack.push_back("0");
            break;

        case QVMOpType::PUSH1:
            stack.push_back("1");
            break;

        case QVMOpType::PUSHM:
            stack.push_back("-1");
            break;

        case QVMOpType::PUSHA:
            stack.push_back("@" + std::to_string(instr.operand));
            break;

        // --- Push string ---
        case QVMOpType::PUSHS:
        case QVMOpType::PUSHSI:
        case QVMOpType::PUSHSIB:
        case QVMOpType::PUSHSIW:
            stack.push_back(SafeString(qvm, instr.operand));
            break;

        // --- Push identifier ---
        case QVMOpType::PUSHI:
        case QVMOpType::PUSHII:
        case QVMOpType::PUSHIIB:
        case QVMOpType::PUSHIIW:
            stack.push_back(SafeIdentifier(qvm, static_cast<int32_t>(instr.operand)));
            break;

        // --- Binary arithmetic / logical / relational ---
        case QVMOpType::ADD: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " + " + b + ")");
            break;
        }
        case QVMOpType::SUB: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " - " + b + ")");
            break;
        }
        case QVMOpType::MUL: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " * " + b + ")");
            break;
        }
        case QVMOpType::DIV: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " / " + b + ")");
            break;
        }
        case QVMOpType::SHL: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " << " + b + ")");
            break;
        }
        case QVMOpType::SHR: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " >> " + b + ")");
            break;
        }
        case QVMOpType::AND: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " & " + b + ")");
            break;
        }
        case QVMOpType::OR: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " | " + b + ")");
            break;
        }
        case QVMOpType::XOR: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " ^ " + b + ")");
            break;
        }
        case QVMOpType::LAND: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " && " + b + ")");
            break;
        }
        case QVMOpType::LOR: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " || " + b + ")");
            break;
        }
        case QVMOpType::EQ: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " == " + b + ")");
            break;
        }
        case QVMOpType::NE: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " != " + b + ")");
            break;
        }
        case QVMOpType::LT: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " < " + b + ")");
            break;
        }
        case QVMOpType::LE: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " <= " + b + ")");
            break;
        }
        case QVMOpType::GT: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " > " + b + ")");
            break;
        }
        case QVMOpType::GE: {
            std::string b = SafePop(stack), a = SafePop(stack);
            stack.push_back("(" + a + " >= " + b + ")");
            break;
        }

        // --- Unary ---
        case QVMOpType::PLUS: {
            std::string a = SafePop(stack);
            stack.push_back("+" + a);
            break;
        }
        case QVMOpType::MINUS: {
            std::string a = SafePop(stack);
            stack.push_back("-" + a);
            break;
        }
        case QVMOpType::INV: {
            std::string a = SafePop(stack);
            stack.push_back("~" + a);
            break;
        }
        case QVMOpType::NOT: {
            std::string a = SafePop(stack);
            stack.push_back("!" + a);
            break;
        }

        // --- Assignment ---
        case QVMOpType::ASSIGN: {
            // Stack: [..., lhs, rhs]  — rhs on top
            std::string rhs = SafePop(stack);
            std::string lhs = SafePop(stack);
            EmitLine(out, indent, lhs + " = " + rhs + ";");
            break;
        }

        // --- POP: flush call result as a statement or silently discard ---
        case QVMOpType::POP: {
            if (!stack.empty()) {
                std::string expr = SafePop(stack);
                if (!expr.empty() && expr.back() == ')') {
                    EmitLine(out, indent, expr + ";");
                }
            }
            break;
        }

        // --- Return ---
        case QVMOpType::RET: {
            FlushCalls();
            if (!stack.empty()) {
                std::string val = SafePop(stack);
                EmitLine(out, indent, "return " + val + ";");
            } else {
                EmitLine(out, indent, "return;");
            }
            break;
        }

        // --- Function call ---
        case QVMOpType::CALL: {
            // QVM v0.5 calling convention:
            //   Stack layout (bottom → top): arg0, arg1, …, argN-1, funcName
            //   funcName is pushed LAST via PUSHI (at stack top).
            //   call_targets = dispatch selector IDs (NOT identifier indices).
            //   numArgs = call_targets.size() - 1.
            //   totalPop = numArgs + 1 (args + funcName slot).

            size_t numArgs = instr.call_targets.size() > 1
                ? instr.call_targets.size() - 1 : 0;
            size_t totalPop = 1 + numArgs;
            size_t safePop  = std::min(totalPop, stack.size());
            size_t base     = stack.size() - safePop;

            // funcName is the topmost item (pushed last).
            std::string funcName = (safePop >= 1)
                ? stack[base + safePop - 1] : "func_unknown";

            // args are the items below funcName in push order.
            std::vector<std::string> args;
            args.reserve(safePop > 1 ? safePop - 1 : 0);
            for (size_t k = 0; k + 1 < safePop; ++k)
                args.push_back(stack[base + k]);
            stack.resize(base);

            std::string callExpr = funcName + "(";
            for (size_t k = 0; k < args.size(); ++k) {
                if (k > 0) callExpr += ", ";
                callExpr += args[k];
            }
            callExpr += ")";

            // Push result — consumed by a comparison/binary op that follows,
            // or flushed as a statement at the next BRK/BRA/RET/block-close.
            stack.push_back(callExpr);
            break;
        }

        // --- Control flow ---

        case QVMOpType::BF: {
            // Branch if FALSE: skip body when condition is false.
            // → emit  if (cond) {
            std::string cond = SafePop(stack);
            EmitLine(out, indent, "if (" + cond + ") {");
            indent++;
            break;
        }

        case QVMOpType::BT: {
            // Branch if TRUE: used for while — branch past body when false.
            // Actually in QVM v0.5 BT branches when true; a while loop is
            // typically:
            //   evaluate condition
            //   BF  → past body
            //   body
            //   BRA → back to condition
            // BT on its own signals "while (cond)".
            std::string cond = SafePop(stack);
            EmitLine(out, indent, "while (" + cond + ") {");
            indent++;
            break;
        }

        case QVMOpType::BRA: {
            FlushCalls();
            uint32_t target = instr.operand;
            if (target <= cur) {
                indent--;
                if (indent < 0) indent = 0;
                EmitLine(out, indent, "}");
            }
            break;
        }

        default:
            break;
        }
    }

    // Flush any remaining unclosed braces (safety net for malformed bytecode).
    while (indent > 0) {
        indent--;
        EmitLine(out, indent, "}");
    }

    out.flush();
    Logger::Get().Log(LogLevel::INFO,
        "[QVM_Decompile] Written " + std::to_string(qvm.instructions.size()) +
        " instructions to " + outpath);
    return true;
}
