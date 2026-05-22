"""
IGI Level Object Verification Script
Usage:
    python level_object_verification.py --level 1
    python level_object_verification.py --level 1 --level 2 --level 3
    python level_object_verification.py --level 1 --timeout 60
    python level_object_verification.py --level 1 --skip-launch  (parse existing log only)

Launches igi1ed.exe -level N, waits for exit, then verifies
loaded objects against IGIModelsAllLevel.json.
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
GAME_PATH = Path(r"D:\IGI1")
EDITOR_EXE = GAME_PATH / "igi1ed.exe"
LOG_FILE    = GAME_PATH / "igi_editor.log"
QSC_FILE    = GAME_PATH / "objects.qsc"

# Tolerance for floating-point position comparison (game units)
POS_TOLERANCE = 1000.0   # 1000 game units ≈ 24 m


# ---------------------------------------------------------------------------
# Locate IGIModelsAllLevel.json by scanning *.json under GAME_PATH
# ---------------------------------------------------------------------------
def find_models_json() -> Path:
    candidates = list(GAME_PATH.glob("*.json"))
    for c in candidates:
        if "AllLevel" in c.name or "AllLevels" in c.name:
            return c
    # Fallback: any json with "Models" in name
    for c in candidates:
        if "Model" in c.name:
            return c
    raise FileNotFoundError(
        f"Could not find IGIModelsAllLevel.json in {GAME_PATH}. "
        f"Files found: {[c.name for c in candidates]}"
    )


# ---------------------------------------------------------------------------
# Parse IGIModelsAllLevel.json for a given level
# ---------------------------------------------------------------------------
def load_expected(json_path: Path, level_no: int) -> dict:
    with open(json_path, encoding="utf-8") as f:
        data = json.load(f)

    key = f"Level {level_no}"
    if key not in data:
        raise KeyError(f"'{key}' not found in {json_path.name}. "
                       f"Available: {list(data.keys())}")

    level_data = data[key]
    expected = {"buildings": [], "objects": [], "ai": []}

    for entry in level_data.get("Buildings", []):
        expected["buildings"].append({
            "name":     entry.get("Name", ""),
            "model_id": entry.get("Model ID", ""),
            "task_id":  str(entry.get("Task ID", -1)),
            "pos":      (float(entry.get("Pos X", 0)),
                         float(entry.get("Pos Y", 0)),
                         float(entry.get("Pos Z", 0))),
        })

    for entry in level_data.get("Objects", []):
        expected["objects"].append({
            "name":     entry.get("Name", ""),
            "model_id": entry.get("Model ID", ""),
            "task_id":  str(entry.get("Task ID", -1)),
            "pos":      (float(entry.get("Pos X", 0)),
                         float(entry.get("Pos Y", 0)),
                         float(entry.get("Pos Z", 0))),
        })

    for entry in level_data.get("AI", []):
        pos_block = entry.get("Position", {})
        expected["ai"].append({
            "name":     entry.get("Name", ""),
            "type":     entry.get("Type", ""),
            "model_id": entry.get("Model", {}).get("ID", ""),
            "pos":      (float(pos_block.get("X", 0)),
                         float(pos_block.get("Y", 0)),
                         float(pos_block.get("Z", 0))),
        })

    return expected


# ---------------------------------------------------------------------------
# Parse objects.qsc for loaded Task_New entries
# ---------------------------------------------------------------------------
TASK_NEW_RE = re.compile(
    r'Task_New\(\s*(-?\d+)\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*'
    r'(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)'
)

def parse_qsc(qsc_path: Path) -> dict:
    """Extract Building / HumanSoldier + HumanAI entries from a QSC file."""
    if not qsc_path.exists():
        return {"buildings": [], "objects": [], "ai": []}

    buildings, objects_, ai = [], [], []

    with open(qsc_path, encoding="utf-8", errors="replace") as f:
        text = f.read()

    for m in TASK_NEW_RE.finditer(text):
        task_id  = m.group(1)
        obj_type = m.group(2)
        name     = m.group(3)
        px, py, pz = float(m.group(4)), float(m.group(5)), float(m.group(6))

        # Extract model ID: first quoted string after the rotation fields
        after = text[m.end():]
        model_match = re.search(r'"([^"]{3,20})"', after[:200])
        model_id = model_match.group(1) if model_match else ""

        entry = {"task_id": task_id, "name": name, "type": obj_type,
                 "model_id": model_id, "pos": (px, py, pz)}

        if obj_type == "Building":
            buildings.append(entry)
        elif obj_type in ("HumanSoldier", "EditRigidObj"):
            objects_.append(entry)
        elif obj_type == "HumanAI":
            ai.append(entry)

    return {"buildings": buildings, "objects": objects_, "ai": ai}


# ---------------------------------------------------------------------------
# Parse igi_editor.log after the run
# ---------------------------------------------------------------------------
def parse_log(log_path: Path, level_no: int) -> dict:
    if not log_path.exists():
        return {}

    with open(log_path, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    # Find the LAST LoadLevel START for this level (most recent run)
    start_idx = -1
    for i in range(len(lines) - 1, -1, -1):
        if f"LoadLevel() START for level {level_no}" in lines[i]:
            start_idx = i
            break

    if start_idx == -1:
        return {"error": f"No 'LoadLevel() START for level {level_no}' found in log"}

    run_lines = lines[start_idx:]

    loaded_models   = set()
    failed_models   = set()
    attachment_counts = {}
    attachments_not_found = []
    snap_count = 0
    ai_updates = 0
    errors = []
    warnings = []

    for line in run_lines:
        stripped = line.strip()

        # Model load success
        m = re.search(r'\[MEF Binary Native\] Loaded: .+?[/\\](\w+\.mef)\s*\|', stripped)
        if m:
            loaded_models.add(m.group(1).replace(".mef", ""))

        # Model load failure
        if "Load FAILED" in stripped or "load FAILED" in stripped:
            m2 = re.search(r"Load FAILED for (\S+):", stripped)
            if m2:
                failed_models.add(m2.group(1))

        # Attachments
        m3 = re.search(r"Attachments for '([^']+)': (\d+)", stripped)
        if m3:
            attachment_counts[m3.group(1)] = int(m3.group(2))

        m4 = re.search(r"Attachment sub-model NOT FOUND: (\S+)", stripped)
        if m4:
            attachments_not_found.append(m4.group(1))

        # AI updates
        if "Updated existing AI object" in stripped:
            ai_updates += 1

        # Snapped
        if "Snapped" in stripped and "to Z=" in stripped:
            snap_count += 1

        # Errors and warnings
        if "[ERR]" in stripped and "LoadLevel" not in stripped:
            errors.append(stripped)
        if "[WARNING]" in stripped:
            warnings.append(stripped)

    return {
        "loaded_models":         sorted(loaded_models),
        "failed_models":         sorted(failed_models),
        "attachment_counts":     attachment_counts,
        "attachments_not_found": sorted(set(attachments_not_found)),
        "ai_updates":            ai_updates,
        "snap_count":            snap_count,
        "errors":                errors[:20],
        "warnings":              warnings[:20],
    }


# ---------------------------------------------------------------------------
# Cross-reference expected JSON against QSC + log data
# ---------------------------------------------------------------------------
def pos_close(p1, p2, tol=POS_TOLERANCE) -> bool:
    return all(abs(a - b) <= tol for a, b in zip(p1, p2))


def verify(expected: dict, qsc_data: dict, log_data: dict, level_no: int) -> dict:
    report = {
        "level": level_no,
        "buildings": {"total_expected": 0, "found": [], "missing": [], "wrong_model": []},
        "objects":   {"total_expected": 0, "found": [], "missing": []},
        "ai":        {"total_expected": 0, "found_models": [], "missing_models": []},
        "log_summary": log_data,
    }

    # --- Buildings ---
    report["buildings"]["total_expected"] = len(expected["buildings"])
    qsc_buildings = qsc_data.get("buildings", [])

    for exp in expected["buildings"]:
        # Match by model_id + position proximity
        matched = [q for q in qsc_buildings
                   if q["model_id"] == exp["model_id"]
                   and pos_close(q["pos"], exp["pos"])]
        if matched:
            report["buildings"]["found"].append({
                "model_id": exp["model_id"],
                "name": exp["name"],
                "pos": exp["pos"],
            })
        else:
            # Try model-id only (position might differ due to terrain snap)
            model_only = [q for q in qsc_buildings if q["model_id"] == exp["model_id"]]
            if model_only:
                report["buildings"]["found"].append({
                    "model_id": exp["model_id"],
                    "name": exp["name"],
                    "note": "model found, pos differs > tolerance",
                })
            else:
                report["buildings"]["missing"].append({
                    "model_id": exp["model_id"],
                    "name": exp["name"],
                    "pos": exp["pos"],
                })

    # --- AI ---
    report["ai"]["total_expected"] = len(expected["ai"])
    expected_ai_models = sorted(set(e["model_id"] for e in expected["ai"] if e["model_id"]))
    report["ai"]["unique_models_expected"] = len(expected_ai_models)
    log_loaded = set(log_data.get("loaded_models", []))

    for model_id in expected_ai_models:
        if model_id in log_loaded:
            report["ai"]["found_models"].append(model_id)
        else:
            report["ai"]["missing_models"].append(model_id)

    return report


# ---------------------------------------------------------------------------
# Launch editor and wait
# ---------------------------------------------------------------------------
def launch_editor(level_no: int, timeout: int) -> bool:
    """Launch igi1ed.exe -level N. If timeout>0, kill after N seconds."""
    if not EDITOR_EXE.exists():
        print(f"[ERROR] Editor not found: {EDITOR_EXE}")
        return False

    cmd = [str(EDITOR_EXE), "-level", str(level_no)]
    print(f"  Launching: {' '.join(cmd)}")
    print(f"  {'Waiting for editor to exit...' if timeout == 0 else f'Will kill after {timeout}s...'}")

    proc = subprocess.Popen(cmd, cwd=str(GAME_PATH))

    try:
        if timeout > 0:
            proc.wait(timeout=timeout)
        else:
            proc.wait()
    except subprocess.TimeoutExpired:
        print(f"  Timeout reached ({timeout}s), killing editor...")
        proc.kill()
        proc.wait()
    except KeyboardInterrupt:
        print("  Interrupted — killing editor...")
        proc.kill()
        proc.wait()
        return False

    print(f"  Editor exited (return code {proc.returncode})")
    return True


# ---------------------------------------------------------------------------
# Report printer
# ---------------------------------------------------------------------------
def print_report(report: dict):
    lvl = report["level"]
    sep = "=" * 60

    print(f"\n{sep}")
    print(f" LEVEL {lvl} VERIFICATION REPORT")
    print(f"{sep}")

    b = report["buildings"]
    print(f"\n[BUILDINGS]  expected={b['total_expected']}  "
          f"found={len(b['found'])}  missing={len(b['missing'])}")
    if b["missing"]:
        print("  MISSING buildings:")
        for item in b["missing"]:
            print(f"    - {item['name']} ({item['model_id']})  pos={item['pos']}")
    if b.get("wrong_model"):
        print("  WRONG MODEL:")
        for item in b["wrong_model"]:
            print(f"    - {item}")

    ai = report["ai"]
    print(f"\n[AI MODELS]  total_ai_entries={ai['total_expected']}"
          f"  unique_models={ai.get('unique_models_expected', '?')}"
          f"  found_in_log={len(ai['found_models'])}"
          f"  missing_from_log={len(ai['missing_models'])}")
    if ai["missing_models"]:
        print("  MISSING AI models (not loaded in log):")
        for m in ai["missing_models"]:
            print(f"    - {m}")

    log = report["log_summary"]
    if not log or "error" in log:
        print(f"\n[LOG]  {log.get('error', 'no log data')}")
    else:
        print(f"\n[LOG SUMMARY]")
        print(f"  Models loaded:         {len(log['loaded_models'])}")
        print(f"  Models failed:         {len(log['failed_models'])}")
        print(f"  AI updates:            {log['ai_updates']}")
        print(f"  Snap operations:       {log['snap_count']}")
        print(f"  Attachments parsed:    {len(log['attachment_counts'])}")
        if log["attachments_not_found"]:
            print(f"  Sub-models NOT found:  {len(log['attachments_not_found'])}")
            for m in log["attachments_not_found"][:10]:
                print(f"    - {m}")
        if log["failed_models"]:
            print(f"  FAILED model loads:")
            for m in log["failed_models"][:10]:
                print(f"    - {m}")
        if log["errors"]:
            print(f"  ERRORS ({len(log['errors'])}):")
            for e in log["errors"][:5]:
                print(f"    {e}")

    # Pass/fail summary
    total_issues = (len(b["missing"]) + len(ai["missing_models"])
                    + len(log.get("failed_models", [])))
    status = "PASS" if total_issues == 0 else f"FAIL ({total_issues} issue(s))"
    print(f"\n  Result: {status}")
    print(sep)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="IGI Level Object Verification — launches editor and verifies object loading."
    )
    parser.add_argument("--level", type=int, action="append", dest="levels",
                        metavar="N", required=True,
                        help="Level number to verify (can repeat, e.g. --level 1 --level 2)")
    parser.add_argument("--timeout", type=int, default=0,
                        help="Seconds to wait before killing editor (0 = wait for manual close)")
    parser.add_argument("--skip-launch", action="store_true",
                        help="Skip launching the editor; analyze existing log and objects.qsc only")
    parser.add_argument("--game-path", type=str, default=r"D:\IGI1",
                        help="Path to IGI1 install (default: D:\\IGI1)")
    parser.add_argument("--report-json", type=str, default="",
                        help="If set, write full JSON report to this file")
    args = parser.parse_args()

    game_path  = Path(args.game_path)
    editor_exe = game_path / "igi1ed.exe"
    log_file   = game_path / "igi_editor.log"
    qsc_file   = game_path / "objects.qsc"

    GAME_PATH  = game_path
    EDITOR_EXE = editor_exe
    LOG_FILE   = log_file
    QSC_FILE   = qsc_file

    # Find JSON reference
    try:
        json_path = find_models_json()
        print(f"[INFO] Reference JSON: {json_path}")
    except FileNotFoundError as e:
        print(f"[ERROR] {e}")
        sys.exit(1)

    all_reports = []

    for level_no in args.levels:
        print(f"\n{'='*60}")
        print(f" Processing Level {level_no}")
        print(f"{'='*60}")

        # Load expected data
        try:
            expected = load_expected(json_path, level_no)
        except KeyError as e:
            print(f"[ERROR] {e}")
            continue

        print(f"  Expected: {len(expected['buildings'])} buildings, "
              f"{len(expected['objects'])} objects, {len(expected['ai'])} AI")

        # Launch editor (unless --skip-launch)
        if not args.skip_launch:
            ok = launch_editor(level_no, args.timeout)
            if not ok:
                print(f"[WARN] Editor launch failed for level {level_no}")
        else:
            print("  [SKIP-LAUNCH] Using existing log and objects.qsc")
            print("  [WARN] objects.qsc may be from a different level — "
                  "run without --skip-launch for accurate results")

        # Small delay to ensure log file is flushed
        time.sleep(0.5)

        # Parse QSC (freshly written by editor on load)
        qsc_data = parse_qsc(QSC_FILE)
        print(f"  QSC parsed: {len(qsc_data['buildings'])} buildings, "
              f"{len(qsc_data['objects'])} objects, {len(qsc_data['ai'])} AI entries")

        # Parse log
        log_data = parse_log(LOG_FILE, level_no)

        # Verify
        report = verify(expected, qsc_data, log_data, level_no)
        print_report(report)
        all_reports.append(report)

    # Optionally dump JSON
    if args.report_json:
        out_path = Path(args.report_json)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(all_reports, f, indent=2)
        print(f"\n[INFO] Full report written to {out_path}")

    # Overall pass/fail exit code
    any_fail = any(
        len(r["buildings"]["missing"]) > 0 or
        len(r["ai"]["missing_models"]) > 0 or
        len(r["log_summary"].get("failed_models", [])) > 0
        for r in all_reports
    )
    sys.exit(1 if any_fail else 0)


if __name__ == "__main__":
    main()
