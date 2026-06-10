# IGI Editor v3.0.0 — CLI & GUI Reference Guide

The **IGI Editor** is a hybrid toolkit that operates in two primary modes:
1. **Graphical User Interface (GUI) Application**: A real-time 3D viewport editor for heightmaps, objects, Splines, AI, and level layouts.
2. **Editor CLI**: Headless commands for level verification, testing, and resource extraction.
3. **gconv1.exe**: Standalone asset converter for all parser operations (located in `content/tools/`).

---

## igi1ed.exe — Editor (GUI + headless verification)

### GUI Launch Options

```powershell
.\bin\Release\igi1ed.exe -level <num> [options]
```

| Argument | Parameter Type | Description |
| :--- | :--- | :--- |
| `-level` | `integer (1-14)` | Specifies which of the 14 original game levels to load. |
| `-w` | `integer` | Width of the editor window in pixels (e.g., `-w 1280`). |
| `-h` | `integer` | Height of the editor window in pixels (e.g., `-h 720`). |
| `-draw_parts` | `bitmask (0-127)` | Controls which elements of the level environment are parsed and rendered. |
| `-stick_to_ground` | *Flag* | Enforces automated ground-snapping on load. |

### Selective Loading (`-draw_parts` Bitmask)

| Bit Value | Subsystem |
| :---: | :--- |
| **1** | Terrain |
| **2** | Sky |
| **4** | Objects |
| **8** | Flat Sky |
| **16** | Buildings |
| **32** | Props |
| **64** | AI Units |

### GUI Keyboard Controls

| Key | Action |
| :--- | :--- |
| `W` / `S` / `A` / `D` | Fly forward, backward, left, right |
| `Q` / `Z` | Adjust altitude (Up / Down) |
| `PageUp` / `PageDown` | Increase / Decrease movement speed |
| `Alt + Enter` | Toggle Fullscreen Mode |
| `F4` | Toggle Edit Mode |
| `F3` | Toggle Collision clipping |
| `F2` | Toggle Terrain Paint brush mode |
| `LMB Click` | Select object in Edit Mode |
| `F11` | Teleport camera to selected object |

---

### Editor CLI Commands

```powershell
.\bin\Release\igi1ed.exe --help
.\bin\Release\igi1ed.exe --run-tests
.\bin\Release\igi1ed.exe --verify-level --level 1 [--level 2 ...]
.\bin\Release\igi1ed.exe --extract-level <N> [outdir]
```

#### `--run-tests`
Runs all native C++ parser unit tests (QSC, QVM, RES, TEX, MTP, etc.).

#### `--verify-level`
Launches igi1ed headlessly, loads each specified level, compares QVM instructions against the editor log.

Flags:
- `--skip-launch` — use existing log without re-launching editor
- `--timeout <sec>` — kill editor after N seconds
- `--game-path <path>` — IGI1 install path
- `--log <path>` — override log file path
- `--report-json <file>` — write aggregated JSON report
- `--report-md <file>` — write aggregated Markdown report
- `--report-dir <dir>` — write per-level reports to directory
- `--delay <sec>` — delay between levels (default: 5)

#### `--extract-level <N> [outdir]`
Extracts all model, texture, and terrain assets for level N to a directory (defaults to `levels/levelN/`).

---

## gconv1.exe — Asset Converter (located in `content/tools/`)

Standalone CLI tool for reading, converting, and inspecting IGI game assets. No OpenGL or editor context required.

```powershell
.\bin\Release\content\tools\gconv1.exe --help
.\bin\Release\content\tools\gconv1.exe <command> --help
```

### Commands

#### `tex` — Texture Operations

```powershell
gconv1 tex info   <input.tex|.spr|.pic>
gconv1 tex decode <input.tex|.spr|.pic> -o <output_dir>
gconv1 tex decode <folder/> -o <output_dir> --batch
gconv1 tex to-png <input.tga|.tex> -o <out.png>
gconv1 tex to-tga <input.png|.tex> -o <out.tga>
```

#### `mef` — 3D Mesh Operations

```powershell
gconv1 mef info   <input.mef>
gconv1 mef dump   <input.mef> [-o <output.txt>]
gconv1 mef export <input.mef> -o <output.obj>
gconv1 mef export <folder/> -o <output_dir> --batch
gconv1 mef bundle <input.mef> -o <outdir> --dat <file.dat> --texdir <dir>
```

#### `qsc` — QSC Quest Script

```powershell
gconv1 qsc validate <file.qsc>
gconv1 qsc compile  <file.qsc> -o <out.qvm>
gconv1 qsc lex      <file.qsc>
gconv1 qsc parse    <file.qsc>
```

#### `qvm` — QVM Bytecode

```powershell
gconv1 qvm info       <file.qvm>
gconv1 qvm decompile  <file.qvm> -o <out.qsc>
gconv1 qvm disasm     <file.qvm>
```

#### `res` — RES Archive

```powershell
gconv1 res list    <input.res>
gconv1 res extract <input.res> -o <output_dir> [--file <name>]
gconv1 res compile <file.qsc>
gconv1 res pack    <dir> <out.res>
gconv1 res unpack  <file.res> <dir>
```

#### `mtp` — MTP Terrain Properties

```powershell
gconv1 mtp info   <input.mtp>
gconv1 mtp dump   <input.mtp> [-o <output.json>]
gconv1 mtp to-dat <input.mtp> [-o <out.dat>]
```

#### `dat` — DAT Model-Texture Data

```powershell
gconv1 dat info   <file.dat>
gconv1 dat export <file.dat> [-o <out.json>] [--filter <model>] [--text]
gconv1 dat to-mtp <file.dat> [-o <out.mtp>]
```

#### `fnt` — FNT Bitmap Font

```powershell
gconv1 fnt info   <file.fnt>
gconv1 fnt export <file.fnt> -o <out.png>
```

#### `terrain` — Terrain Height/Cube Data

```powershell
gconv1 terrain info       <file.ctr|.lmp>
gconv1 terrain export-lmp <file.lmp> -o <outdir>
gconv1 terrain export-ctr <file.ctr> -o <outdir>
```

#### `graph` — AI Navigation Graph

```powershell
gconv1 graph info   <file.dat>
gconv1 graph export <file.dat> -o <out.json>
```

---

Run `gconv1 --help` for the full command tree, or `gconv1 <command> --help` for per-command usage.
