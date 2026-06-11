# IGI Editor v3.0.0 — CLI & GUI Reference Guide

The **IGI Editor** is a hybrid toolkit that operates in two primary modes:
1. **Graphical User Interface (GUI) Application**: A real-time 3D viewport editor for heightmaps, objects, Splines, AI, and level layouts.
2. **Editor CLI**: Headless commands for level verification, testing, and resource extraction.
3. **igi1conv.exe**: Standalone asset converter, bundled at `editor/tools/igi1conv.exe` (built from the separate [project-igi-conv](https://github.com/jones-hm/project-igi-conv) repo).

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

## igi1conv.exe — Asset Converter

The standalone asset converter has moved to its own repository:
**[project-igi-conv](https://github.com/jones-hm/project-igi-conv)**.

The editor bundles the prebuilt binary at `editor/tools/igi1conv.exe`. For the full
command tree (tex, mef, qsc, qvm, res, mtp, dat, fnt, terrain, graph) see that
repo's [`docs/COMMANDS.md`](https://github.com/jones-hm/project-igi-conv/blob/main/docs/COMMANDS.md),
or run:

```powershell
.\bin\Release\editor\tools\igi1conv.exe --help
.\bin\Release\editor\tools\igi1conv.exe <command> --help
```
