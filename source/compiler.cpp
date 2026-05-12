#include "pch.h"
#include "compiler.h"
#include <filesystem>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <fstream>

Compiler::Compiler() {}

Compiler::~Compiler() {}

void Compiler::SetOutputCallback(std::function<void(const std::string&)> callback) {
    output_callback_ = callback;
}

bool Compiler::Compile(const std::string& qsc_path, const std::string& qvm_output_path) {
    namespace fs = std::filesystem;

    if (output_callback_) output_callback_("[Compiler] === Compile started ===");
    if (output_callback_) output_callback_("[Compiler] Input QSC: " + qsc_path);
    if (output_callback_) output_callback_("[Compiler] Output QVM: " + qvm_output_path);

    // Read QCompiler path from config
    char appDataPath[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", appDataPath, MAX_PATH);
    std::string qcompiler_path = std::string(appDataPath) + "\\QEditor\\QCompiler";
    if (output_callback_) output_callback_("[Compiler] QCompiler path: " + qcompiler_path);

    // Paths
    std::string compile_input_dir = qcompiler_path + "\\Compile\\input";
    std::string compile_output_dir = qcompiler_path + "\\Compile\\output";
    std::string compile_bat = qcompiler_path + "\\Compile\\compile_v5.bat";

    // Ensure directories exist
    if (!fs::exists(compile_input_dir)) {
        if (output_callback_) output_callback_("[Compiler] ERROR: Compile input directory not found: " + compile_input_dir);
        return false;
    }
    if (output_callback_) output_callback_("[Compiler] Input dir exists: " + compile_input_dir);

    if (!fs::exists(compile_output_dir)) {
        if (output_callback_) output_callback_("[Compiler] ERROR: Compile output directory not found: " + compile_output_dir);
        return false;
    }
    if (output_callback_) output_callback_("[Compiler] Output dir exists: " + compile_output_dir);

    if (!fs::exists(compile_bat)) {
        if (output_callback_) output_callback_("[Compiler] ERROR: compile_v5.bat not found: " + compile_bat);
        return false;
    }
    if (output_callback_) output_callback_("[Compiler] Batch file exists: " + compile_bat);

    // Copy qsc file to compile input directory
    std::string qsc_filename = fs::path(qsc_path).filename().string();
    std::string input_qsc = compile_input_dir + "\\" + qsc_filename;

    try {
        if (fs::exists(input_qsc)) fs::remove(input_qsc);
        fs::copy_file(qsc_path, input_qsc, fs::copy_options::overwrite_existing);
        if (output_callback_) output_callback_("[Compiler] Copied QSC to input: " + input_qsc);
    } catch (const std::exception& e) {
        if (output_callback_) output_callback_("[Compiler] ERROR: Failed to copy qsc file: " + std::string(e.what()));
        return false;
    }

    // Run compile_v5.bat using system() for reliable Windows batch execution
    std::string cmd = "cd /d \"" + qcompiler_path + "\\Compile\\" + "\" && " + "compile_v5.bat" + " > \"" + compile_output_dir + "\\compile.log\" 2>&1";
    if (output_callback_) output_callback_("[Compiler] Executing command: " + cmd);

    int result = system(cmd.c_str());
    if (output_callback_) output_callback_("[Compiler] Batch exit code: " + std::to_string(result));

    // Read the log file for output
    std::string log_file = compile_output_dir + "\\compile.log";
    std::ifstream log_stream(log_file);
    std::string output;
    if (log_stream.is_open()) {
        std::string line;
        while (std::getline(log_stream, line)) {
            output += line + "\n";
        }
        log_stream.close();
        if (output_callback_) output_callback_("[Compiler] Read compile.log successfully");
    } else {
        if (output_callback_) output_callback_("[Compiler] WARNING: Could not read compile.log at: " + log_file);
    }

    if (output_callback_) output_callback_("[Compiler] --- Batch output ---");
    if (output_callback_) output_callback_(output);
    if (output_callback_) output_callback_("[Compiler] --- End output ---");

    // Check for success indicators in output
    bool hasSuccess = (output.find("success") != std::string::npos || output.find("SUCCESS") != std::string::npos);
    bool hasCompiled = (output.find("compiled") != std::string::npos || output.find("COMPILED") != std::string::npos);
    bool hasError = (output.find("error") != std::string::npos || output.find("ERROR") != std::string::npos);
    bool success = hasSuccess || hasCompiled || !hasError;

    if (output_callback_) output_callback_("[Compiler] Success keyword: " + std::string(hasSuccess ? "yes" : "no"));
    if (output_callback_) output_callback_("[Compiler] Compiled keyword: " + std::string(hasCompiled ? "yes" : "no"));
    if (output_callback_) output_callback_("[Compiler] Error keyword: " + std::string(hasError ? "yes" : "no"));
    if (output_callback_) output_callback_("[Compiler] Overall success: " + std::string(success ? "yes" : "no"));

    if (!success) {
        if (output_callback_) output_callback_("[Compiler] ERROR: Compilation failed based on output check");
        return false;
    }

    // Find the output QVM file
    std::string qvm_filename = qsc_filename;
    size_t dot = qvm_filename.find_last_of('.');
    if (dot != std::string::npos) qvm_filename = qvm_filename.substr(0, dot) + ".qvm";

    std::string output_qvm = compile_output_dir + "\\" + qvm_filename;
    if (output_callback_) output_callback_("[Compiler] Looking for output QVM: " + output_qvm);

    if (!fs::exists(output_qvm)) {
        if (output_callback_) output_callback_("[Compiler] ERROR: Output QVM not found: " + output_qvm);
        return false;
    }
    if (output_callback_) output_callback_("[Compiler] Output QVM exists: " + output_qvm);

    // Copy QVM to destination
    try {
        fs::create_directories(fs::path(qvm_output_path).parent_path());
        if (output_callback_) output_callback_("[Compiler] Created destination dirs for: " + qvm_output_path);
        if (fs::exists(qvm_output_path)) fs::remove(qvm_output_path);
        fs::copy_file(output_qvm, qvm_output_path, fs::copy_options::overwrite_existing);
        if (output_callback_) output_callback_("[Compiler] SUCCESS: QVM copied to game path: " + qvm_output_path);
        return true;
    } catch (const std::exception& e) {
        if (output_callback_) output_callback_("[Compiler] ERROR: Failed to copy QVM: " + std::string(e.what()));
        return false;
    }
}
