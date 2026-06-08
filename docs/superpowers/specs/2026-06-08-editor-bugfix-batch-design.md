# Editor Bugfix & Feature Batch — Design Spec

**Date:** 2026-06-08
**Author:** Jones-HM (with Claude)
**Status:** Approved for implementation planning

## Overview

Seven reported issues against the IGI Editor (current code is v2.8.0; some bug
reports/screenshots are from v2.7.0). All confirmed to manifest in the **editor
viewport** (not only in-game). Grouped below by confidence: four have a
confirmed root cause and a concrete fix; three require in-editor reproduction to
pin the cause before fixing.

The work is independent per issue and can be implemented/parallelized as
separate units.

---

## Confident Fixes

### Issue 1 — Weapon ID does not live-update in the editor

**Symptom:** Changing a `GunPickup`'s weapon (e.g. `WEAPON_ID_UZI` →
`WEAPON_ID_XXXX`) does not change the rendered model until the level is reloaded.

**Root cause (confirmed):** For `GunPickup`/`AmmoPickup`, QSC arg 9 holds the
enum string while `obj.modelId` holds the *resolved render model*. The load-time
resolution pass (`level_objects.cpp:187–208`, `:880–887`) maps enum → model id,
but the interactive edit path does not. In `App::CommitPropTextEdit`
(`app.cpp:4763`), the committed arg token is written, but `obj.modelId` is only
re-synced when `is_model_field` is true (`app.cpp:4789`), which matches fields
literally named "Model". The weapon-enum field is not named "Model", so
`obj.modelId` stays stale until reload re-runs the resolution pass.

**Fix:** In `CommitPropTextEdit`, after committing a field on a
`GunPickup`/`AmmoPickup`, if the committed field is the weapon/ammo enum arg,
re-resolve `obj.modelId` from `LevelObjects`' `modelIds_` map (reuse the existing
logic at `level_objects.cpp:880–887`). Expose a small helper on `LevelObjects`
(e.g. `ResolvePickupModelId(const std::string& enumId)`) so the resolution rule
lives in one place. The render mesh then updates immediately.

**Verify:** Load a level with a GunPickup, change its weapon enum in the prop
panel, confirm the rendered model changes without reload.

---

### Issue 6 — Shift+M should toggle Magic objects

**Status:** Already implemented in v2.8.0:
- Keybinding: `qedkeybindings.qsc:115` → `SetEventBinding("TaskMagicObjToggle", "<Shift><M>")`
- Handler: `app.cpp:3284` toggles `show_magic_obj_spheres_`
- Renderer: `renderer_objects.cpp:1702` draws the spheres when enabled

The bug report's screenshot is v2.7.0.

**Fix:** Verify on a current build. If Shift+M correctly toggles magic-object
spheres, no code change — close as already-fixed. If it does not toggle (e.g.
binding not loaded, or spheres never render), debug the render/binding path and
fix the actual break found.

**Verify:** In a current build, press Shift+M and confirm magic-object spheres
appear/disappear.

---

### Issue 3 — Terrain ID tooltip + "Add Terrain" hint

**Symptom:** The viewport tooltip shows a hardcoded `"Terrain ID: -1"`
(`renderer.cpp:993`) instead of the real terrain id under the cursor.

**Fix:** When the cursor is over terrain (and not over an object/UI), raycast the
cursor ray against the terrain to determine the terrain region/tile under it,
resolve its real terrain id, and render:
- `Terrain ID: <id>`
- `Add Terrain: <id>` (hint line showing the command form the user can use)

Implementation notes (to confirm during implementation): identify the terrain
picking primitive in `terrain.cpp` / `renderer_terrain.cpp`. If a ground-ray
intersection already exists for object placement/snapping, reuse it; otherwise
add a minimal ray-vs-terrain query that returns the terrain id. Keep the change
scoped to the tooltip branch at `renderer.cpp:992–994`.

**Verify:** Hover terrain in the viewport; tooltip shows the correct terrain id
(matching the level data) and the `Add Terrain: <id>` hint. Over an object, the
object tooltip still wins.

---

### Issue 2 — Validate model against the level's `.res`; warn + offer to add

**Symptom:** The editor renders models from a shared pool, so a model missing
from the *level's* `.res` still displays in-editor but is transparent in-game.

**Design:** Three parts.

1. **Enumerate the level's `.res` model set.** On level load, stream the level's
   `.res` (`RES_ForEachEntry`, `res_parser.h:26`) and collect the set of model
   resource names present. Cache it on the level/app.

