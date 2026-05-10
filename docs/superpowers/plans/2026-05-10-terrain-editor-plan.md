# Terrain Editor: Height Sculpting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform the Project-IGI-Terrain viewer into a basic terrain editor with real-time height modification.

**Architecture:** Adds `edit_mode_` and `edit_brush_` states to `App`. Implements Raycasting from screen space through the mouse cursor into world space to find intersection with the terrain. Mutates the height map binary data in-memory directly and relies on the next frame's vertex buffer generation to display the changes visually.

**Tech Stack:** C++, OpenGL, GLUT, GLM

---

### Task 1: Editor State and UI in App

**Files:**
- Modify: `source/app.h`
- Modify: `source/app.cpp`
- Modify: `source/main.cpp`

- [ ] **Step 1: Add editor state variables to `app.h`**

In `source/app.h`, at the public section, add getter/setter methods. Add the `edit_mode_` and `edit_brush_` states in the private section.

```cpp
	// editor
	void					ToggleEditMode();
	bool					GetEditMode() const;
	void					SetEditBrush(int brush);
	int						GetEditBrush() const;
```

```cpp
	int						terrain_mod_options_;

	// editor
	bool					edit_mode_;
	int						edit_brush_;

	int64_t					prior_frame_time_;
```

- [ ] **Step 2: Initialize editor state in `app.cpp`**

In `source/app.cpp` constructor:
```cpp
App::App() :
	terrain_mod_options_(-1),
	edit_mode_(false),
	edit_brush_(0), // 0: raise, 1: lower
	prior_frame_time_(0),
```

- [ ] **Step 3: Implement getters/setters in `app.cpp`**

At the bottom of `source/app.cpp`, or under the other Toggles:
```cpp
void App::ToggleEditMode() {
	edit_mode_ = !edit_mode_;
}

bool App::GetEditMode() const {
	return edit_mode_;
}

void App::SetEditBrush(int brush) {
	edit_brush_ = brush;
}

int App::GetEditBrush() const {
	return edit_brush_;
}
```

- [ ] **Step 4: Update GLUT context menu in `main.cpp`**

In `source/main.cpp`, add new menu constants at the top:
```cpp
// exit
constexpr int MENU_CLOSE = 61;

// editor
constexpr int MENU_EDITOR_TOGGLE = 71;
constexpr int MENU_EDITOR_BRUSH_RAISE = 72;
constexpr int MENU_EDITOR_BRUSH_LOWER = 73;

constexpr int BRUSH_RAISE = 0;
constexpr int BRUSH_LOWER = 1;
```

Add menu variable:
```cpp
static int g_menu_terrain_modifier_opts;
static int g_menu_editor_tools;
static int g_menu_choose_level;
```

Update menu updates:
```cpp
constexpr int UPDATE_MENU_TERRAIN_MODIFIER_OPTS = FLAG_BIT(3);
constexpr int UPDATE_MENU_CHOOSE_LEVEL = FLAG_BIT(4);
constexpr int UPDATE_MENU_EDITOR_TOOLS = FLAG_BIT(5);
```

Add `UpdateEditorToolsMenuText` declaration:
```cpp
static void UpdateTerrainModOptionsMenuText();
static void UpdateChooseLevelMenuText();
static void UpdateEditorToolsMenuText();
```

Call it in `OnIdle`:
```cpp
		if (g_update_menu_flags & UPDATE_MENU_CHOOSE_LEVEL) {
			UpdateChooseLevelMenuText();
		}
		if (g_update_menu_flags & UPDATE_MENU_EDITOR_TOOLS) {
			UpdateEditorToolsMenuText();
		}
```

