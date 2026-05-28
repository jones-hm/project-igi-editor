# GPU Color Picking — Design Spec
**Date:** 2026-05-29  
**Status:** Approved

---

## Problem

The existing `App::PickObjectAtScreenPos()` uses CPU-side projected AABB bounding boxes with a 6-pixel margin. This is not pixel-perfect: hovering anywhere near an object's bounding box triggers it, large empty bounding boxes cause false positives, and thin/irregular models are unreliable. Additionally, objects inside buildings are selectable even when the camera is outside, exposing interior state that should be hidden.

---

## Goals

1. **Pixel-perfect hover/selection** — only the exact rendered pixels of a mesh surface trigger a hit.
2. **Building interior occlusion** — objects parented to a building are invisible to the picker when the camera is outside that building. They become pickable only when the camera enters.

---

## Architecture

All picking logic lives in `Renderer_Objects`. The class gains:

| Addition | Purpose |
|---|---|
| `pick_fbo_`, `pick_color_tex_`, `pick_depth_rb_` | Off-screen framebuffer + attachments |
| `pick_shader_program_` | Flat-color picking shader |
| `pick_fbo_w_`, `pick_fbo_h_` | Track FBO dimensions for resize detection |
| `DrawForPicking(ubo_mats, objects, draw_parts, camera_pos)` | Renders scene to picking FBO |
| `PickObjectAtScreen(x, y, w, h, ubo_mats, objects, draw_parts, camera_pos) → int` | Full pick: render + readback + decode |

`App::PickObjectAtScreenPos()` is replaced with a one-liner delegating to `renderer_.PickObjectAtScreen(...)`.

---

## ID Encoding

- **ID 0** = background (no object)
- **ID = index + 1** for each `LevelObject`

Packed into RGB8:
```
R = (id >> 16) & 0xFF
G = (id >>  8) & 0xFF
B =  id        & 0xFF
```
Supports up to 16,777,215 unique IDs. Decoding: `id = (R<<16)|(G<<8)|B`.

---

## Picking Shader

**Vertex shader** — identical transforms to `OBJ_VERT_SRC` (UBO MVP + `u_model` uniform). No changes needed.

**Fragment shader** — minimal:
```glsl
#version 330 core
uniform int u_object_id;
out vec4 fragColor;
void main() {
    int id = u_object_id;
    fragColor = vec4(
        float((id >> 16) & 0xFF) / 255.0,
        float((id >>  8) & 0xFF) / 255.0,
        float( id        & 0xFF) / 255.0,
        1.0
    );
}
```

No textures, no lighting, no alpha discard. All pixels of a mesh write the same flat color.

---

## ATTA Sub-model Handling

ATTA sub-models (attached child meshes) are rendered in the picking pass with the **same ID as their top-level parent object**. This means clicking any part of a compound model (e.g. a building with attached window frames) selects the whole top-level object, not a sub-mesh.

---

## Building Interior Occlusion

### Rule
> If camera is outside a building, that building's interior child objects are invisible to the picker. Objects become pickable only when the camera is inside.

### Implementation

Before `DrawForPicking()` iterates objects, build:
```cpp
std::unordered_set<int> inside_buildings;
```
For each building object: test whether the camera world position is inside its 3D AABB:
- AABB center = `obj.pos` (world space)
- AABB half-extents = `GetMeshExtents(obj.modelId, true) * BASE_SCALE`

During the picking render loop:
- **Buildings**: always rendered (always pickable).
- **Objects with `parentIndex != -1` pointing to a building**: skip unless that building index is in `inside_buildings`.
- **Objects with no building parent** (free-standing props, AI, etc.): always rendered.

---

## FBO Lifecycle

| Event | Action |
|---|---|
| `Renderer_Objects::Init()` | Create FBO at size 1×1 (resized on first use) |
| Start of `DrawForPicking()` | If `w != pick_fbo_w_` or `h != pick_fbo_h_`, delete and recreate FBO |
| `Renderer_Objects::Shutdown()` | Delete FBO, textures, renderbuffers |

FBO attachments:
- Color: `GL_TEXTURE_2D`, internal format `GL_RGB8`
- Depth: `GL_RENDERBUFFER`, internal format `GL_DEPTH_COMPONENT24`

---

## Render State for Picking Pass

```
glBindFramebuffer(GL_FRAMEBUFFER, pick_fbo_)
glClearColor(0, 0, 0, 1)   // ID 0 = background
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
glEnable(GL_DEPTH_TEST)
glDepthFunc(GL_LESS)
glDepthMask(GL_TRUE)
glEnable(GL_CULL_FACE)
glCullFace(GL_BACK)
glDisable(GL_BLEND)
```

After rendering: `glBindFramebuffer(GL_FRAMEBUFFER, 0)` to restore default.

---

## Pixel Readback

```cpp
uint8_t pixel[3] = {0, 0, 0};
glBindFramebuffer(GL_FRAMEBUFFER, pick_fbo_);
glReadPixels(x, viewport_height - y - 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
glBindFramebuffer(GL_FRAMEBUFFER, 0);
int id = (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];
return (id == 0) ? -1 : id - 1;
```

Note: Y is flipped (`viewport_height - y - 1`) because OpenGL origin is bottom-left, screen coords are top-left.

---

## Performance

- `DrawForPicking()` runs only when mouse position changes (gated in `App::OnMouseMove`).
- Picking render: no textures, no lighting — significantly cheaper than the main render pass.
- Readback: `glReadPixels` for 1 pixel = 3 bytes. Negligible cost.
- FBO resize: only happens when window is resized.

---

## Files Changed

| File | Change |
|---|---|
| `source/renderer/renderer_objects.h` | Add FBO members, `DrawForPicking()`, `PickObjectAtScreen()` declarations |
| `source/renderer/renderer_objects.cpp` | Add picking shader source, FBO setup/teardown, `DrawForPicking()`, `PickObjectAtScreen()` |
| `source/app.cpp` | Replace `PickObjectAtScreenPos()` body with `renderer_.PickObjectAtScreen(...)` |
| `source/app.h` | Remove `PickObjectAtScreenPos()` declaration if it becomes a thin wrapper |

---

## Out of Scope

- Picking terrain pixels (existing terrain-ID display is separate)
- Multi-select / rubber-band selection
- Sub-mesh level selection (always selects top-level object)
