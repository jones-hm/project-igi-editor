#include "pch.h"
#include "compiler.h"
#include "utils.h"
#include "config.h"
#include "logger.h"

#include <filesystem>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>

namespace fs = std::filesystem;

Compiler::Compiler() {}

Compiler::~Compiler() {}

void Compiler::SetOutputCallback(std::function<void(const std::string&)> callback) {
    output_callback_ = callback;
}

static bool RunProcess(const std::string& exePath, const std::string& args, const std::string& workingDir, std::string& errLog) {
    std::string cmdLine = "\"" + exePath + "\" " + args;
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    ZeroMemory(&pi, sizeof(pi));
    
    std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back('\0');
    
    BOOL success = CreateProcessA(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        workingDir.empty() ? NULL : workingDir.c_str(),
        &si,
        &pi
    );
    
    if (!success) {
        errLog = "CreateProcess failed with code " + std::to_string(GetLastError()) + " for command: " + cmdLine;
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode != 0) {
        errLog = "Process exited with code " + std::to_string(exitCode) + " for command: " + cmdLine;
        return false;
    }
    
    return true;
}

bool Compiler::Compile(const std::string& qsc_path, const std::string& qvm_output_path) {
    if (output_callback_) {
        output_callback_("[Compiler] Compiling QSC: " + qsc_path + " -> " + qvm_output_path);
    }
    
    // Resolve dynamic path from Config
    std::string compilerPath = Config::Get().compilerPath;
    if (compilerPath.empty()) {
        char appData[1024];
        GetEnvironmentVariableA("APPDATA", appData, 1024);
        compilerPath = std::string(appData) + "\\QEditor\\QCompiler";
    }
    
    fs::path compilerDir(compilerPath);
    fs::path gconvDir = compilerDir / "Tools" / "GConv";
    fs::path dconvDir = compilerDir / "Tools" / "DConv";
    
    fs::path gconvExe = gconvDir / "gconv.exe";
    fs::path dconvExe = dconvDir / "dconv.exe";
    
    if (!fs::exists(gconvExe)) {
        std::string err = "[Compiler] ERROR: GConv executable not found at: " + gconvExe.string();
        if (output_callback_) output_callback_(err);
        return false;
    }
    if (!fs::exists(dconvExe)) {
        std::string err = "[Compiler] ERROR: DConv executable not found at: " + dconvExe.string();
        if (output_callback_) output_callback_(err);
        return false;
    }
    
    fs::path gconvInput = gconvDir / "input";
    fs::path dconvOutput = dconvDir / "output";
    fs::path dconvTempOut = dconvDir / "temp_out";
    
    try {
        fs::create_directories(gconvInput);
        fs::create_directories(dconvOutput);
        fs::create_directories(dconvTempOut);
        
        // Clean any existing files in temporary dirs to avoid collisions
        for (const auto& entry : fs::directory_iterator(gconvInput)) {
            fs::remove_all(entry.path());
        }
        for (const auto& entry : fs::directory_iterator(dconvOutput)) {
            fs::remove_all(entry.path());
        }
        for (const auto& entry : fs::directory_iterator(dconvTempOut)) {
            fs::remove_all(entry.path());
        }
        
        // Copy the QSC file to Tools/GConv/input
        std::string filename = fs::path(qsc_path).filename().string();
        std::string stem = fs::path(qsc_path).stem().string();
        fs::path gconvQscFile = gconvInput / filename;
        fs::copy_file(qsc_path, gconvQscFile, fs::copy_options::overwrite_existing);
        
        // Step 1: Run GConv to compile QSC
        std::string errLog;
        if (!RunProcess(gconvExe.string(), "compile_scripts.qsc", gconvDir.string(), errLog)) {
            if (output_callback_) output_callback_("[Compiler] GConv Error: " + errLog);
            return false;
        }
        
        // GConv outputs compiled .qvm inside the input folder
        fs::path compiledQvm = gconvInput / (stem + ".qvm");
        if (!fs::exists(compiledQvm)) {
            if (output_callback_) output_callback_("[Compiler] ERROR: GConv did not produce: " + compiledQvm.string());
            return false;
        }
        
        // Step 2: Move compiled .qvm to Tools/DConv/output
        fs::path dconvQvm = dconvOutput / (stem + ".qvm");
        fs::copy_file(compiledQvm, dconvQvm, fs::copy_options::overwrite_existing);
        
        // Cleanup intermediate .qvm and .qsc in GConv
        fs::remove(compiledQvm);
        fs::remove(gconvQscFile);
        
        // Step 3: Run DConv to convert to QVM Version 5 format
        if (!RunProcess(dconvExe.string(), "qvm convert output temp_out", dconvDir.string(), errLog)) {
            if (output_callback_) output_callback_("[Compiler] DConv Error: " + errLog);
            return false;
        }
        
        // DConv outputs converted QVM as a .qsc file in temp_out
        fs::path finalConvertedQsc = dconvTempOut / (stem + ".qsc");
        if (!fs::exists(finalConvertedQsc)) {
            if (output_callback_) output_callback_("[Compiler] ERROR: DConv did not produce converted file: " + finalConvertedQsc.string());
            return false;
        }
        
        // Copy final converted file to our desired output path (renaming back to .qvm)
        fs::create_directories(fs::path(qvm_output_path).parent_path());
        fs::copy_file(finalConvertedQsc, qvm_output_path, fs::copy_options::overwrite_existing);
        
        // Final cleanup
        fs::remove(dconvQvm);
        fs::remove(finalConvertedQsc);
        fs::remove_all(dconvTempOut);
        
        if (output_callback_) output_callback_("[Compiler] SUCCESS: Compiled QVM written to: " + qvm_output_path);
        return true;
        
    } catch (const std::exception& ex) {
        if (output_callback_) output_callback_("[Compiler] Exception: " + std::string(ex.what()));
        return false;
    }
}
