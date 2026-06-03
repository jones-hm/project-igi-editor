# Comprehensive Test Suite Design

**Date:** 2026-06-03  
**Status:** Approved

---

## Context

The project is a Win32 C++ editor for Project IGI 1. It has 158 existing Google Tests covering the QSC lexer/parser, QVM compiler/decompiler/roundtrip, config, and utils. This spec extends coverage to:

1. The verify-level pipeline (unit + integration)
2. All file-format parsers in `source/parsers/`

### Deployment Invariant

The editor executable (`igi1ed.exe`), the test binary (`igi_tests.exe`), and all IGI game files always live in the **same output directory** (e.g. `bin/Release/`). `Utils::GetExeDirectory()` returns that directory; `Utils::GetIGIRootPath()` falls back to it when `IGI_GAME_PATH` is not set. No env-var configuration is needed — tests derive all paths from `GetExeDirectory()`.

---

## Problem: `verify_level.cpp` Cannot Link Into `igi_tests`

`verify_level.cpp` starts with `#include "pch.h"`, which pulls in `glew.h`, `app.h`, and the full renderer stack. The test binary has no OpenGL context and cannot compile those headers. The pure parsing logic (`ParseLog`, `CrossRef`, `ParseQscObjects`) does not need any of that — it only uses standard C++, `task_schema`, `qvm_parser`/`qvm_decompiler`, `utils`, `logger`, and `common`.

**Fix:** Extract the pure logic into a new lightweight file with no OpenGL dependency.

---

## Architecture

### New Source Files

| File | Purpose |
|------|---------|
| `source/cli/verify_level_core.h` | Public structs (`VerifyObj`, `LevelReport`) and free-function declarations |
| `source/cli/verify_level_core.cpp` | Impl: `ParseLog`, `CrossRef`, `ParseQscObjects`, `LoadModelNames`, `VerifyOneLevel` — **no pch.h, no GL** |

### Modified Source Files

| File | Change |
|------|--------|
| `source/cli/verify_level.cpp` | `#include "verify_level_core.h"`, remove extracted code, keep `LaunchEditor` + report output + `CLIHandler::VerifyLevel` |
| `source/cli/cli_handler.h` | Move `VerifyLevel` from `private` → `public` |

### New Test Files (9 files)

| File | Layer | Game files needed |
|------|-------|-------------------|
| `tests/test_verify_unit.cpp` | Pure unit | No (uses fixtures) |
| `tests/test_verify_level.cpp` | Integration (launches editor) | Yes |
| `tests/test_dat_parser.cpp` | Parser integration | Yes |
| `tests/test_graph_parser.cpp` | Parser integration | Yes |
| `tests/test_res_parser.cpp` | Parser integration | Yes |
| `tests/test_tex_parser.cpp` | Parser integration | Yes |
| `tests/test_mtp_parser.cpp` | Parser integration | Yes |
| `tests/test_fnt_parser.cpp` | Parser integration | Yes |

### New Fixture Files

| File | Used by |
|------|---------|
| `tests/fixtures/verify_log_l1.txt` | `test_verify_unit.cpp` |

---

## `verify_level_core.h/.cpp` — Extracted API

```cpp
// verify_level_core.h
struct VerifyObj {
    std::string modelId, modelName, name, type;
    long long px, py, pz;
    double ox, oy, oz;
    bool ori_logged, posIsRail, tex_logged, mesh_logged;
    std::string texId, meshId;
};

struct LevelReport {
    int levelNo;
    struct Category { /* label, expected, found, pos_mismatch, ori_mismatch,
                         tex_mismatch, mesh_mismatch, missing */ };
    Category buildings, objects, ai;
    bool logError; std::string logErrorMsg; int logEntries;
};

// Free functions (testable):
std::vector<VerifyObj> ParseLog(const std::string& logPath, int levelNo,
                                bool& errorOut, std::string& errorMsg);
std::vector<VerifyObj> ParseQscObjects(const std::string& qscPath,
                                       const std::map<std::string,std::string>& modelNames);
void CrossRef(LevelReport::Category& cat,
              const std::vector<VerifyObj>& logged, bool matchOri);
LevelReport VerifyOneLevel(const std::string& igiPath, const std::string& exeDir,
                           const std::string& logPath, int levelNo,
                           const std::map<std::string,std::string>& modelNames);

// Helpers exposed for tests:
bool PosMatch(const VerifyObj& a, const VerifyObj& b);
bool OriMatch(const VerifyObj& a, const VerifyObj& b);
std::map<std::string,std::string> LoadModelNames(const std::string& path);
```

