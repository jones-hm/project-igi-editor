#include "app_internal.h"

// GameMonitorParam, GameMonitorProc, and HOTKEY_ID_TOGGLE_GAME live in
// app_internal.h (shared with app_editor.cpp's LaunchGame). The mutable window
// subclass globals below are used only here.

// ── Global hotkey support ────────────────────────────────────────────────────
// We subclass GLUT's window so WM_HOTKEY messages reach our code even when
// the editor is iconified and the game has keyboard focus.
static WNDPROC g_origEditorWndProc = nullptr;
static App*    g_appForHotkey      = nullptr;

static LRESULT CALLBACK EditorSubclassWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_HOTKEY && static_cast<int>(wParam) == HOTKEY_ID_TOGGLE_GAME) {
		Logger::Get().Log(LogLevel::INFO, "[ToggleGame] Global hotkey fired");
		if (g_appForHotkey) g_appForHotkey->LaunchGame();
		return 0;
	}
	return CallWindowProc(g_origEditorWndProc, hwnd, msg, wParam, lParam);
}

/*
================================================================================
 App
================================================================================
*/

App::App():
	frame_(0),
	terrain_mod_options_(-1),
	edit_mode_(true), // Enable by default as requested
	terrain_edit_enabled_(false),
	pause_mode_(false),
	edit_brush_(0), // 0: raise, 1: lower
	selected_object_index_(0),
	hover_object_index_(-1),
	show_hud_(true),
	show_debug_(false),
	show_help_(false),
	show_magic_obj_spheres_(false),
	tree_scroll_offset_(0),
	tree_decl_expanded_(false),
	status_message_(),
	noclip_mode_(true), // By default true as requested by user
	prior_frame_time_(0),
	skip_input_on_motion_once_(false)
{
	view_define_.pos_ = glm::vec3(0.0f);
	view_define_.forward_ = VEC3_Y_DIR;
	view_define_.right_ = VEC3_X_DIR;
	view_define_.up_ = VEC3_Z_DIR;
	view_define_.fovx_ = glm::radians(FOVY_IN_DEGREE);
	view_define_.fovy_ = glm::radians(FOVY_IN_DEGREE);
	view_define_.render_z_near_ = RENDER_Z_NEAR;
	view_define_.render_z_far_ = RENDER_Z_FAR;
	view_define_.render_min_depth_ = RENDER_DEPTH_MIN;
	view_define_.render_max_depth_ = RENDER_DEPTH_MAX;
	view_define_.viewport_width_ = 1;
	view_define_.viewport_height_ = 1;

	draw_params_.view_define_ = &view_define_;
	draw_params_.overlay_wireframe_ = false;
	draw_params_.draw_parts_ = -1;
	draw_params_.draw_terrain_options_ = -1;
	draw_params_.flat_sky_layer_is_visible_ = true;
	draw_params_.num_terrain_render_chunk_ = 0;
	draw_params_.selected_object_index_ = -1;

	memset(&window_state_, 0, sizeof(window_state_));
	memset(&mouse_state_, 0, sizeof(mouse_state_));
	memset(&input_, 0, sizeof(input_));

	window_state_.cursor_visible_ = true;

	memset(&viewer_, 0, sizeof(viewer_));
	viewer_.clip_to_z_ = false;
	viewer_.move_speed_ = MIN_MOVE_SPEED;
	viewer_.jump_speed_ = MIN_JUMP_SPEED;
	window_state_.cursor_visible_ = true;
}

App::~App() {
	Shutdown();
}

bool App::Init(int argc, char** argv) {
	// Initialize logger with absolute path to exe directory
	std::string exeDir = Utils::GetExeDirectory();
	Logger::Get().Init(exeDir + "\\igi1ed.log");
	Logger::Get().Log(LogLevel::INFO, "IGI Editor Initializing...");

	if (!renderer_.Init()) {
		return false;
	}

	ConfigData& cfg = Config::Get();


	// read options from command line
	draw_params_.overlay_wireframe_ = Arg_OptionIdx(argc, argv, "-wireframe") > 0;
	draw_params_.draw_parts_ = Arg_ReadInt(argc, argv, "-draw_parts", -1);
	draw_params_.draw_terrain_options_ = Arg_ReadInt(argc, argv, "-draw_terrain_opts", -1);
	terrain_mod_options_ = Arg_ReadInt(argc, argv, "-terrain_mod_opts", terrain_mod_options_);
	stick_to_ground_ = Arg_OptionIdx(argc, argv, "-stick_to_ground") > 0;

	int start_level = Arg_ReadInt(argc, argv, "-level", cfg.level);
	if (start_level >= MIN_LEVEL_NO && start_level <= MAX_LEVEL_NO) {
		try {
			LoadLevel(start_level);
		}
		catch (const std::exception& e) {
			std::string errorMsg = "Failed to load level " + std::to_string(start_level) + ":\n" + std::string(e.what());
			Utils::LogAndShowError(errorMsg, "IGI Editor - Error");
			Logger::Get().Log(LogLevel::ERR, errorMsg);
		}
		catch (...) {
			std::string errorMsg = "Failed to load level " + std::to_string(start_level) + ":\nUnknown error occurred.";
			Utils::LogAndShowError(errorMsg, "IGI Editor - Error");
			Logger::Get().Log(LogLevel::ERR, errorMsg);
		}
	}


	if (Arg_OptionIdx(argc, argv, "-yaw") > -1) {
		// override yaw
		viewer_.yaw_ = Arg_ReadFloat(argc, argv, "-yaw", 0.0f);
		UpdateViewerVectors();
	}

	if (Arg_OptionIdx(argc, argv, "-pitch") > -1) {
		// override pitch
		viewer_.pitch_ = Arg_ReadFloat(argc, argv, "-pitch", 0.0f);
		UpdateViewerVectors();
	}

	int wnd_w = Arg_ReadInt(argc, argv, "-w", 800);
	int wnd_h = Arg_ReadInt(argc, argv, "-h", 600);
	OnWindowResize(wnd_w, wnd_h);

	prior_frame_time_ = Sys_Milliseconds();

	bridge_.SetEnabled(show_hud_);
	bridge_.Start();


	// Set initial cursor state
	LoadAllCursors();
	LoadHelpEntries();
	LoadAutoCompleteKeywords();
	glutSetCursor(GLUT_CURSOR_NONE);

	// Cache editor HWND for minimize/restore around game launch
	editor_hwnd_ = Utils::FindWindow("IGI Editor");
	if (!editor_hwnd_) editor_hwnd_ = GetActiveWindow();

	// Subclass GLUT's window so WM_HOTKEY messages reach EditorSubclassWndProc
	// even when the editor is iconified and the game holds keyboard focus.
	if (editor_hwnd_) {
		g_appForHotkey     = this;
		g_origEditorWndProc = reinterpret_cast<WNDPROC>(
			SetWindowLongPtr(editor_hwnd_, GWLP_WNDPROC,
			                 reinterpret_cast<LONG_PTR>(EditorSubclassWndProc)));
		Logger::Get().Log(LogLevel::INFO, "[App] Editor window subclassed for global hotkey (HWND=" +
		                  std::to_string(reinterpret_cast<uintptr_t>(editor_hwnd_)) + ")");
	} else {
		Logger::Get().Log(LogLevel::WARNING, "[App] editor_hwnd_ is NULL — global hotkey will not work");
	}

	return true;
}

void App::Shutdown() {
	if (game_process_.running) {
		// Wait briefly for monitor thread (it's blocking on the game process handle)
		if (game_process_.hMonitorThread) {
			WaitForSingleObject(game_process_.hMonitorThread, 500);
			CloseHandle(game_process_.hMonitorThread);
		}
		CloseHandle(game_process_.hProcess);
		CloseHandle(game_process_.hThread);
		game_process_ = {};
	}
	bridge_.Stop();
	level_.Unload();
	level_.FreeTerrainCubeDataPools();
	renderer_.Shutdown();
	if (!g_isCLIMode) {
		AssetExtractor::CleanupExtractedAssets(Utils::GetExeDirectory());
	}
}