Implement it:
```cpp
static void UpdateEditorToolsMenuText() {
	glutSetMenu(g_menu_editor_tools);

	if (g_app.GetEditMode()) {
		glutChangeToMenuEntry(1, "Toggle Edit Mode [+]", MENU_EDITOR_TOGGLE);
	}
	else {
		glutChangeToMenuEntry(1, "Toggle Edit Mode [-]", MENU_EDITOR_TOGGLE);
	}

	if (g_app.GetEditBrush() == BRUSH_RAISE) {
		glutChangeToMenuEntry(2, "Brush: Raise Terrain [*]", MENU_EDITOR_BRUSH_RAISE);
		glutChangeToMenuEntry(3, "Brush: Lower Terrain [ ]", MENU_EDITOR_BRUSH_LOWER);
	}
	else {
		glutChangeToMenuEntry(2, "Brush: Raise Terrain [ ]", MENU_EDITOR_BRUSH_RAISE);
		glutChangeToMenuEntry(3, "Brush: Lower Terrain [*]", MENU_EDITOR_BRUSH_LOWER);
	}
}
```

Add switch cases in `OnMenu`:
```cpp
	case MENU_TERRAIN_DISCARD_MOD:
		g_app.ToggleTerrainModOption(TERRAIN_DISCARD_MOD);
		g_update_menu_flags |= UPDATE_MENU_TERRAIN_MODIFIER_OPTS;
		break;
	case MENU_EDITOR_TOGGLE:
		g_app.ToggleEditMode();
		g_update_menu_flags |= UPDATE_MENU_EDITOR_TOOLS;
		break;
	case MENU_EDITOR_BRUSH_RAISE:
		g_app.SetEditBrush(BRUSH_RAISE);
		g_update_menu_flags |= UPDATE_MENU_EDITOR_TOOLS;
		break;
	case MENU_EDITOR_BRUSH_LOWER:
		g_app.SetEditBrush(BRUSH_LOWER);
		g_update_menu_flags |= UPDATE_MENU_EDITOR_TOOLS;
		break;
```

In `main()`:
```cpp
	g_menu_choose_level = glutCreateMenu(OnMenu);
	for (int i = MENU_LEVEL_FIRST; i <= MENU_LEVEL_LAST; ++i) {
		glutAddMenuEntry("", i);
	}

	g_menu_editor_tools = glutCreateMenu(OnMenu);
	glutAddMenuEntry("", MENU_EDITOR_TOGGLE);
	glutAddMenuEntry("", MENU_EDITOR_BRUSH_RAISE);
	glutAddMenuEntry("", MENU_EDITOR_BRUSH_LOWER);

	g_main_menu = glutCreateMenu(OnMenu);
	glutAddMenuEntry("", MENU_OVERLAY_WIREFRAME);
	glutAddSubMenu("Draw Parts", g_menu_draw_parts);
	glutAddSubMenu("Terrain Draw Options", g_menu_terrain_draw_opts);
	glutAddSubMenu("Terrain Mod Options", g_menu_terrain_modifier_opts);
	glutAddSubMenu("Editor Tools", g_menu_editor_tools);
	glutAddSubMenu("Choose Level", g_menu_choose_level);
```

Init menu text at end of `main`:
```cpp
	UpdateTerrainModOptionsMenuText();
	UpdateChooseLevelMenuText();
	UpdateEditorToolsMenuText();
```

- [ ] **Step 5: Build and Run**
Build the code. Ensure the right-click menu now has an "Editor Tools" sub-menu with working toggles.

---

### Task 2: Raycasting Implementation

**Files:**
- Modify: `source/app.h`
- Modify: `source/app.cpp`
- Modify: `source/level/level.h`
- Modify: `source/level/level.cpp`
- Modify: `source/level/terrain.h`
- Modify: `source/level/terrain.cpp`

- [ ] **Step 1: Disable camera movement in Edit Mode**

In `source/app.cpp` inside `ProcessInput`:
```cpp
void App::ProcessInput(float delta_seconds) {
	if (!edit_mode_) {
		viewer_.yaw_ += -input_.mouse_delta_x_ * MOUSE_SENSITIVE;
		viewer_.pitch_ += -input_.mouse_delta_y_ * MOUSE_SENSITIVE;
	}

	input_.mouse_delta_x_ = 0;
	input_.mouse_delta_y_ = 0;
```

- [ ] **Step 2: Add Raycasting method to `app.h` and `app.cpp`**

