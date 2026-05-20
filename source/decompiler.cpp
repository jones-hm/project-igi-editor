#include "pch.h"
#include "decompiler.h"
#include "utils.h"
#include "renderer/qvm_parser.h"
#include "renderer/qvm_decompiler.h"
#include <filesystem>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <fstream>

Decompiler::Decompiler() {}

Decompiler::~Decompiler() {}

void Decompiler::SetOutputCallback(std::function<void(const std::string&)> callback) {
    output_callback_ = callback;
}

bool Decompiler::Decompile(const std::string& qvm_path, const std::string& qsc_output_path) {
    if (output_callback_) output_callback_("[Decompiler] Decompiling: " + qvm_path + " -> " + qsc_output_path);
    
    QVMFile qvm = QVM_Parse(qvm_path);
    if (!qvm.valid) {
        if (output_callback_) output_callback_("[Decompiler] ERROR: Failed to parse QVM: " + qvm.error);
        return false;
    }

    if (QVM_Decompile(qvm, qsc_output_path)) {
        if (output_callback_) output_callback_("[Decompiler] SUCCESS: QSC written to: " + qsc_output_path);
        return true;
    } else {
        if (output_callback_) output_callback_("[Decompiler] ERROR: Failed to write QSC");
        return false;
    }
}
