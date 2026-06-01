# IGI Editor — Parsers Reference

> **Repo:** [jones-hm/project-igi1ed](https://github.com/jones-hm/project-igi1ed)  
> **Source path:** `source/parsers/` · `source/cli/cli_handler.cpp`

***

## Parser Summary

| Parser | Format | Function | Output |
|--------|-----------|-----------|-----------|
| **DAT** | `.dat` (text file) | Reads model→texture mapping lists inside a level | JSON (stdout or `.json` file), Plain-text report (`.txt`) |
| **MEF** | `.mef` (binary/ASCII) | Reads 3D model geometry — vertices, faces, bones, normals, attachments | OBJ + MTL, OBJ Bundle folder (`.obj` + `.mtl` + real `.tga` textures), MEF ASCII |
| **QVM** | `.qvm` (bytecode) | Reads IGI script bytecode — opcodes, identifiers, strings, instructions | QSC source text (`.qsc`) via decompile, or stdout info dump |
| **QSC Lexer** | `.qsc` (script text) | Tokenizes raw QSC source | Token stream printed to stdout (`line:col  KIND  lexeme`) |
| **QSC Parser** | `.qsc` (script text) | Builds AST from QSC tokens | AST tree dump to stdout; feeds compiler |
| **QVM Compiler** | `.qsc` (script text) | Compiles QSC source to binary | `.qvm` binary file (LOOP 8.5 format) |
| **RES** | `.res` (ILFF archive) | Reads resource archive containing `.mef`, `.tex`, etc. | Lists entries to stdout, extract single resource → any file, extract ALL → folder |
| **TEX** | `.tex` / `.spr` / `.pic` | Reads IGI proprietary texture format (RGB565 or ARGB8888) | `.tga` image files (one per image/mip-level), info dump to stdout |
| **MTP** | `.mtp` (FORM package) | Reads model-texture package — animations, shadows, models, textures, instance mappings | Inspect only — prints counts to log |
| **Graph** | `graph*.dat` (binary) | Reads AI navigation graph — nodes (position, material, links), edges | Inspect only — prints node/edge count + first 10 nodes to stdout |
| **Terrain** | `.lmp` (lightmap), `.ctr` (quadtree) | Reads terrain lightmap textures and chunk quadtree structure | Inspect only — prints texture/item count to log |
| **FNT** | `.fnt` (bitmap font) | Reads ILFF font atlas — glyphs, UV rects, advance widths | In-memory RGBA pixel data only (used by GUI renderer, no CLI export) |
| **Extract Level** | Level N (1–14) via `.res` + loose `.mef`/`.tex` | Pulls all assets from one full game level at once | `models/levelN/*.mef`, `textures/levelN/*.tex`, `terrain/*.*` |

***

## Conversion Chain

```
.qsc  ──►  compile   ──►  .qvm        (QSC → QVM Compiler)
.qvm  ──►  decompile ──►  .qsc        (QVM Decompiler)
.mef  ──►  export    ──►  .obj / .tga (MEF Exporter)
.tex  ──►  export    ──►  .tga        (TEX Parser)
.res  ──►  extract   ──►  any files   (RES Parser)
.dat  ──►  convert   ──►  .json / .txt (DAT Parser)
```

***

## Per-Parser CLI Commands

### DAT Parser
```bash
# Print JSON to stdout
igi1ed --dat <file.dat>

# Write JSON to file
igi1ed --dat <file.dat> --output <file.json>

# Filter by model name
igi1ed --dat <file.dat> --filter <model_name>

# Plain-text output instead of JSON
igi1ed --dat <file.dat> --text
```

### MEF Parser / Exporter
```bash
# Parse and print model info
igi1ed --mef <file.mef>

# Export to OBJ + MTL
igi1ed --mef <file.mef> --export-obj <out.obj>

# Export to ASCII MEF
igi1ed --mef <file.mef> --export-mef <out.mef>

# Export full OBJ bundle with real textures
igi1ed --mef <file.mef> --export-bundle <outdir> --dat <file.dat> --texdir <dir>
```

### QVM / QSC Pipeline
```bash
# Inspect QVM bytecode
igi1ed --qvm <file.qvm>

# Decompile QVM → QSC source
igi1ed --qvm <file.qvm> --decompile <out.qsc>

# Lex QSC (print tokens)
igi1ed --qsc <file.qsc> --lex

# Parse QSC (dump AST)
igi1ed --qsc <file.qsc> --parse

# Compile QSC → QVM binary
igi1ed --qsc <file.qsc> --compile <out.qvm>
```

### RES Parser
```bash
# List archive contents
igi1ed --res <file.res>

# Extract a single resource
igi1ed --res <file.res> --extract <name> <outfile>

# Extract all resources to directory
igi1ed --res <file.res> --extract-all <dir>
```

### TEX Parser
```bash
# Print texture info
igi1ed --tex <file.tex>

# Export all images as TGA files
igi1ed --tex <file.tex> --export-tga <dir>
```

### MTP / Graph / Terrain (Inspect Only)
```bash
igi1ed --mtp <file.mtp>
igi1ed --graph <file.dat>
igi1ed --terrain <file.lmp>
igi1ed --terrain <file.ctr>
```

### Extract Full Level
```bash
igi1ed --extract-level <levelNo> <outDir>
```

***

## Format Reference

| File Extension | Format Type | Used By |
|---------------|-------------|---------|
| `.dat` | Text — model/texture map | DAT Parser, Graph Parser |
| `.mef` | Binary or ASCII — 3D mesh | MEF Parser / Exporter |
| `.qvm` | Binary bytecode — VM script | QVM Parser / Decompiler |
| `.qsc` | Text — script source | QSC Lexer / Parser / Compiler |
| `.res` | Binary — ILFF resource archive | RES Parser |
| `.tex` / `.spr` / `.pic` | Binary — proprietary texture | TEX Parser |
| `.mtp` | Binary — FORM model-texture package | MTP Parser |
| `.fnt` | Binary — ILFF bitmap font | FNT Parser |
| `.lmp` | Binary — lightmap pictures | Terrain Parser |
| `.ctr` | Binary — terrain quadtree | Terrain Parser |
| `.obj` / `.mtl` | Text — standard 3D model | MEF Export output |
| `.tga` | Binary — standard image | TEX / MEF Export output |
| `.json` | Text — structured data | DAT Export output |