In `source/app.h`:
```cpp
	void					ProcessInput(float delta_seconds);
	void					UpdateViewerVectors();
	void					EditorProcessClick();
```

In `source/app.cpp` add `EditorProcessClick` call in `OnIdle`:
```cpp
void App::Frame(float delta_seconds) {
	frame_++;
	frame_ %= 0xFFFFFFFF;	// reserve value 0xFFFFFFFF (-1) for INVALID_FRAME

	ProcessInput(delta_seconds);
	
	if (edit_mode_ && mouse_state_.left_button_down_) {
		EditorProcessClick();
	}
```

Implementation of `EditorProcessClick` in `source/app.cpp`:
```cpp
#include <glm/ext/matrix_projection.hpp>

void App::EditorProcessClick() {
	if (!window_state_.viewport_width_ || !window_state_.viewport_height_) return;

	// View matrix
	glm::dmat4 view_matrix(
		view_define_.mat_rot_[0][0], view_define_.mat_rot_[1][0], view_define_.mat_rot_[2][0], 0.0,
		view_define_.mat_rot_[0][1], view_define_.mat_rot_[1][1], view_define_.mat_rot_[2][1], 0.0,
		view_define_.mat_rot_[0][2], view_define_.mat_rot_[1][2], view_define_.mat_rot_[2][2], 0.0,
		0.0, 0.0, 0.0, 1.0
	);
	
	glm::dmat4 trans = glm::translate(glm::dmat4(1.0), glm::dvec3(-view_define_.pos_.x, -view_define_.pos_.y, -view_define_.pos_.z));
	view_matrix = view_matrix * trans;

	// Projection matrix
	glm::dmat4 proj_matrix = glm::perspective(
		(double)view_define_.fovy_,
		(double)window_state_.viewport_width_ / (double)window_state_.viewport_height_,
		(double)view_define_.z_near_,
		(double)view_define_.z_far_
	);

	glm::dvec4 viewport(0.0, 0.0, (double)window_state_.viewport_width_, (double)window_state_.viewport_height_);
	
	double winX = (double)mouse_state_.prior_x_;
	double winY = (double)window_state_.viewport_height_ - (double)mouse_state_.prior_y_;
	
	glm::dvec3 start_pos = glm::unProject(glm::dvec3(winX, winY, 0.0), view_matrix, proj_matrix, viewport);
	glm::dvec3 end_pos = glm::unProject(glm::dvec3(winX, winY, 1.0), view_matrix, proj_matrix, viewport);
	
	glm::vec3 ray_origin = start_pos;
	glm::vec3 ray_dir = glm::normalize(end_pos - start_pos);

	level_.EditorRaycastAndModify(ray_origin, ray_dir, edit_brush_);
}
```

- [ ] **Step 3: Define `EditorRaycastAndModify` in `level.h` and `level.cpp`**

In `source/level/level.h`:
```cpp
	bool					GetTerrainZ(const glm::vec3 & pos, float & z);
	void					EditorRaycastAndModify(const glm::vec3& ray_origin, const glm::vec3& ray_dir, int brush_type);
```

In `source/level/level.cpp`:
```cpp
void Level::EditorRaycastAndModify(const glm::vec3& ray_origin, const glm::vec3& ray_dir, int brush_type) {
	if (root_dyn_cube_) {
		terrain_.EditorRaycastAndModify(root_dyn_cube_, ray_origin, ray_dir, brush_type);
	}
}
```

- [ ] **Step 4: Define `EditorRaycastAndModify` in `terrain.h` and `terrain.cpp`**

In `source/level/terrain.h`:
```cpp
	struct height_map_s {
		int					height_map_line_width_shift_;
		float				local_pos_to_hmp_pos_;
		double				cube_min_x_;
		double				cube_min_y_;
		int8_t*				height_map_item_; // REMOVED CONST
	};
```

Change `height_map_items_` to non-const:
```cpp
	int8_t *				height_map_items_[MAX_HMP];
```

Add method in `terrain.h`:
```cpp
	bool					GetZ(const dyn_cube_s* root_dyn_cube, const glm::vec3 & pos, float & ret_z);
	void					EditorRaycastAndModify(const dyn_cube_s* root_dyn_cube, const glm::vec3& ray_origin, const glm::vec3& ray_dir, int brush_type);
```