// ── C1: Custom SPR cursor — multi-mode ────────────────────────────────────────

static GLuint LoadOneSpr(const char* path, int& w, int& h) {
	TEXFile tex = TEX_Parse(path);
	if (!tex.valid || tex.images.empty()) return 0;
	const TEXImage& img = tex.images[0];

	std::vector<uint8_t> rgba;
	if (img.mode == 2) { // RGB565 — no alpha channel; chroma-key dark pixels as transparent
		rgba.reserve(img.width * img.height * 4);
		for (size_t i = 0; i + 1 < img.pixels.size(); i += 2) {
			uint16_t p = img.pixels[i] | ((uint16_t)img.pixels[i + 1] << 8);
			uint8_t r = ((p >> 11) & 0x1F) << 3;
			uint8_t g = ((p >> 5)  & 0x3F) << 2;
			uint8_t b =  (p        & 0x1F) << 3;
			// Chroma-key: near-black pixels become transparent background
			uint8_t a = (r < 30 && g < 30 && b < 30) ? 0 : 255;
			rgba.push_back(r);
			rgba.push_back(g);
			rgba.push_back(b);
			rgba.push_back(a);
		}
	} else { // ARGB8888 (modes 3, 67) — swizzle BGRA → RGBA
		rgba.resize(img.pixels.size());
		for (size_t i = 0; i + 3 < img.pixels.size(); i += 4) {
			rgba[i + 0] = img.pixels[i + 2]; // R ← B
			rgba[i + 1] = img.pixels[i + 1]; // G
			rgba[i + 2] = img.pixels[i + 0]; // B ← R
			rgba[i + 3] = img.pixels[i + 3]; // A
		}
	}

	pic_s pic;
	pic.width_  = (int)img.width;
	pic.height_ = (int)img.height;
	pic.pixels_ = rgba.data();
	w = (int)img.width;
	h = (int)img.height;
	return GL_RegisterTexture(&pic, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, false);
}

void App::LoadAllCursors() {
	// Order must match CursorMode enum values (0..9)
	const char* paths[NUM_CURSORS] = {
		"editor\\qed\\TerrainEditIcon_Pointer.spr",     // 0 Default
		"editor\\qed\\highlighttool.spr",               // 1 Hover
		"editor\\qed\\activetool.spr",                  // 2 Selected
		"editor\\qed\\TerrainEditIcon_Lift.spr",        // 3 TerrainLift
		"editor\\qed\\TerrainEditIcon_Lower.spr",       // 4 TerrainLower
		"editor\\qed\\TerrainEditIcon_Flatten.spr",     // 5 TerrainFlatten
		"editor\\qed\\TerrainEditIcon_FlattenLine.spr", // 6 TerrainFlattenLine
		"editor\\qed\\TerrainEditIcon_Drop.spr",        // 7 TerrainDrop
		"editor\\qed\\TerrainEditIcon_Soften.spr",      // 8 TerrainSoften
		"editor\\qed\\inactivetool.spr",                // 9 Inactive
		"editor\\qed\\editor_camera.spr",               // 10 Camera (ALT held)
		"editor\\qed\\editor_move.spr",                 // 11 Move (ALT held + moving)
	};
	cursor_loaded_count_ = 0;
	for (int i = 0; i < NUM_CURSORS; ++i) {
		cursor_tex_ids_[i] = LoadOneSpr(paths[i], cursor_tex_ws_[i], cursor_tex_hs_[i]);
		if (cursor_tex_ids_[i]) cursor_loaded_count_++;
	}

	// Cache AITYPE_ model IDs from IGIModels.json
	ai_model_ids_.clear();
	{
		std::string jsonPath = Utils::GetExeDirectory() + "\\editor\\tools\\IGIModels.json";
		std::ifstream jf(jsonPath);
		if (jf) {
			std::string content((std::istreambuf_iterator<char>(jf)), {});
			size_t pos = 0;
			while ((pos = content.find("\"AITYPE_", pos)) != std::string::npos) {
				size_t idPos = content.find("\"ModelId\"", pos);
				if (idPos != std::string::npos && idPos - pos < 200) {
					size_t q1 = content.find('"', idPos + 9);
					size_t q2 = (q1 != std::string::npos) ? content.find('"', q1 + 1) : std::string::npos;
					if (q1 != std::string::npos && q2 != std::string::npos)
						ai_model_ids_.insert(content.substr(q1 + 1, q2 - q1 - 1));
				}
				pos++;
			}
		}
		Logger::Get().Log(LogLevel::INFO, "[App] Loaded " + std::to_string(ai_model_ids_.size()) + " AITYPE_ model IDs");
	}
}

void App::LoadHelpEntries() {
	help_entries_.clear();
	std::vector<std::string> paths = {
		Utils::GetExeDirectory() + "\\editor\\qed\\qedkeybindings.qsc",
		Utils::GetExeDirectory() + "\\qed\\qedkeybindings.qsc",
		Utils::GetIGIRootPath() + "\\editor\\qed\\qedkeybindings.qsc",
		Utils::GetIGIRootPath() + "\\qed\\qedkeybindings.qsc",
		"editor\\qed\\qedkeybindings.qsc",
		"qed\\qedkeybindings.qsc"
	};
	std::ifstream f;
	for (const auto& p : paths) {
		f.open(p);
		if (f.is_open()) {
			Logger::Get().Log(LogLevel::INFO, "[App] Loaded keybindings from: " + p);
			break;
		}
		f.clear();
	}
	if (!f.is_open()) {
		Logger::Get().Log(LogLevel::WARNING, "[App] Could not load help keybindings file: qedkeybindings.qsc");
		return;
	}
	std::string line;
	while (std::getline(f, line)) {
		// Strip leading whitespace
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) continue;
		line = line.substr(start);
		if (line.empty() || line[0] == '/' ) continue; // skip blank/comment lines
		// Keep SetEventBinding lines; format them as "  Key  =>  Event"
		if (line.find("SetEventBinding") == 0 || line.find("QEDSetEventBinding") == 0) {
			// extract: SetEventBinding("EventName", "KeyCombo") or QEDSetEventBinding(...)
			size_t q1 = line.find('"');
			size_t q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
			size_t q3 = (q2 != std::string::npos) ? line.find('"', q2 + 1) : std::string::npos;
			size_t q4 = (q3 != std::string::npos) ? line.find('"', q3 + 1) : std::string::npos;
			if (q1 != std::string::npos && q2 != std::string::npos &&
			    q3 != std::string::npos && q4 != std::string::npos) {
				std::string evtName = line.substr(q1 + 1, q2 - q1 - 1);
				std::string keyCombo = line.substr(q3 + 1, q4 - q3 - 1);
				// Replace angle-bracket modifiers for readability
				auto repl = [](std::string s, const std::string& from, const std::string& to) {
					size_t p = 0;
					while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
					return s;
				};
				keyCombo = repl(keyCombo, "<Alt>", "Alt+");
				keyCombo = repl(keyCombo, "<Ctrl>", "Ctrl+");
				keyCombo = repl(keyCombo, "<Control>", "Ctrl+");
				keyCombo = repl(keyCombo, "<Shift>", "Shift+");
				keyCombo = repl(keyCombo, "<", "");
				keyCombo = repl(keyCombo, ">", "");
				help_entries_.push_back(keyCombo + "  =>  " + evtName);
			}
		}
	}
}

void App::LoadAutoCompleteKeywords() {
	autocomplete_keywords_.clear();
	std::string path = Utils::GetExeDirectory() + "\\editor\\tools\\IGIAutoComplete.txt";
	std::ifstream f(path);
	if (!f.is_open()) {
		// try IGI root fallback
		path = Utils::GetIGIRootPath() + "\\editor\\tools\\IGIAutoComplete.txt";
		f.open(path);
	}
	if (!f.is_open()) { Logger::Get().Log(LogLevel::WARNING, "[App] IGIAutoComplete.txt not found"); return; }
	std::string line;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (!line.empty()) autocomplete_keywords_.push_back(line);
	}
	Logger::Get().Log(LogLevel::INFO, "[App] Loaded " + std::to_string(autocomplete_keywords_.size()) + " autocomplete keywords");
}

