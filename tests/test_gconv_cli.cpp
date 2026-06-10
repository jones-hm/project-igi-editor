#include <gtest/gtest.h>
#include "utils.h"
#include <string>
#include <filesystem>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ============================================================
//  gconv CLI integration tests
//
//  Spawns gconv.exe (at <exeDir>/content/tools/gconv.exe) with
//  real game files from GetIGIRootPath().  Each test asserts
//  exit code 0; export tests also assert the output file exists
//  and is non-empty.
// ============================================================

static std::string GConvExe() {
    return Utils::GetExeDirectory() + "\\content\\tools\\gconv.exe";
}

static std::string IGI(const std::string& rel) {
    return Utils::GetIGIRootPath() + "\\" + rel;
}

// Temporary directory scoped to the test run — cleaned up automatically.
class TempDir {
public:
    TempDir() {
        char buf[MAX_PATH];
        GetTempPathA(MAX_PATH, buf);
        path_ = std::string(buf) + "gconv_test_" + std::to_string(GetCurrentProcessId());
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    std::string operator/(const std::string& name) const { return path_ + "\\" + name; }
private:
    std::string path_;
};

// Run gconv.exe with space-separated args, return exit code.
// stdout/stderr go to NUL so the process never blocks on output.
static int RunGConv(const std::string& args, DWORD timeoutMs = 15000) {
    const std::string exePath = GConvExe();
    std::string cmdLine = "\"" + exePath + "\" " + args;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE,
                               &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput  = hNull;
    si.hStdError   = hNull;
    si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    std::vector<char> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back('\0');

    BOOL ok = CreateProcessA(nullptr, buf.data(),
                             nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW,
                             nullptr, nullptr, &si, &pi);
    CloseHandle(hNull);
    if (!ok) return -1;

    DWORD wait = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD code = 1;
    if (wait == WAIT_OBJECT_0)
        GetExitCodeProcess(pi.hProcess, &code);
    else
        TerminateProcess(pi.hProcess, 1);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}

// ── Prerequisite guard ──────────────────────────────────────

class GConvCli : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(std::filesystem::exists(GConvExe()))
            << "gconv.exe not found: " << GConvExe();
    }
};

// ── tex ─────────────────────────────────────────────────────

TEST_F(GConvCli, TexInfo) {
    std::string f = IGI("common\\TEXTURES\\FLARE00.TEX");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("tex info \"" + f + "\""), 0);
}

TEST_F(GConvCli, TexDecode) {
    TempDir tmp;
    std::string f      = IGI("common\\TEXTURES\\FLARE00.TEX");
    std::string outDir = tmp / "texout";
    std::filesystem::create_directories(outDir);
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("tex decode \"" + f + "\" -o \"" + outDir + "\""), 0);
    // tex decode writes <outDir>/<NAME>.tga
    std::string tga = outDir + "\\FLARE00.tga";
    EXPECT_TRUE(std::filesystem::exists(tga));
    EXPECT_GT(std::filesystem::file_size(tga), 0u);
}

// ── mef ─────────────────────────────────────────────────────

TEST_F(GConvCli, MefInfo) {
    std::string f = IGI("content\\models\\common\\005_01_1.mef");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("mef info \"" + f + "\""), 0);
}

TEST_F(GConvCli, MefExport) {
    TempDir tmp;
    std::string f   = IGI("content\\models\\common\\005_01_1.mef");
    std::string out = tmp / "model.obj";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("mef export \"" + f + "\" -o \"" + out + "\""), 0);
    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

// ── qvm ─────────────────────────────────────────────────────

TEST_F(GConvCli, QvmInfo) {
    std::string f = IGI("ammo\\1000\\AMMO.QVM");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("qvm info \"" + f + "\""), 0);
}

TEST_F(GConvCli, QvmDisasm) {
    std::string f = IGI("ammo\\1000\\AMMO.QVM");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("qvm disasm \"" + f + "\""), 0);
}