`verify_level_core.cpp` includes only:
```cpp
#include "verify_level_core.h"
#include "level/task_schema.h"
#include "parsers/qvm_parser.h"
#include "parsers/qvm_decompiler.h"
#include "utils.h"
#include "logger.h"
#include "common.h"
// standard C++ headers
```

---

## CMakeLists.txt — `igi_tests` additions

```cmake
target_sources(igi_tests PRIVATE
    # existing ...
    source/cli/verify_level_core.cpp   # extracted pure verify logic
    source/level/task_schema.cpp       # GetBuiltinSchemas
    source/parsers/dat_parser.cpp
    source/parsers/graph_parser.cpp
    source/parsers/res_parser.cpp
    source/parsers/tex_parser.cpp
    source/parsers/mtp_parser.cpp
    source/parsers/fnt_parser.cpp
)
```

And the test sources:
```cmake
add_executable(igi_tests
    # existing tests ...
    tests/test_verify_unit.cpp
    tests/test_verify_level.cpp
    tests/test_dat_parser.cpp
    tests/test_graph_parser.cpp
    tests/test_res_parser.cpp
    tests/test_tex_parser.cpp
    tests/test_mtp_parser.cpp
    tests/test_fnt_parser.cpp
)
```

---

## Timeout

All per-level operations (both in `VerifyLevelParams::timeout` default and in integration test `WaitForSingleObject`) use a **15-second maximum**. The `VerifyLevelParams::timeout` field default changes from `0` (infinite) to `15`.

---

## Test Coverage Detail

### `test_verify_unit.cpp` — Pure Logic

Uses `tests/fixtures/verify_log_l1.txt` — a synthetic log file with known entries.

**ParseLog tests:**
- Extracts correct `modelId`, `type`, `name`, `px/py/pz` from a well-formed line
- Sets `ori_logged=true` when `Ori=` present, `false` when absent
- Sets `tex_logged` / `mesh_logged` when those fields present
- Returns `errorOut=true` when log file does not exist
- Returns `errorOut=true` when level marker absent from log
- Picks the last occurrence of the level's start marker (handles multiple loads)

**PosMatch / OriMatch tests:**
- `PosMatch` returns true for identical positions
- `PosMatch` returns false when any coordinate differs by 1
- `OriMatch` tolerates difference < 0.05 rad
- `OriMatch` rejects difference ≥ 0.05 rad

**CrossRef tests:**
- Exact match: object in both expected and logged → appears in `found`
- Missing: in expected, not in logged → appears in `missing`
- Position mismatch: same modelId, different pos → `pos_mismatch`
- Orientation mismatch (matchOri=true): same pos, ori differs → `ori_mismatch`
- matchOri=false (AI category): orientation not checked even when logged
- Rail object (`posIsRail=true`): matched by modelId only, no position check
- Tex mismatch: same pos+ori but differing `texId` → `tex_mismatch`
- Mesh mismatch: same pos+ori but differing `meshId` → `mesh_mismatch`

**ParseQscObjects tests:**
- Uses `tests/fixtures/level01_simple.qsc` (existing fixture)
- Parses `Task_New` entries and extracts position/orientation/modelId
- Returns empty vector for non-existent file (no crash)

### `test_verify_level.cpp` — Integration

```
GamePath  = Utils::GetExeDirectory()
ExePath   = GamePath + "\\igi1ed.exe"
```

