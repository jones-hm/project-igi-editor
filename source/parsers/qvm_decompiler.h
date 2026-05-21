#pragma once
#include "qvm_parser.h"
#include <string>

// Decompile a parsed QVM v0.5 file to QSC source text.
// Returns false if outpath cannot be opened or qvm is invalid.
bool QVM_Decompile(const QVMFile& qvm, const std::string& outpath);
