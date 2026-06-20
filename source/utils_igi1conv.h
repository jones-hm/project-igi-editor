/******************************************************************************
 * @file    utils_igi1conv.h
 * @brief   Shared runner for the bundled igi1conv.exe asset converter.
 *          All file conversion in the editor MUST go through this helper
 *          instead of in-process parsers.
 *****************************************************************************/
#pragma once

#include <string>
#include <vector>

namespace igi1conv {

// Path to the bundled converter (editor/tools/igi1conv/igi1conv.exe).
std::string GetExePath();

// Generic runner. Spawns igi1conv.exe with the given argument string and
// waits up to timeoutMs milliseconds. Returns true on exit code 0.
// On failure, `err` is populated with a human-readable reason and (when
// available) the captured stdout/stderr of the tool.
bool Run(const std::string& args, std::string& err, DWORD timeoutMs = 120000);

// ── High-level helpers ──────────────────────────────────────────────────────
// Each one constructs the right `igi1conv <cmd>` invocation and either
// returns the produced path / data, or returns false with `err` set.

struct ExtractResult {
    std::vector<std::string> entry_names;
    std::string out_dir;
    std::string err;
};

// `igi1conv res list <f.res>` — parses the names out of stdout.
std::vector<std::string> ResList(const std::string& resPath, std::string& err);

// `igi1conv res extract <f.res> -o <out_dir>` — extracts every entry.
bool ResExtract(const std::string& resPath, const std::string& outDir, std::string& err);

// `igi1conv res extract <f.res> --file <name> -o <out_dir>`.
bool ResExtractOne(const std::string& resPath, const std::string& entryName,
                   const std::string& outDir, std::string& err);

// `igi1conv res append <src> <files...> -o <dst> [--prefix ...]`.
bool ResAppend(const std::string& srcRes,
               const std::vector<std::string>& newFiles,
               const std::string& dstRes,
               const std::string& prefix,
               std::string& err);

// `igi1conv res pack <dir> <out.res>`.
bool ResPack(const std::string& dir, const std::string& outRes, std::string& err);

// `igi1conv dat export <f.dat> [-o <out.json>] [--text]` — returns the JSON
// path written. If outJson is empty, a temp file is used.
std::string DatExportJson(const std::string& datPath, const std::string& outJson,
                          std::string& err, bool asText = false);

// `igi1conv dat to-mtp <f.dat> -o <out.mtp>`.
bool DatToMtp(const std::string& datPath, const std::string& outMtp, std::string& err);

// `igi1conv mtp dump <f.mtp> -o <out.json>` — returns the JSON path written.
std::string MtpDumpJson(const std::string& mtpPath, const std::string& outJson,
                        std::string& err);

// `igi1conv mtp info <f.mtp>` — returns the captured info text.
std::string MtpInfo(const std::string& mtpPath, std::string& err);

// `igi1conv graph export <f.dat> -o <out.json>`.
std::string GraphExportJson(const std::string& datPath, const std::string& outJson,
                            std::string& err);

// `igi1conv graph info <f.dat>`.
std::string GraphInfo(const std::string& datPath, std::string& err);

// `igi1conv tex decode <f.tex|.spr|.pic> -o <out_dir>` — decodes a texture
// to its raw form (TGA/PNG in the out dir). Useful for cursor/UI use.
bool TexDecode(const std::string& texPath, const std::string& outDir, std::string& err);

// `igi1conv tex to-png <f> [-o <out.png>] [--resize W H]`.
bool TexToPng(const std::string& texPath, const std::string& outPng, std::string& err);

// `igi1conv tex to-tga <f> [-o <out.tga>]`.
bool TexToTga(const std::string& texPath, const std::string& outTga, std::string& err);

// `igi1conv tex info <f>`.
std::string TexInfo(const std::string& texPath, std::string& err);

// `igi1conv qvm decompile <f.qvm> -o <out.qsc>`.
bool QvmDecompile(const std::string& qvmPath, const std::string& outQsc, std::string& err);

// `igi1conv qvm info <f.qvm>`.
std::string QvmInfo(const std::string& qvmPath, std::string& err);

// `igi1conv qvm disasm <f.qvm> [-o <out.txt>]`.
bool QvmDisasm(const std::string& qvmPath, const std::string& outTxt, std::string& err);

// `igi1conv qsc compile <f.qsc> -o <f.qvm>`.
bool QscCompile(const std::string& qscPath, const std::string& outQvm, std::string& err);

// `igi1conv qsc validate <f.qsc>`.
bool QscValidate(const std::string& qscPath, std::string& err);

// `igi1conv fnt export <f.fnt> -o <out.png>`.
bool FntExportPng(const std::string& fntPath, const std::string& outPng, std::string& err);

// `igi1conv fnt info <f.fnt>`.
std::string FntInfo(const std::string& fntPath, std::string& err);

// `igi1conv terrain export-lmp <f.lmp> -o <out.pgm>`.
bool TerrainExportLmp(const std::string& lmpPath, const std::string& outPgm, std::string& err);

// `igi1conv terrain export-ctr <f.ctr> -o <out.json>`.
std::string TerrainExportCtr(const std::string& ctrPath, const std::string& outJson,
                             std::string& err);

// `igi1conv terrain info <f>`.
std::string TerrainInfo(const std::string& path, std::string& err);

// `igi1conv mef export <f.mef> -o <out.obj> [--dat ... --texdir ...]`.
bool MefExportObj(const std::string& mefPath, const std::string& outObj,
                  const std::string& datFile, const std::string& texDir,
                  std::string& err);

// `igi1conv mef info <f.mef>`.
std::string MefInfo(const std::string& mefPath, std::string& err);

// Generates a unique temp file path under the system temp dir.
std::string MakeTempPath(const std::string& suffix);

} // namespace igi1conv
