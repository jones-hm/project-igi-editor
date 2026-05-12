# Changelogs

## ALPHA 0.0.1

### QSC Save Logic Improvements
- **Added `original_rot` tracking**: Store original rotation values from QSC during load to enable proper change detection
- **Change detection on save**: Only update objects whose position or rotation has genuinely changed compared to their originally loaded state
- **Terrain snap offset handling**: Added `snap_z_offset` field to track Z offset added by terrain snapping, subtracted during save to match game coordinates
- **Multi-line Building support**: Fixed `Task_New` line replacement for Buildings with child tasks (no closing paren on first line)
- **Snap-to-terrain sync**: After snapping objects to terrain, sync `original_pos.z` so snap-only changes don't count as user edits
- **Preserved formatting**: Maintain original line indentation and terminators (`;` or `,`) for multi-line blocks

### Key Fixes
- Fixed issue where all `EditRigidObj` entries were being rewritten after reset and save
- Fixed Z-axis position discrepancy between editor view and in-game (editor was saving snapped Z, game expects raw terrain Z)
- Fixed compiler errors caused by incorrect multi-line `Task_New` termination
- Improved float formatting with `%.10g` to preserve precision and avoid scientific notation

### Technical Details
- Modified `LevelObject` struct to include `original_rot` and `snap_z_offset` fields
- Updated `LevelObjects::Load()` to capture original rotation values from QSC arguments
- Updated `App::SnapObjectsToTerrain()` to store snap offset and sync original Z
- Updated `LevelObjects::SaveToQSC()` with comprehensive change detection logic
- Added detailed logging for skip/save operations during QSC save

### Compilation
- Successfully builds with MSVC 2022 (Visual Studio)
- No breaking changes to existing functionality
