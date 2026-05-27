# Known Issues and Engine Limitations

This document tracks all known rendering bugs, engine limitations, and editor work-in-progress features for the Project IGI Editor.

---

## 🏔️ 1. Rendering and Model Issues

### 👤 Level 7: PATROL_AK Soldier (Model `003_01_1`) Mesh Artifacts
* **Symptom**: The `PATROL_AK` Soldier unit (Model ID `003_01_1`) is not drawn properly or appears with missing mesh segments/distorted polygons in the 3D viewport.
* **Root Cause**: This soldier is bound specifically to an introductory/in-game cutscene. Because it lacks standard texture mapping coordinates within the loaded level's material buffers, the renderer is unable to bind and project textures onto the bone mesh successfully.

### 🚪 Levels 12, 13, 14: Metal Slide-Up Door Orientation Mismatch
* **Symptom**: Slide-up metal doors (Model ID `506_01_1`, `METAL_DOOR_SLIDE_UP`) appear in the viewport or log files with mismatched orientations or snapped axes.
* **Root Cause**: The physical game levels register these doors as standard `"EditRigidObj"` tasks rather than actual `"Door"` tasks. Because they are defined as rigid objects, they bypass the special hinged/sliding door transformation matrix handling, resulting in incorrect rotation interpretations between the engine's loader and the editor.

---

## 🛤️ 2. Splines and Waypoints

### 🚂 Levels 1, 9, 10: Jagged Train Track Spline Segments on Bridges
* **Symptom**: Train track paths do not draw smooth curved spline segments. The segment lines appear highly jagged or disjointed, particularly when crossing bridge structures.
* **Root Cause**: Spline track rendering over complex geometries requires dynamic Hermite/Bezier interpolation. Currently, bridge transitions do not trigger the necessary interpolation densities, causing straight-line segments to bridge the gap and look disjointed.

---

## 🎮 3. Game Engine Stability

### 💥 AnimationTask and Cutscene Modifications Crash
* **Symptom**: Modifying `AnimationTask` parameters, camera nodes, or cutscene objectives inside the Task Tree triggers a complete game crash (`igi.exe` Access Violation) when loading the mission.
* **Root Cause**: The Project IGI engine utilizes highly rigid binary offsets and bytecode structures to map cutscene camera animations and timeline events. Any ad-hoc changes made in the compiled QVM directly invalidate these hardcoded sequence offsets, causing the game's virtual machine to dereference invalid memory.

---

## 📢 4. Reporting Other Bugs and Issues

If you encounter any other bugs, crashes, or rendering issues that are not documented here, please feel free to report them to the dev team!

* **🎮 Discord**: Message us directly at `Jones_IGI#3954` or join the modding community on the [Project IGI Discord Server](https://discord.com/invite/QpbQrRFAER).
* **📧 Email**: [igiproz.hm@gmail.com](mailto:igiproz.hm@gmail.com)
* **🌟 GitHub**: Create a detailed issue on the [project-igi-editor GitHub Repository](https://github.com/Jones-HM/project-igi-editor/issues) with reproduction steps and logs.
