#pragma once
#include <string>
#include "mef_native.h"

namespace MefExporter {
    // Export binary ParsedGeometry to OBJ format
    bool ExportToObj(const ParsedGeometry& geometry, const std::string& outpath);
    
    // Export binary ParsedGeometry to text-based MEF format (parsed by MEFParser)
    bool ExportToMefAscii(const ParsedGeometry& geometry, const std::string& outpath);
}