1. Assert `igi1ed.exe` exists at `ExePath` (fail fast with clear message if not)
2. `CreateProcess("igi1ed.exe --verify-level 1")` with `CREATE_NEW_CONSOLE | STARTF_USESHOWWINDOW | SW_SHOWMINNOACTIVE`
3. `WaitForSingleObject(hProcess, 15000)` — 15-second timeout
4. If `WAIT_TIMEOUT`: `TerminateProcess`, fail test with "verify timed out after 15s"
5. `GetExitCodeProcess` — assert exit code == 0

### `test_dat_parser.cpp`

Path: `GetIGIRootPath() + "\\missions\\location0\\level1\\level1.dat"`

- `dat.valid == true`
- `dat.declaredModelCount > 0`
- `dat.models.size() == (size_t)dat.declaredModelCount`
- All `models[i].modelName` non-empty
- `dat.allTextures.size() > 0`
- `DAT_FormatJSON(dat)` starts with `[` and ends with `]`

### `test_graph_parser.cpp`

Path: `GetIGIRootPath() + "\\missions\\location0\\level1\\graph1.dat"`

- `graph.valid == true`
- `graph.nodes.size() > 0`
- All node IDs ≥ 0
- All node coordinates are finite (not NaN, not ±inf)
- All `material` values in range 0–23

### `test_res_parser.cpp`

Path: `GetIGIRootPath() + "\\missions\\location0\\level1\\models\\level1.res"`

- `res.valid == true`
- `res.entries.size() > 0`
- All entry names non-empty
- `RES_ForEachEntry` callback fires at least once
- `RES_Extract` returns non-empty data for the first entry's name

### `test_tex_parser.cpp`

Strategy: open `level1.res`, find the first `.tex` entry, write to a temp file, parse it.

- Parsing succeeds: `tex.valid == true`
- `tex.version` ∈ {2, 7, 9, 11}
- `tex.images.size() > 0`
- First image: `width > 0`, `height > 0`
- Pixel data size matches `width * height * bytes_per_pixel(mode)`

### `test_mtp_parser.cpp`

Strategy: scan `GetIGIRootPath()` recursively for the first `.mtp` file.

- `mtp.valid == true`
- At least one of `models`, `textures`, `mappings` is non-empty

### `test_fnt_parser.cpp`

Strategy: scan `GetIGIRootPath()` recursively for the first `.fnt` file.

- `font.valid == true`
- `font.lineHeight > 0`
- `font.texWidth > 0`, `font.texHeight > 0`
- `font.glyphs.size() > 0`
- Atlas pixel data size == `texWidth * texHeight * 4`

---

## File Scan Helper

Both `test_mtp_parser.cpp` and `test_fnt_parser.cpp` use a shared inline helper:

```cpp
static std::string FindFirstFile(const std::string& root, const std::string& ext) {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(root, ec))
        if (!ec && e.is_regular_file() && e.path().extension() == ext)
            return e.path().string();
    return "";
}
```

---

## Fixture: `tests/fixtures/verify_log_l1.txt`

Hand-crafted log with entries for both categories (Buildings and AI), covering:
- A well-matched object (pos + ori present)
- An object missing orientation (ori_logged=false)
- An object that will be missing from QSC (for CrossRef missing test)
- Duplicate level-start marker (to test "last occurrence" logic)

Format matches the regex in `ParseLog`:
```
[LevelLoader] Object Loaded: ModelID=100_01_1, Type=Building, Name=Fence, Pos=(1000, 2000, 3000), Ori=(0.0000, 1.5708, 0.0000)
```

---

## Implementation Order

1. Extract `verify_level_core.h/.cpp` from `verify_level.cpp`
2. Update `verify_level.cpp` to include core header, remove extracted code
3. Move `CLIHandler::VerifyLevel` to public in `cli_handler.h`
4. Update `CMakeLists.txt` (sources + test files)
5. Create fixture `tests/fixtures/verify_log_l1.txt`
6. Write `tests/test_verify_unit.cpp`
7. Write `tests/test_verify_level.cpp`
8. Write parser test files (dat, graph, res, tex, mtp, fnt) — 6 files
9. Build and run full test suite