TEST_F(GConvCli, QvmDecompile) {
    TempDir tmp;
    std::string f   = IGI("ammo\\1000\\AMMO.QVM");
    std::string out = tmp / "ammo.qsc";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("qvm decompile \"" + f + "\" -o \"" + out + "\""), 0);
    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

// ── res ─────────────────────────────────────────────────────

TEST_F(GConvCli, ResList) {
    std::string f = IGI("common\\SOUNDS\\SOUNDS.RES");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("res list \"" + f + "\""), 0);
}

TEST_F(GConvCli, ResExtract) {
    TempDir tmp;
    std::string f   = IGI("common\\SOUNDS\\SOUNDS.RES");
    std::string out = tmp / "res_out";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("res extract \"" + f + "\" -o \"" + out + "\""), 0);
    EXPECT_TRUE(std::filesystem::is_directory(out));
}

// ── mtp ─────────────────────────────────────────────────────

TEST_F(GConvCli, MtpInfo) {
    std::string f = IGI("common\\common.mtp");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("mtp info \"" + f + "\""), 0);
}

TEST_F(GConvCli, MtpDump) {
    TempDir tmp;
    std::string f   = IGI("common\\common.mtp");
    std::string out = tmp / "mtp.json";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("mtp dump \"" + f + "\" -o \"" + out + "\""), 0);
    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

// ── dat ─────────────────────────────────────────────────────

TEST_F(GConvCli, DatInfo) {
    std::string f = IGI("common\\common.dat");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("dat info \"" + f + "\""), 0);
}

TEST_F(GConvCli, DatExport) {
    TempDir tmp;
    std::string f   = IGI("common\\common.dat");
    std::string out = tmp / "dat.json";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("dat export \"" + f + "\" -o \"" + out + "\""), 0);
    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

// ── fnt ─────────────────────────────────────────────────────

TEST_F(GConvCli, FntInfo) {
    std::string f = IGI("computer\\computer\\font1.fnt");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("fnt info \"" + f + "\""), 0);
}

TEST_F(GConvCli, FntExport) {
    TempDir tmp;
    std::string f   = IGI("computer\\computer\\font1.fnt");
    std::string out = tmp / "font1.png";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("fnt export \"" + f + "\" -o \"" + out + "\""), 0);
    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

// ── graph ────────────────────────────────────────────────────

TEST_F(GConvCli, GraphInfo) {
    std::string f = IGI("content\\backup\\level1\\graphs\\graph1.dat");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("graph info \"" + f + "\""), 0);
}

TEST_F(GConvCli, GraphExport) {
    TempDir tmp;
    std::string f   = IGI("content\\backup\\level1\\graphs\\graph1.dat");
    std::string out = tmp / "graph.json";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("graph export \"" + f + "\" -o \"" + out + "\""), 0);
    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

// ── terrain ──────────────────────────────────────────────────

TEST_F(GConvCli, TerrainInfo) {
    std::string f = IGI("content\\backup\\level1\\terrain\\TERRAIN.LMP");
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("terrain info \"" + f + "\""), 0);
}

TEST_F(GConvCli, TerrainExportLmp) {
    TempDir tmp;
    std::string f    = IGI("content\\backup\\level1\\terrain\\TERRAIN.LMP");
    std::string stem = tmp / "lmp";
    ASSERT_TRUE(std::filesystem::exists(f)) << f;
    EXPECT_EQ(RunGConv("terrain export-lmp \"" + f + "\" -o \"" + stem + "\""), 0);
    // export-lmp writes <stem>_0, <stem>_1, ... (PGM files, no directory)
    EXPECT_TRUE(std::filesystem::exists(stem + "_0"));
}

// ── error handling ───────────────────────────────────────────

TEST_F(GConvCli, MissingFileReturnsNonZero) {
    EXPECT_NE(RunGConv("tex info \"C:\\nonexistent_file_xyz.TEX\""), 0);
}

TEST_F(GConvCli, UnknownCommandReturnsNonZero) {
    EXPECT_NE(RunGConv("badcmd info \"somefile\""), 0);
}