void App::SaveTaskSubtreeToFile(int idx, const std::string& path) {
	auto& objects = level_.GetLevelObjects().GetObjects();
	if (idx < 0 || idx >= (int)objects.size()) { status_message_ = "Save task: no task selected"; return; }
	// Serialize ONLY the selected task + its descendants as a proper nested QSC block.
	// (Previously this wrote each object's raw qscLine, but a parent's qscLine holds the
	//  whole original nested block, so it exported the entire object.)
	level_.GetLevelObjects().SaveSubtreeToQSC(idx, path);
	status_message_ = "Saved task to: " + path;
	Config::Get().taskFileName = path;
}

void App::ConfirmFileDialog() {
	std::string path = file_dialog_path_;
	FileDialogMode mode = file_dialog_mode_;
	file_dialog_mode_ = FileDialogMode::None;
	if (mode != FileDialogMode::SaveObjectFile)
		Config::Get().taskFileName = path; // remember last sub-task path only

	if (mode == FileDialogMode::SaveObjectFile) {
		// Ctrl+S: write the live objects QSC to the user-specified path.
		level_.GetLevelObjects().SaveToQSC(path);
		status_message_ = "Saved objects file: " + path;
	} else if (mode == FileDialogMode::SaveSubTask) {
		SaveTaskSubtreeToFile(selected_object_index_, path);
	} else if (mode == FileDialogMode::SaveSubTaskParent) {
		auto& objects = level_.GetLevelObjects().GetObjects();
		if (selected_object_index_ >= 0) {
			int par = objects[selected_object_index_].parentIndex;
			if (par >= 0) SaveTaskSubtreeToFile(par, path);
		}
	} else if (mode == FileDialogMode::LoadSubTask) {
		std::ifstream f(path);
		if (!f.is_open()) { status_message_ = "Error: cannot read " + path; return; }
		std::string block((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		f.close();
		block = Utils::Trim(block);
		if (block.empty()) { status_message_ = "LoadSubTask: file is empty"; return; }

		PushUndoState();
		// Flush current live objects to the temp QSC, append the loaded task block as a
		// new top-level task, then reparse the whole level from the merged QSC. Using the
		// real parser keeps nested subtrees intact.
		std::string tempQsc = Utils::GetExeDirectory() + "\\editor\\qed\\temp\\objects.qsc";
		level_.GetLevelObjects().SaveToQSC(tempQsc);
		{
			std::ofstream out(tempQsc, std::ios::app);
			if (!out.is_open()) { status_message_ = "LoadSubTask: cannot write temp QSC"; return; }
			out << "\n" << block;
			if (block.back() != ';') out << ";";
			out << "\n";
		}
		level_.ReloadObjectsFromFile(tempQsc);
		EvaluateTrainTrackPositions();
		SnapObjectsToTerrain();
		RebuildLevelModelIds();
		// Object list changed — keep selection valid.
		if (selected_object_index_ >= (int)level_.GetLevelObjects().GetObjects().size())
			selected_object_index_ = -1;
		status_message_ = "Loaded task from: " + path + " (added as top-level task)";
		Logger::Get().Log(LogLevel::INFO, "[App] LoadSubTask: loaded " + path);
	}
}

void App::UpdateCursorMode() {
	if (terrain_edit_enabled_) {
		// Drop is the neutral terrain cursor; Lift/Lower for raise/lower brush
		if (edit_brush_ == 0)
			current_cursor_mode_ = CursorMode::TerrainLift;  // BRUSH_RAISE
		else if (edit_brush_ == 1)
			current_cursor_mode_ = CursorMode::TerrainLower; // BRUSH_LOWER
		else
			current_cursor_mode_ = CursorMode::TerrainDrop;  // any other terrain mode
		return;
	}
	// Camera mode (ALT held)
	bool enableCameraMode = Utils::IsKeyBindingPressed(Config::Get().keyEnableCamera);
	if (enableCameraMode) {
		// ALT + left button held = camera look (editor_camera.spr)
		// ALT + any movement = lateral move (editor_move.spr)
		current_cursor_mode_ = mouse_state_.left_button_down_ ? CursorMode::Camera : CursorMode::Move;
		camera_mode_moved_ = false;
		return;
	}
	current_cursor_mode_ = CursorMode::Default;
}

void App::DrawCustomCursor() {
	UpdateCursorMode();
	int idx = (int)current_cursor_mode_;
	if (idx < 0 || idx >= NUM_CURSORS || !cursor_tex_ids_[idx]) {
		idx = 0; // fallback to Default
		if (!cursor_tex_ids_[0]) return;
	}
	int mx = mouse_state_.prior_x_;
	int my = mouse_state_.prior_y_;
	int vw = window_state_.viewport_width_;
	int vh = window_state_.viewport_height_;
	int w  = cursor_tex_ws_[idx];
	int h  = cursor_tex_hs_[idx];

	// Match the renderer's fixed-function HUD state: a shader/VAO left bound from
	// the 3D pass makes immediate-mode textured quads draw nothing.
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
	glActiveTexture(GL_TEXTURE0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
	glOrtho(0, vw, vh, 0, -1, 1);   // y=0 at top (top-down to match mouse coords)
	glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, cursor_tex_ids_[idx]);
	glColor4f(1.f, 1.f, 1.f, 1.f);
	glBegin(GL_QUADS);
	  glTexCoord2f(0, 0); glVertex2i(mx,     my);
	  glTexCoord2f(1, 0); glVertex2i(mx + w, my);
	  glTexCoord2f(1, 1); glVertex2i(mx + w, my + h);
	  glTexCoord2f(0, 1); glVertex2i(mx,     my + h);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	glMatrixMode(GL_MODELVIEW);  glPopMatrix();
	glMatrixMode(GL_PROJECTION); glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

// ─────────────────────────────────────────────────────────────────────────────

void App::DrawProgressOverlay(const char* title, int pct, const char* stage) {
	pct = std::max(0, std::min(100, pct));
	int vw = window_state_.viewport_width_;
	int vh = window_state_.viewport_height_;
	if (vw <= 0 || vh <= 0) return;
	glViewport(0, 0, vw, vh);
	glClearColor(0.02f, 0.06f, 0.02f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); glLoadIdentity();
	glOrtho(0, vw, 0, vh, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix(); glLoadIdentity();

	int pw = 340, ph = 84;
	int px = (vw - pw) / 2, py = (vh - ph) / 2;
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.0f, 0.15f, 0.0f, 0.9f);
	glBegin(GL_QUADS);
	glVertex2i(px, py); glVertex2i(px+pw, py);
	glVertex2i(px+pw, py+ph); glVertex2i(px, py+ph);
	glEnd();
	glColor3f(0.0f, 0.8f, 0.0f);
	glBegin(GL_LINE_LOOP);
	glVertex2i(px, py); glVertex2i(px+pw, py);
	glVertex2i(px+pw, py+ph); glVertex2i(px, py+ph);
	glEnd();
	int bx = px+20, by = py+18, bw = pw-40, bh = 12;
	glColor3f(0.0f, 0.3f, 0.0f);
	glBegin(GL_QUADS);
	glVertex2i(bx, by); glVertex2i(bx+bw, by);
	glVertex2i(bx+bw, by+bh); glVertex2i(bx, by+bh);
	glEnd();
	int fw = bw * pct / 100;
	glColor3f(0.1f, 0.95f, 0.1f);
	glBegin(GL_QUADS);
	glVertex2i(bx, by); glVertex2i(bx+fw, by);
	glVertex2i(bx+fw, by+bh); glVertex2i(bx, by+bh);
	glEnd();
	glDisable(GL_BLEND);

	char msg[160];
	snprintf(msg, sizeof(msg), "%s  -  %d%%  (%s)", title ? title : "", pct, stage ? stage : "");
	glColor3f(0.0f, 0.9f, 0.0f);
	glRasterPos2i(px + 20, py + ph - 18);
	for (const char* c = msg; *c; ++c)
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

	glMatrixMode(GL_PROJECTION); glPopMatrix();
	glMatrixMode(GL_MODELVIEW);  glPopMatrix();
	glEnable(GL_DEPTH_TEST);
	glutSwapBuffers();
}

void App::LoadLevel(int level_no) {
	try {
		Logger::Get().Log(LogLevel::INFO, "[App] ==========================================");
		Logger::Get().Log(LogLevel::INFO, "[App] LoadLevel() START for level " + std::to_string(level_no));
		Logger::Get().Log(LogLevel::INFO, "[App] ==========================================");

		// Track in-session level switches. The FIRST load (last_loaded_level_ < 0) is
		// equivalent to a fresh process; any later load is a MENU/RELOAD switch whose
		// only difference from a fresh process is leftover per-level state.
		const bool is_switch = (last_loaded_level_ >= 0 && last_loaded_level_ != level_no);
		if (is_switch) {
			Logger::Get().Log(LogLevel::INFO, "[App] MENU/RELOAD switch from level " +
				std::to_string(last_loaded_level_) + " to " + std::to_string(level_no) +
				" — performing full previous-level teardown");
			AssetExtractor::ClearLevelAssets(last_loaded_level_, Utils::GetExeDirectory());
		} else {
			Logger::Get().Log(LogLevel::INFO, "[App] Initial level load (level " +
				std::to_string(level_no) + ")");
		}

		AssetExtractor::ClearLevelAssets(level_no, Utils::GetExeDirectory());

		// Verify level number is valid
		if (level_no < MIN_LEVEL_NO || level_no > MAX_LEVEL_NO) {
			std::string errorMsg = "Invalid level number: " + std::to_string(level_no) + " (valid range: " + std::to_string(MIN_LEVEL_NO) + "-" + std::to_string(MAX_LEVEL_NO) + ")";
			Utils::LogAndShowError(errorMsg, "IGI Editor - Error");
			Logger::Get().Log(LogLevel::ERR, errorMsg);
			// Exit the application safely; destructor will be called automatically
			std::exit(EXIT_FAILURE);
		}
		if (Config::Get().enableBackup) {
			std::string gameLevelDir = Utils::GetIGIRootPath() + "\\missions\\location0\\level" + std::to_string(level_no);
			std::string backupLevelDir = Utils::GetExeDirectory() + "\\editor\\backup\\level" + std::to_string(level_no);
			if (!std::filesystem::exists(backupLevelDir) && std::filesystem::exists(gameLevelDir)) {
				try {
					std::filesystem::create_directories(backupLevelDir);
					std::filesystem::copy(gameLevelDir, backupLevelDir, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
					Logger::Get().Log(LogLevel::INFO, "[App] Backup created for level " + std::to_string(level_no) + " at " + backupLevelDir);
				} catch (const std::exception& e) {
					Logger::Get().Log(LogLevel::ERR, "[App] Failed to create level backup: " + std::string(e.what()));
				}
			}
		}
		
		selected_object_index_ = -1;
		hover_object_index_ = -1;
		status_message_.clear();

		// NOTE: We must NOT purge the previous level's extracted assets here. The
		// texture/model resolver (FindTextureFile step 6 + lazy cross-level extraction)
		// deliberately searches *other* levels' extracted folders to resolve cross-level
		// references (e.g. a level-6 object using a level-14 texture). Deleting them on
		// switch removes legitimate fallback sources. The renderer caches are still fully
		// torn down by BeginLoadLevel()/ClearCaches() below.
		renderer_.SetLevel(level_no);
		renderer_.BeginLoadLevel();
		Logger::Get().Log(LogLevel::INFO, "[App] After BeginLoadLevel teardown: renderer meshCache=" +
			std::to_string(renderer_.GetMeshCacheCount()) + " textureCache=" +
			std::to_string(renderer_.GetTextureCacheCount()) + " (both should be 0)");
		renderer_.SetSplineTerrainQuery([this](double x, double y, float& z) {
			return level_.GetTerrainZ(x, y, z);
		});

		Level::load_params_s level_load_params_s = {
			.level_no_ = level_no,
			.render_res_loader_ = &renderer_
		};

		// ── Loading overlay ──────────────────────────────────────────────────
		// Staged progress: the load is synchronous, so we redraw + swap the bar at
		// each milestone (10% → 100%). Reusable lambda fills proportionally to pct.
		auto drawLoadBar = [&](int pct, const char* stage) {
			char title[32];
			snprintf(title, sizeof(title), "Loading Level %d", level_no);
			DrawProgressOverlay(title, pct, stage);
		};
		drawLoadBar(10, "reading level");
		// ─────────────────────────────────────────────────────────────────────

		drawLoadBar(25, "models & terrain");
		glm::vec3 start_pos;
		float start_yaw;
		if (level_.Load(level_load_params_s, start_pos, start_yaw)) {
			const auto& config = Config::Get();
			viewer_.pos_ = (config.cameraPosX != 0.0f || config.cameraPosY != 0.0f || config.cameraPosZ != 0.0f) ?
				glm::vec3(config.cameraPosX, config.cameraPosY, config.cameraPosZ) : start_pos;

			bool hasConfigOri = (config.cameraOriX != 0.0f || config.cameraOriY != 0.0f || config.cameraOriZ != 0.0f);
			if (hasConfigOri) {
				viewer_.yaw_   = config.cameraOriX;
				viewer_.pitch_ = config.cameraOriY;
				viewer_.roll_  = config.cameraOriZ;
			} else {
				// Convert game yaw (radians) to viewer yaw (degrees, 0=+Y north, CW).
				// Add 180° so the editor camera faces toward objects rather than with them.
				viewer_.yaw_   = glm::degrees(atan2f(-cosf(start_yaw), sinf(start_yaw))) + 180.0f;
				viewer_.pitch_ = 10.0f;
				viewer_.roll_  = 0.0f;
			}

			UpdateViewerVectors();
			Logger::Get().Log(LogLevel::INFO, "[App] Level " + std::to_string(level_no) + " loaded. Viewer start=(" + std::to_string(viewer_.pos_.x) + "," + std::to_string(viewer_.pos_.y) + "," + std::to_string(viewer_.pos_.z) + ") yaw=" + std::to_string(viewer_.yaw_));
			last_loaded_level_ = level_no;
		}
		else {
			std::string errorMsg = "Failed to load level " + std::to_string(level_no) + "\n\nPlease check if the terrain files exist in the correct location.";
			Utils::ShowError(errorMsg, "IGI Editor - Error");
			Logger::Get().Log(LogLevel::ERR, "[App] Failed to load level " + std::to_string(level_no));
		}


		drawLoadBar(60, "tracks");
		// Step 2: Track Evaluation (Dynamic object placement along paths)
		EvaluateTrainTrackPositions();

		drawLoadBar(75, "snapping objects");
		// Step 3: Always snap objects to terrain after any level load
		Logger::Get().Log(LogLevel::INFO, "[App] Step 3: Snapping objects to terrain...");
		SnapObjectsToTerrain();
		drawLoadBar(90, "finalizing");

		// AI rotation override: AI models (HumanSoldier, HumanAI) only have horizontal rotation
		auto& objects = level_.GetLevelObjects().GetObjects();

		// Build runtime parameter schemas from this level's Task_DeclareParameters so
		// the property editor can expose EVERY declared field of any task type.
		TaskSchemaNS::ClearRegisteredSchemas();
		for (const auto& obj : objects) {
			if (obj.type == "Task_DeclareParameters" && obj.argTokens.size() >= 3) {
				std::string typeName = obj.argTokens[0];
				if (typeName.size() >= 2 && typeName.front() == '"' && typeName.back() == '"')
					typeName = typeName.substr(1, typeName.size() - 2);
				TaskSchemaNS::RegisterSchema(typeName, TaskSchemaNS::ParseDeclaration(obj.argTokens));
			}
		}

		for (auto& obj : objects) {
			if (obj.modelId == "000_01_1") continue; // Skip Player Jones
			if (obj.type == "HumanSoldier" || obj.type == "HumanAI" || obj.type.find("AITYPE") == 0) {
				obj.rot.x = 0.0;           // PITCH = 0
				obj.rot.y = 0.0;           // ROLL = 0
				// Preserve existing rotation if it's already set, otherwise default to a full circle
				if (obj.rot.z == 0.0) obj.rot.z = 6.28318;
				Logger::Get().Log(LogLevel::INFO, "[App] Applied AI rotation override (horizontal only) for " + obj.name + " (" + obj.type + ")");
			}
		}

		// Log all loaded objects for verification script
		for (const auto& obj : objects) {
			if (obj.isSplineWaypoint || !obj.segmentModelId.empty()) {
			    Logger::Get().Log(LogLevel::INFO, "[App_Debug] Found waypoint/segment. modelId=" + obj.modelId + " segmentModelId=" + obj.segmentModelId);
			}
			std::string mId = !obj.modelId.empty() ? obj.modelId : obj.segmentModelId;
			if (obj.deleted || mId.empty()) continue;
			Logger::Get().Log(LogLevel::INFO, "[LevelLoader] Object Loaded: ModelID=" + mId +
				", Type=" + obj.type + ", Name=" + obj.name + ", Pos=(" +
				std::to_string(obj.pos.x) + ", " + std::to_string(obj.pos.y) + ", " + std::to_string(obj.pos.z) + ")" +
				", Ori=(" + std::to_string(obj.rot.x) + ", " + std::to_string(obj.rot.y) + ", " + std::to_string(obj.rot.z) + ")" +
				", Tex=" + mId + ", Model=" + mId);		}
		
		RebuildLevelModelIds();

		// Build the set of models packed in this level's .res (names only — stream so we
		// never buffer the whole 200+MB archive in the 32-bit process). (issue 2)
		{
			std::string gameRes = Utils::GetIGIRootPath() +
				"\\missions\\location0\\level" + std::to_string(level_no) +
				"\\models\\level" + std::to_string(level_no) + ".res";
			level_res_models_ = ResModelSet();
			std::string resErr;
			size_t entryCount = 0;
			bool ok = RES_ForEachEntry(gameRes,
				[&](const std::string& name, const uint8_t*, size_t) {
					level_res_models_.AddEntry(name);
					++entryCount;
				}, resErr);
			Logger::Get().Log(LogLevel::INFO, std::string("[App] Level .res model set: ") +
				(ok ? std::to_string(entryCount) + " entries (streamed)"
				    : "UNAVAILABLE (" + gameRes + "): " + resErr));
		}
		Logger::Get().Log(LogLevel::INFO, "[App] ==========================================");
		Logger::Get().Log(LogLevel::INFO, "[App] LoadLevel() COMPLETE for level " + std::to_string(level_no));
		Logger::Get().Log(LogLevel::INFO, "[App] ==========================================");
		drawLoadBar(100, "done");
	}
	catch (const std::exception& e) {
		std::string errorMsg = "Error loading level " + std::to_string(level_no) + ":\n" + std::string(e.what());
		Utils::LogAndShowError(errorMsg, "IGI Editor - Error");
		Logger::Get().Log(LogLevel::ERR, errorMsg);
	}
	catch (...) {
		std::string errorMsg = "Unknown error loading level " + std::to_string(level_no);
		Utils::LogAndShowError(errorMsg, "IGI Editor - Error");
		Logger::Get().Log(LogLevel::ERR, errorMsg);
	}
}

void App::SetGameLevel(int level_no) {
	bridge_.SetGameLevel(level_no);
}








void App::FlushAttaProxiesToMef() {
	for (auto& obj : level_.GetLevelObjects().GetObjects()) {
		if (!obj.isAttaProxy || !obj.modified) continue;

		// Position: convert world pos → local pos via stored inverse parent matrix.
		glm::vec4 lp = obj.attaInvParentMat * glm::vec4(glm::vec3(obj.pos), 1.0f);
		glm::vec3 localPos(lp);

		// Rotation: rebuild world rotation matrix from Euler angles (Rz * Rx * Ry),
		// then convert to local rotation: localRot = parentRotInv * worldRot.
		// The upper-left 3×3 of attaInvParentMat IS parentRotInv.
		glm::mat4 wr(1.0f);
		wr = glm::rotate(wr, (float)obj.rot.z, glm::vec3(0,0,1));
		wr = glm::rotate(wr, (float)obj.rot.x, glm::vec3(1,0,0));
		wr = glm::rotate(wr, (float)obj.rot.y, glm::vec3(0,1,0));
		glm::mat3 worldRot3(wr);
		glm::mat3 parentRotInv = glm::mat3(obj.attaInvParentMat);
		glm::mat3 localRot = parentRotInv * worldRot3;

		renderer_.UpdateAttaLocalPosInMef(obj.attaParentModelId, obj.attaIsBuilding,
		                                  obj.attaRecordIndex, localPos, localRot);
		obj.modified = false;
	}
}

void App::SaveCurrentLevel() {
	try {
		Logger::Get().Log(LogLevel::INFO, "[App] SaveCurrentLevel() called");
		FlushAttaProxiesToMef();
		level_.SaveChanges();

		// Compile edited AI script (.qsc text -> .qvm file) before saving the level QVM.
		if (ai_script_dirty_ && !ai_script_path_.empty()) {
			Logger::Get().Log(LogLevel::INFO, "[App] Compiling modified AI script to: " + ai_script_path_);
			auto lexResult   = qsc::Lex(ai_script_text_);
			auto parseResult = lexResult.ok ? qsc::Parse(lexResult.tokens) : qsc::ParseResult{};
			std::string compileErr;
			bool ok = lexResult.ok && parseResult.ok &&
			          qvm::CompileToFile(*parseResult.program, ai_script_path_, &compileErr);
			if (ok) {
				QVMFile check = QVM_Parse(ai_script_path_);
				if (check.valid) {
					ai_script_dirty_ = false;
					status_message_ = "AI script compiled: " + ai_script_path_;
					Logger::Get().Log(LogLevel::INFO, "[App] AI script compiled OK");
				} else {
					Logger::Get().Log(LogLevel::ERR, "[App] AI script round-trip failed -- file may be corrupt");
					status_message_ = "AI script compile: round-trip failed";
				}
			} else {
				std::string detail = compileErr.empty() ? "(no detail)" : compileErr;
				Logger::Get().Log(LogLevel::ERR, "[App] AI script compile error: " + detail);
				status_message_ = "AI script compile error: " + detail;
			}
		}

		Logger::Get().Log(LogLevel::INFO, "[App] Calling SaveAndCompile()");
		SaveAndCompile();
	}
	catch (const std::exception& e) {
		std::string errorMsg = "Error saving level:\n" + std::string(e.what());
		Utils::LogAndShowError(errorMsg, "IGI Editor - Error");
		Logger::Get().Log(LogLevel::ERR, errorMsg);
	}
	catch (...) {
		std::string errorMsg = "Unknown error saving level";
		Utils::LogAndShowError(errorMsg, "IGI Editor - Error");
		Logger::Get().Log(LogLevel::ERR, errorMsg);
	}
}

void App::ExportTextureMap() {
	int levelNo = level_.GetLevelNo();
	const std::string root = Utils::GetIGIRootPath();
	const std::string datPath = root + "\\missions\\location0\\level" +
	    std::to_string(levelNo) + "\\level" + std::to_string(levelNo) + ".dat";
	const std::string outPath = Utils::GetExeDirectory() +
	    "\\level" + std::to_string(levelNo) + "_texmap.json";

	Logger::Get().Log(LogLevel::INFO,
	    "[App] ExportTextureMap level=" + std::to_string(levelNo) +
	    " dat=" + datPath + " out=" + outPath);

	DATFile dat = DAT_Parse(datPath);
	if (!dat.valid) {
		Utils::ShowError("Failed to parse DAT:\n" + dat.error, "Export Texture Map");
		return;
	}
	if (DAT_WriteJSON(dat, outPath)) {
		Utils::ShowInfo(
		    "Texture map exported (JSON):\n" + outPath +
		    "\nModels: " + std::to_string(dat.models.size()) +
		    "  Textures: " + std::to_string(dat.allTextures.size()),
		    "Export Texture Map");
	} else {
		Utils::ShowError("Could not write to:\n" + outPath, "Export Texture Map");
	}
}

int App::GetCurLevelNo() const {
	return level_.GetLevelNo();
}

void App::ToggleOverlayWireframe() {
	draw_params_.overlay_wireframe_ = !draw_params_.overlay_wireframe_;
}

void App::ToggleDrawParts(int part) {
	if (draw_params_.draw_parts_ & part) {
		draw_params_.draw_parts_ &= ~part;
	}
	else {
		draw_params_.draw_parts_ |= part;
	}
}

void App::SetDrawParts(int parts) {
	draw_params_.draw_parts_ = parts;
}

void App::ToggleTerrainDrawOption(int opt) {
	if (draw_params_.draw_terrain_options_ & opt) {
		draw_params_.draw_terrain_options_ &= ~opt;
	}
	else {
		draw_params_.draw_terrain_options_ |= opt;
	}
}

void App::ToggleTerrainModOption(int opt) {
	if (terrain_mod_options_ & opt) {
		terrain_mod_options_ &= ~opt;
	}
	else {
		terrain_mod_options_ |= opt;
	}
}

bool App::GetOverlayWireframe() const {
	return draw_params_.overlay_wireframe_;
}

int	App::GetDrawParts() const {
	return draw_params_.draw_parts_;
}

int	App::GetTerrainDrawOptions() const {
	return draw_params_.draw_terrain_options_;
}

int	App::GetTerrainModOptions() const {
	return terrain_mod_options_;
}

// events
void App::OnWindowResize(int width, int height) {
	window_state_.viewport_width_ = std::max(1, width);
	window_state_.viewport_height_ = std::max(1, height);

	view_define_.viewport_width_ = window_state_.viewport_width_;
	view_define_.viewport_height_ = window_state_.viewport_height_;

	glViewport(0, 0, width, height);

	// update fovx_
	float h = std::tan(view_define_.fovy_ * 0.5f);
	float w = h * width / height;
	view_define_.fovx_ = std::atan(w) * 2.0f;

	float tan_half_fovx = (float)std::tan(view_define_.fovx_ * 0.5);
	float tan_half_fovy = (float)std::tan(view_define_.fovy_ * 0.5);

	view_define_.tan_half_fovx_ = tan_half_fovx;
	view_define_.tan_half_fovy_ = tan_half_fovy;

	view_define_.half_viewport_width_div_tan_half_fovx_ = window_state_.viewport_width_ * 0.5f / tan_half_fovx;
	view_define_.half_viewport_height_div_tan_half_fovy_ = window_state_.viewport_height_ * 0.5f / tan_half_fovy;

}

void App::OnDisplay() {
	Frame(0.0f);
}

// AI text editor helpers — must be defined before Input_OnMouse and Input_OnSpecial.

// Returns flat text offsets of each visual line start.
// Lines split on '\n'; lines longer than max_chars wrap to the next visual line.

// input
void App::OnIdle() {
	// freeglut pumps messages with a window-handle filter and misses WM_HOTKEY,
	// which is a thread message only retrievable via PeekMessage(NULL, ...).
	// Poll it here so F3 works while the game is running and editor is iconified.
	if (game_process_.running) {
		MSG msg = {};
		while (PeekMessage(&msg, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {
			if (static_cast<int>(msg.wParam) == HOTKEY_ID_TOGGLE_GAME) {
				Logger::Get().Log(LogLevel::INFO, "[ToggleGame] Global hotkey received — stopping game");
				LaunchGame();
			}
		}
	}

	// Check game exit before the frame-rate throttle so it fires on every call,
	// even when GLUT is running slowly while the editor is iconified.
	if (game_process_.running && game_process_.hProcess) {
		bool exited = game_exited_.load(std::memory_order_acquire);
		if (!exited) {
			// Direct non-blocking poll as fallback in case monitor thread signal was missed
			DWORD waitResult = WaitForSingleObject(game_process_.hProcess, 0);
			exited = (waitResult == WAIT_OBJECT_0);
		}
		if (exited) {
			Logger::Get().Log(LogLevel::INFO, "[App] Game process exited (PID=" +
			                  std::to_string(game_process_.pid) + "), restoring editor");
			CloseHandle(game_process_.hProcess);
			CloseHandle(game_process_.hThread);
			if (game_process_.hMonitorThread) {
				WaitForSingleObject(game_process_.hMonitorThread, 1000);
				CloseHandle(game_process_.hMonitorThread);
			}
			game_exited_.store(false, std::memory_order_relaxed);
			game_process_ = {};
			prior_frame_time_ = Sys_Milliseconds();
			glutShowWindow();
			glutPostRedisplay();
			if (editor_hwnd_) {
				ShowWindow(editor_hwnd_, SW_RESTORE);
				SetForegroundWindow(editor_hwnd_);
				BringWindowToTop(editor_hwnd_);
			}
			if (editor_hwnd_) {
				KillTimer(editor_hwnd_, 1);
				UnregisterHotKey(editor_hwnd_, HOTKEY_ID_TOGGLE_GAME);
			}
			Logger::Get().Log(LogLevel::INFO, "[ToggleGame] Global hotkey unregistered — editor restored");
			return;
		}
	}

	// While the game is running the editor window is iconified.
	// glutSwapBuffers() deadlocks on minimized windows, so skip rendering entirely.
	if (game_process_.running) return;

	int64_t cur_time = Sys_Milliseconds();
	int64_t delta_time = cur_time - prior_frame_time_;
	if (delta_time < 16) {
		return;
	}

	Frame(delta_time * 0.001f);	// convert to seconds

	prior_frame_time_ = cur_time;
}

void App::Frame(float delta_seconds) {
	if (pause_mode_) {
		// Skip all updates when paused, just render
		UpdateViewDefine();
		if (mouse_state_.prior_x_ != last_pick_x_ || mouse_state_.prior_y_ != last_pick_y_) {
			hover_object_index_ = PickObjectAtScreenPos(mouse_state_.prior_x_, mouse_state_.prior_y_);
			if (hover_object_index_ >= Renderer::kAttaPickBase) hover_object_index_ = -1; // ATTA hovered (clickable; promote on click)
			last_pick_x_ = mouse_state_.prior_x_;
			last_pick_y_ = mouse_state_.prior_y_;
		}
		float ground_z = 0.0f;
		level_.GetTerrainZ(viewer_.pos_.x, viewer_.pos_.y, ground_z);
		Renderer::task_tree_view_params_s task_tree_view = {
			.show_hud_ = true,
			.status_msg_ = status_message_,
			.pause_mode_ = true,
			.pause_active_input_ = pause_active_input_,
			.pause_level_input_ = pause_level_input_,
			.pause_search_input_ = pause_search_input_,
			.pause_terrain_expanded_ = pause_terrain_expanded_,
			.show_debug_ = show_debug_,
			.show_help_ = show_help_,
			.edit_mode_ = edit_mode_,
			.terrain_edit_enabled_ = terrain_edit_enabled_,
			.terrain_mod_options_ = terrain_mod_options_,
			.selected_object_index_ = selected_object_index_,
			.hover_object_index_ = hover_object_index_,
			.hover_tree_index_ = hover_tree_index_,
			.mouse_x_ = mouse_state_.prior_x_,
			.mouse_y_ = mouse_state_.prior_y_,
			.tree_scroll_offset = tree_scroll_offset_,
			.tree_decl_expanded = tree_decl_expanded_,
			.level_objects_ = &level_.GetLevelObjects(),
			.task_picker_open_ = task_picker_open_,
			.task_picker_selected_idx_ = task_picker_selected_idx_,
			.task_picker_scroll_offset_ = task_picker_scroll_offset_,
			.task_picker_search_ = task_picker_search_,
			.enable_camera_mode_ = Utils::IsKeyBindingPressed(Config::Get().keyEnableCamera),
			.prop_editor_open_     = prop_editor_open_,
			.prop_field_index_     = prop_field_index_,
			.prop_text_edit_field_ = prop_text_edit_field_,
			.prop_edit_obj_index_  = prop_edit_obj_index_,
			.prop_drag_obj_index_  = prop_drag_obj_index_,
			.prop_text_buf_        = prop_text_buf_,
			.prop_text_caret_      = prop_text_caret_,
			.prop_panel_scroll_    = prop_panel_scroll_,
			.find_open_            = find_open_,
			.find_query_           = find_query_,
			.find_result_idx_      = find_result_idx_,
			.selected_obj_is_ai    = (selected_object_index_ >= 0 &&
				selected_object_index_ < (int)level_.GetLevelObjects().GetObjects().size() &&
				ai_model_ids_.count(level_.GetLevelObjects().GetObjects()[selected_object_index_].modelId) > 0),
			.help_scroll_offset_   = help_scroll_offset_,
			.help_entries_         = &help_entries_,
			.show_task_type_       = show_task_type_,
			.find_mode_            = (int)find_mode_,
			.file_dialog_mode_     = (int)file_dialog_mode_,
			.file_dialog_path_     = file_dialog_path_,
			.file_dialog_caret_    = file_dialog_caret_,
			.ac_task_picker_open_  = ac_task_picker_open_,
			.ac_task_selected_idx_ = ac_task_selected_idx_,
			.ac_task_scroll_offset_= ac_task_scroll_offset_,
			.ac_task_filter_       = ac_task_filter_,
			.ac_task_items_        = &ac_task_items_,
			.model_picker_open_    = model_picker_open_,
			.model_picker_selected_= model_picker_selected_,
			.model_picker_scroll_  = model_picker_scroll_,
			.model_picker_filter_  = model_picker_filter_,
			.model_ids_            = &level_model_ids_,
			.ai_script_path_       = ai_script_path_,
			.ai_script_text_       = ai_script_text_,
			.ai_script_dirty_      = ai_script_dirty_,
			.terrain_brush_          = edit_brush_,
			.terrain_brush_radius_   = edit_brush_radius_,
			.terrain_brush_strength_ = edit_brush_strength_,
		};
		draw_params_.level_objects_ = &level_.GetLevelObjects();
		draw_params_.selected_object_index_ = selected_object_index_;
		draw_params_.show_magic_obj_spheres_ = show_magic_obj_spheres_;
		draw_params_.terrain_id_at_world_xy_ =
			[this](double x, double y) { return level_.GetTerrainNodeId(x, y); };
		draw_params_.terrain_z_at_world_xy_ =
			[this](double x, double y, float& z) { return level_.GetTerrainZ(x, y, z); };
		renderer_.Draw(draw_params_, task_tree_view);

		DrawCustomCursor();
		glutSwapBuffers();
		return;
	}

	frame_++;
	frame_ %= 0xFFFFFFFF;	// reserve value 0xFFFFFFFF (-1) for INVALID_FRAME

	ProcessInput(delta_seconds);

	// Per-frame position-drag velocity: the pad / Z slider accelerate while held in
	// a direction and keep moving when the cursor is pinned at the window edge.
	if (mouse_state_.left_button_down_ && prop_field_index_ >= 0 && selected_object_index_ >= 0 &&
	    !Utils::IsKeyBindingPressed(Config::Get().keyEnableCamera)) {
		ApplyPropPositionDrag();
	}

	if (edit_mode_ && terrain_edit_enabled_ && mouse_state_.left_button_down_) {
		EditorProcessClick();
	}

	UpdateViewDefine();
	if (mouse_state_.prior_x_ != last_pick_x_ || mouse_state_.prior_y_ != last_pick_y_) {
		bool camMode    = Utils::IsKeyBindingPressed(Config::Get().keyEnableCamera);
		bool overPanel  = prop_editor_open_ &&
		                  mouse_state_.prior_x_ < (PropPanel::kLeft + PropPanel::kWidth);
		if (camMode || overPanel) {
			hover_object_index_ = -1;
		} else {
			hover_object_index_ = PickObjectAtScreenPos(mouse_state_.prior_x_, mouse_state_.prior_y_);
			if (hover_object_index_ >= Renderer::kAttaPickBase) hover_object_index_ = -1; // ATTA hovered (clickable; promote on click)
		}
		last_pick_x_ = mouse_state_.prior_x_;
		last_pick_y_ = mouse_state_.prior_y_;
	}

	vert_flat_sky_layer_s * fsl_vb = renderer_.MapFlatSkyLayersVB();
	vert_pos_a_uv_s* terrain_vb = renderer_.MapTerrainVB();
	uint32_t* terrain_ib = renderer_.MapTerrainIB();
	render_chunk_s* render_chunks = renderer_.GetTerrainRenderChunckBuffer();

	update_params_s update_params = {
		.frame_ = frame_,
		.delta_seconds_ = delta_seconds,
		.view_define_ = &view_define_,
		.flat_sky_layer_vb_ = fsl_vb,
		.terrain_mod_options_ = terrain_mod_options_,
		.terrain_vb_ = terrain_vb,
		.terrain_ib_ = terrain_ib,
		.terrain_render_chunks_ = render_chunks
	};

	level_.Update(update_params);

	if (terrain_ib) {
		renderer_.UnmapTerrainIB();
	}

	if (terrain_vb) {
		renderer_.UnmapTerrainVB();
	}

	if (fsl_vb) {
		renderer_.UnmapFlatSkyLayersVB();
	}

	draw_params_.flat_sky_layer_is_visible_ = update_params.flat_sky_layer_is_visible_;
	draw_params_.num_terrain_render_chunk_ = update_params.num_terrain_render_chunk_;
	draw_params_.level_objects_ = &level_.GetLevelObjects();
	draw_params_.selected_object_index_ = selected_object_index_;
	draw_params_.show_magic_obj_spheres_ = show_magic_obj_spheres_;
	draw_params_.terrain_id_at_world_xy_ =
		[this](double x, double y) { return level_.GetTerrainNodeId(x, y); };


	float ground_z = 0.0f;
	bridge_.SetEnabled(show_hud_);
	IGIBridge::PositionData data = bridge_.GetLatestData();
	level_.GetTerrainZ(viewer_.pos_.x, viewer_.pos_.y, ground_z);

	Renderer::task_tree_view_params_s task_tree_view = {
		.show_hud_ = show_hud_,
		.status_msg_ = status_message_,
		.pause_mode_ = pause_mode_,
		.pause_active_input_ = pause_active_input_,
		.pause_level_input_ = pause_level_input_,
		.pause_search_input_ = pause_search_input_,
		.pause_terrain_expanded_ = pause_terrain_expanded_,
		.show_debug_ = show_debug_,
		.show_help_ = show_help_,
		.edit_mode_ = edit_mode_,
		.terrain_edit_enabled_ = terrain_edit_enabled_,
		.terrain_mod_options_ = terrain_mod_options_,
		.selected_object_index_ = selected_object_index_,
		.hover_object_index_ = hover_object_index_,
		.hover_tree_index_ = hover_tree_index_,
		.mouse_x_ = mouse_state_.prior_x_,
		.mouse_y_ = mouse_state_.prior_y_,
		.tree_scroll_offset = tree_scroll_offset_,
		.tree_decl_expanded = tree_decl_expanded_,
		.level_objects_ = &level_.GetLevelObjects(),
		.task_picker_open_ = task_picker_open_,
		.task_picker_selected_idx_ = task_picker_selected_idx_,
		.task_picker_scroll_offset_ = task_picker_scroll_offset_,
		.task_picker_search_ = task_picker_search_,
		.enable_camera_mode_ = Utils::IsKeyBindingPressed(Config::Get().keyEnableCamera),
		.prop_editor_open_     = prop_editor_open_,
		.prop_field_index_     = prop_field_index_,
		.prop_text_edit_field_ = prop_text_edit_field_,
		.prop_edit_obj_index_  = prop_edit_obj_index_,
		.prop_drag_obj_index_  = prop_drag_obj_index_,
		.prop_text_buf_        = prop_text_buf_,
		.prop_text_caret_      = prop_text_caret_,
		.prop_panel_scroll_    = prop_panel_scroll_,
		.find_open_            = find_open_,
		.find_query_           = find_query_,
		.find_result_idx_      = find_result_idx_,
		.selected_obj_is_ai    = (selected_object_index_ >= 0 &&
			selected_object_index_ < (int)level_.GetLevelObjects().GetObjects().size() &&
			ai_model_ids_.count(level_.GetLevelObjects().GetObjects()[selected_object_index_].modelId) > 0),
		.help_scroll_offset_   = help_scroll_offset_,
		.help_entries_         = &help_entries_,
		.show_task_type_       = show_task_type_,
		.find_mode_            = (int)find_mode_,
		.file_dialog_mode_     = (int)file_dialog_mode_,
		.file_dialog_path_     = file_dialog_path_,
		.file_dialog_caret_    = file_dialog_caret_,
		.ac_task_picker_open_  = ac_task_picker_open_,
		.ac_task_selected_idx_ = ac_task_selected_idx_,
		.ac_task_scroll_offset_= ac_task_scroll_offset_,
		.ac_task_filter_       = ac_task_filter_,
		.ac_task_items_        = &ac_task_items_,
		.model_picker_open_    = model_picker_open_,
		.model_picker_selected_= model_picker_selected_,
		.model_picker_scroll_  = model_picker_scroll_,
		.model_picker_filter_  = model_picker_filter_,
		.model_ids_            = &level_model_ids_,
		.ai_script_path_        = ai_script_path_,
		.ai_script_text_        = ai_script_text_,
		.ai_script_dirty_       = ai_script_dirty_,
		.ai_script_vscroll_     = ai_script_vscroll_,
		.ai_script_path_hscroll_= ai_script_path_hscroll_,
		.terrain_brush_          = edit_brush_,
		.terrain_brush_radius_   = edit_brush_radius_,
		.terrain_brush_strength_ = edit_brush_strength_,
	};

	renderer_.Draw(draw_params_, task_tree_view);

	DrawCustomCursor();
	glutSwapBuffers();
}

void App::ToggleShowHUD() {
	show_hud_ = true;
}

bool App::GetShowHUD() const {
	return show_hud_;
}

void App::SetShowHUD(bool show) {
	show_hud_ = true;
}

void App::ToggleEditMode() {
    // Logic removed as requested
}

bool App::GetEditMode() const {
	return true; // Always true
}

void App::SetEditMode(bool enabled) {
    // Logic removed as requested
}

void App::SetTerrainEditEnabled(bool enabled) {
	terrain_edit_enabled_ = enabled;
	if (enabled) {
		static const char* kNames[] = {"Raise","Lower","Soften","Flatten"};
		int b = (edit_brush_ >= 0 && edit_brush_ < 4) ? edit_brush_ : 0;
		status_message_ = std::string("Terrain edit ON | Brush: ") + kNames[b] +
			" | Radius: " + std::to_string((long)edit_brush_radius_) +
			" | Strength: " + std::to_string((long)edit_brush_strength_);
	}
	UpdateCursorMode(); // Force cursor update instantly
	glutPostRedisplay(); // Force instant UI refresh
}

bool App::GetTerrainEditEnabled() const {
	return terrain_edit_enabled_;
}

void App::TogglePauseMenu() {
	pause_mode_ = !pause_mode_;
	// cursor_visible_ stays TRUE always — camera lock is handled dynamically in Input_OnMotion.
	// Hiding the cursor permanently caused the "mouse stuck" bug after resuming.
	window_state_.cursor_visible_ = true;
	if (pause_mode_) {
		// Opening pause menu
		glutSetCursor(GLUT_CURSOR_NONE);
	} else {
		// Closing pause menu: reset mouse state so no stale drag occurs
		input_.mouse_delta_x_ = 0;
		input_.mouse_delta_y_ = 0;
		mouse_state_.left_button_down_ = false;
		skip_input_on_motion_once_ = false;
		glutSetCursor(GLUT_CURSOR_NONE);
	}
}

bool App::GetPauseMode() const {
	return pause_mode_;
}

void App::SetEditBrush(int brush) {
	if (brush < 0) brush = 0;
	if (brush > 3) brush = 3;
	edit_brush_ = brush;
	static const char* kNames[] = {"Raise", "Lower", "Soften", "Flatten"};
	status_message_ = std::string("Terrain brush: ") + kNames[edit_brush_] +
		"  (radius " + std::to_string((long)edit_brush_radius_) +
		", strength " + std::to_string((long)edit_brush_strength_) + ")";
}

int App::GetEditBrush() const {
	return edit_brush_;
}

bool App::TerrainPaletteClick(int x, int y) {
	if (!edit_mode_ || !terrain_edit_enabled_) return false;
	int idx = TerrainPalette::HitTest(x, y, window_state_.viewport_width_, window_state_.viewport_height_);
	if (idx < 0) return false;
	switch (idx) {
	case TerrainPalette::kSelect:
		// Select/exit button: leave terrain edit, back to object editing.
		SetTerrainEditEnabled(false);
		break;
	case TerrainPalette::kRadiusDec:   AdjustBrushRadius(0.8);    break;
	case TerrainPalette::kRadiusInc:   AdjustBrushRadius(1.25);   break;
	case TerrainPalette::kStrengthDec: AdjustBrushStrength(-1.0); break;
	case TerrainPalette::kStrengthInc: AdjustBrushStrength(1.0);  break;
	default:
		SetEditBrush(TerrainPalette::BrushForIndex(idx));
		break;
	}
	return true;
}

void App::AdjustBrushRadius(double factor) {
	edit_brush_radius_ *= factor;
	if (edit_brush_radius_ < 5000.0)   edit_brush_radius_ = 5000.0;
	if (edit_brush_radius_ > 250000.0) edit_brush_radius_ = 250000.0;
	status_message_ = "Brush radius: " + std::to_string((long)edit_brush_radius_);
}

void App::AdjustBrushStrength(double delta) {
	edit_brush_strength_ += delta;
	if (edit_brush_strength_ < 1.0)   edit_brush_strength_ = 1.0;
	if (edit_brush_strength_ > 100.0) edit_brush_strength_ = 100.0;
	status_message_ = "Brush strength: " + std::to_string((long)edit_brush_strength_);
}

void App::SetSelectedObjectScale(float scale) {
	if (selected_object_index_ >= 0 && selected_object_index_ < (int)level_.GetLevelObjects().GetObjects().size()) {
		level_.GetLevelObjects().GetObjects()[selected_object_index_].scale = scale;
		Logger::Get().Log(LogLevel::INFO, "[App] Scale changed to " + std::to_string(scale) + " for object " + std::to_string(selected_object_index_));
	}
}

float App::GetSelectedObjectScale() const {
	if (selected_object_index_ >= 0 && selected_object_index_ < (int)level_.GetLevelObjects().GetObjects().size()) {
		return level_.GetLevelObjects().GetObjects()[selected_object_index_].scale;
	}
	return 1.0f;
}

#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