2. **Warn on commit.** When a model field is committed (`CommitPropTextEdit`,
   the `is_model_field` branch, `app.cpp:4789`), check the chosen model against
   the cached set. If absent: set a status message warning and apply a visual
   tint/marker to the object in the viewport so the user sees it will be
   transparent in-game.

3. **Offer to add.** After the warning, offer an action to add the missing model
   to the level's `.res`. Mechanism: parse the level `.res` (`RES_Parse`), copy
   the model's resource(s) — the MEF and any required textures — from the source
   archive the editor resolved the model from, append them, and write the archive
   back via `RES_WriteEntries` (`res_compiler.h:15`).

   **Open implementation detail:** the source of the model data. During
   implementation, determine where `GetOrLoadMesh` pulls model bytes from (which
   archive/path) so the "add" copies the correct MEF + texture resources. If a
   model's source bytes cannot be located, the "offer to add" degrades
   gracefully to warn-only for that model.

**Scope guard:** Adding rewrites the level `.res`. Gate it behind explicit user
confirmation and back up the original archive before writing.

**Verify:** Set a soldier/object to a model not in the level `.res` → warning +
tint shown. Accept "add" → model resource appears in the rewritten `.res`;
reload confirms it now resolves from the level archive.

---

## Diagnose-First (reproduce in editor, then fix)

These three are confirmed to appear in the editor viewport. Reproduction will be
done by launching the editor against the IGI install (`D:\IGI1`) and loading the
relevant level. Each fix is implemented only after the cause is confirmed.

### Issue 5 — Zipline wire "widens" inside the house (Level 12, `426_02_1`)

**Strong lead:** In `renderer_splines.cpp::DrawSplineSegment`, each tile scales X
(length) by `sx = chordLen / localLen` (`:162`) but keeps width/height fixed at
`LENGTH_SCALE = 40.96` (`:168`). On short segments (e.g. the wire span inside the
house), length compresses while the cross-section stays full width → the wire
looks disproportionately wide ("widened breadth").

**Plan:** Reproduce on Level 12 with model `426_02_1`. Confirm the
short-segment/cross-section interaction. Fix so the wire/cable cross-section
stays proportionate to its natural model regardless of segment length (e.g.
decouple cable cross-section scaling from the per-tile X stretch, or special-case
wire segments). Confirm no regression to cable-car *track* tiles, which share
this code path.

### Issue 4 — Door-frame texture artifact

**Lead:** The striped/noisy band around the door frame looks like a texture
wrap/UV or texture-decode issue on the frame texture (or its specific MEF
submesh).

**Plan:** Reproduce, identify the offending door model + texture, inspect the
texture decode (`tex_parser.cpp`) and the sampler/UV setup in the object shader
path. Determine whether it is a UV-wrap (`GL_REPEAT` vs `GL_CLAMP`), a mip, or a
decode error, then fix the confirmed cause.

### Issue 7 — Bugged interactable objects (e.g. M2HB mounted machine gun)

**Lead:** `StationaryGun` / `AIStationaryGunHolder` are in the `isMissingGeneric`
set (`level_objects.cpp:272`). Their model id is found by scanning args for an
8-char `NNN_NN_N` token (`:897–903`); if that scan fails, the object renders
wrong or as a placeholder.

**Plan:** Reproduce with a mounted M2HB. Confirm whether the model id resolves
and whether the mesh renders at the right place/orientation. Fix the confirmed
cause (model-token resolution, placeholder fallback, or transform).

---

## Out of Scope

- No auto-refactoring of unrelated code.
- Issue 2 "add to .res" does not build models from scratch — it only copies
  existing model resources from a located source archive.
- No changes to in-game behavior beyond what the editor writes to level/`.res`
  files.

## Success Criteria

1. Editing a GunPickup weapon enum updates the rendered model immediately (no
   reload). — *Issue 1*
2. Hovering terrain shows the correct terrain id + `Add Terrain: <id>` hint. —
   *Issue 3*
3. Choosing a model absent from the level `.res` warns the user and offers to add
   it; accepting writes a valid `.res` that resolves the model. — *Issue 2*
4. Shift+M toggles magic-object spheres on a current build (verified, or fixed if
   broken). — *Issue 6*
5. Level 12 zipline wire keeps a proportionate cross-section inside the house;
   cable-car track tiles unaffected. — *Issue 5*
6. Door-frame texture renders cleanly. — *Issue 4*
7. Mounted M2HB / stationary guns render correctly. — *Issue 7*