In `source/level/terrain.cpp`, update type casts where `height_map_items_` is populated:
```cpp
			height_map_items_[i] = (int8_t*)cur_body; // line 505
```

And in `CalcHMPDeltaZ`:
```cpp
	const uint8_t * height_map_item = (const uint8_t*)height_map->height_map_item_;
```

- [ ] **Step 5: Implement `Terrain::EditorRaycastAndModify`**

In `source/level/terrain.cpp` add the implementation. Because doing a full polygon raycast on the octree is extremely complex given the mesh generation code, we will use a simplified Raymarch. Since the max height difference is limited, we step along the ray and check `GetZ`.

```cpp
void Terrain::EditorRaycastAndModify(const dyn_cube_s* root_dyn_cube, const glm::vec3& ray_origin, const glm::vec3& ray_dir, int brush_type) {
	// Basic Raymarch
	glm::vec3 current_pos = ray_origin;
	float step_size = 100.0f; // 100 units per step
	bool hit = false;
	
	for (int i = 0; i < 5000; ++i) { // max 500,000 units distance
		current_pos += ray_dir * step_size;
		
		float terrain_z = 0.0f;
		if (GetZ(root_dyn_cube, current_pos, terrain_z)) {
			if (current_pos.z <= terrain_z) {
				hit = true;
				break;
			}
		}
	}
	
	if (!hit) return;

	// We have an approximate hit point on the terrain (current_pos.x, current_pos.y)
	// Now we need to modify the height map at this location.
	// Iterate over all active height maps to find which one covers this X/Y area.
	
	float radius = 5000.0f; // Brush radius
	float strength = 1.0f; // Height map values are ints, so 1 unit of change
	
	for (int i = 0; i < num_height_map_; ++i) {
		height_map_s* hmp = height_maps_ + qtask_height_maps_[i].idx_in_array_;
		
		int hmp_dim = 1 << hmp->height_map_line_width_shift_;
		
		// Find world coordinate bounds for this height map
		double min_x = hmp->cube_min_x_;
		double min_y = hmp->cube_min_y_;
		double max_x = min_x + (hmp_dim / hmp->local_pos_to_hmp_pos_);
		double max_y = min_y + (hmp_dim / hmp->local_pos_to_hmp_pos_);
		
		// Does the brush intersect this height map bounding box?
		if (current_pos.x + radius < min_x || current_pos.x - radius > max_x ||
			current_pos.y + radius < min_y || current_pos.y - radius > max_y) {
			continue;
		}
		
		// Modify pixels in the height map
		for (int y = 0; y <= hmp_dim; ++y) {
			for (int x = 0; x <= hmp_dim; ++x) {
				double world_x = min_x + x / hmp->local_pos_to_hmp_pos_;
				double world_y = min_y + y / hmp->local_pos_to_hmp_pos_;
				
				float dx = current_pos.x - world_x;
				float dy = current_pos.y - world_y;
				float dist_sq = dx*dx + dy*dy;
				
				if (dist_sq <= radius * radius) {
					int idx = y * (hmp_dim + 1) + x;
					int current_val = hmp->height_map_item_[idx];
					
					if (brush_type == 0) { // BRUSH_RAISE
						current_val += 1; // Increase height
					} else {
						current_val -= 1; // Decrease height
					}
					
					// Clamp to int8 bounds
					if (current_val > 127) current_val = 127;
					if (current_val < -128) current_val = -128;
					
					hmp->height_map_item_[idx] = (int8_t)current_val;
				}
			}
		}
	}
	
	// Force the terrain engine to rebuild the chunks since data changed
	ClearCubeDataHash();
}
```

- [ ] **Step 6: Build and Verify**
Build the executable (`cmake --build . --config Release`). Run `-level 6`. Turn on Wireframe (F2) and Height Maps via Context Menu. Toggle Edit Mode. Drag the mouse on the terrain and verify it dynamically raises or lowers the ground where the height map is active.
