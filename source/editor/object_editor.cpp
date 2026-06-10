/******************************************************************************
 * @file    object_editor.cpp
 * @brief   Object selection, 6-DOF manipulation, undo/redo — App methods
 *          extracted from app.cpp. Includes pch.h for the full header graph.
 *****************************************************************************/

#include "../pch.h"
#include <freeglut.h>
#include "../logger.h"
#include "../utils.h"
#include "../parsers/qvm_parser.h"
#include "../parsers/qvm_decompiler.h"
#include "../level/task_schema.h"
using namespace TaskSchemaNS;
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <optional>

// Forward declarations for helpers defined in camera.cpp
glm::dmat3 BuildRotMatZXY(const glm::dvec3& euler);
glm::dvec3 ExtractEulerZXY(const glm::dmat3& M);

// Manip key flags (same values as in app.cpp)
constexpr int OE_MK_MANIP_A     = FLAG_BIT(10);
constexpr int OE_MK_MANIP_B     = FLAG_BIT(11);
constexpr int OE_MK_MANIP_G     = FLAG_BIT(12);
constexpr int OE_MK_MANIP_S     = FLAG_BIT(13);
constexpr int OE_MK_MANIP_O     = FLAG_BIT(14);
constexpr int OE_MK_MANIP_SPACE = FLAG_BIT(15);

// ── File-scope static helpers ─────────────────────────────────────────────────

static bool containsIgnoreCaseOE(const std::string& str, const std::string& substr) {
	if (substr.empty()) return true;
	auto it = std::search(
		str.begin(), str.end(),
		substr.begin(), substr.end(),
		[](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
	);
	return it != str.end();
}

static int GetLookupObjectIndex(int hoverIdx, int selectedIdx) {
	if (hoverIdx >= 0) return hoverIdx;
	if (selectedIdx >= 0) return selectedIdx;
	return -1;
}

static void SetLookupStatus(std::string& status_message, const std::string& msg) {
	status_message = msg;
	Logger::Get().Log(LogLevel::INFO, msg);
	printf("%s\n", msg.c_str());
}

struct ModelEntry {
	std::string modelName;
	std::string modelId;
};

static std::vector<ModelEntry> LoadAllModelsFromJson() {
	std::vector<ModelEntry> entries;
	std::string jsonPath = Utils::GetExeDirectory() + "\\content\\tools\\IGIModels.json";

	if (!std::filesystem::exists(jsonPath)) {
		Logger::Get().Log(LogLevel::ERR, "[App] IGIModels.json not found in executable directory: " + jsonPath);
	} else {
		Logger::Get().Log(LogLevel::INFO, "[App] Loading model database from: " + jsonPath);
	}

	std::ifstream file(jsonPath, std::ios::binary);
	if (!file) {
		Logger::Get().Log(LogLevel::WARNING, "[App] Could not open database file: " + jsonPath);
		return entries;
	}

	std::stringstream ss;
	ss << file.rdbuf();
	std::string content = ss.str();

	size_t pos = 0;
	while ((pos = content.find("{", pos)) != std::string::npos) {
		size_t end = content.find("}", pos);
		if (end == std::string::npos) break;

		std::string entry = content.substr(pos, end - pos + 1);
		pos = end + 1;

		auto extractValue = [](const std::string& str, const std::string& key) -> std::string {
			size_t kpos = str.find("\"" + key + "\"");
			if (kpos == std::string::npos) return "";
			size_t colon = str.find(":", kpos);
			if (colon == std::string::npos) return "";
			size_t qStart = str.find("\"", colon);
			if (qStart == std::string::npos) return "";
			size_t qEnd = str.find("\"", qStart + 1);
			if (qEnd == std::string::npos) return "";
			return str.substr(qStart + 1, qEnd - qStart - 1);
		};

		ModelEntry item;
		item.modelName = extractValue(entry, "ModelName");
		item.modelId   = extractValue(entry, "ModelId");

		if (!item.modelId.empty() || !item.modelName.empty())
			entries.push_back(item);
	}

	return entries;
}

// ── App::EditorProcessClick ───────────────────────────────────────────────────
void App::EditorProcessClick() {
	if (!window_state_.viewport_width_ || !window_state_.viewport_height_) return;

	LevelObjects& lo = level_.GetLevelObjects();
	std::vector<LevelObject>& objects = lo.GetObjects();

	if (terrain_edit_enabled_) {
		// Terrain edit mode: build ray from camera through mouse and edit terrain
		glm::dmat4 proj_matrix = glm::perspective(
			(double)view_define_.fovy_,
			(double)window_state_.viewport_width_ / (double)window_state_.viewport_height_,
			(double)view_define_.render_z_near_,
			(double)view_define_.render_z_far_
		);
		glm::dmat4 mat_view = glm::lookAt(
			glm::dvec3(view_define_.pos_),
			glm::dvec3(view_define_.pos_) + glm::dvec3(view_define_.forward_),
			glm::dvec3(view_define_.up_));
		glm::dvec4 viewport(0.0, 0.0, (double)window_state_.viewport_width_, (double)window_state_.viewport_height_);

		double winX = (double)mouse_state_.prior_x_;
		double winY = (double)window_state_.viewport_height_ - (double)mouse_state_.prior_y_;

		glm::dvec3 start_pos = glm::unProject(glm::dvec3(winX, winY, 0.0), mat_view, proj_matrix, viewport);
		glm::dvec3 end_pos   = glm::unProject(glm::dvec3(winX, winY, 1.0), mat_view, proj_matrix, viewport);
		glm::vec3 ray_origin = (glm::vec3)start_pos;
		glm::vec3 ray_dir    = glm::normalize((glm::vec3)(end_pos - start_pos));

		printf("EditorClick: Mouse(%.0f, %.0f), RayDir(%.2f, %.2f, %.2f)\n", winX, winY, ray_dir.x, ray_dir.y, ray_dir.z);
		level_.EditorRaycastAndModify(ray_origin, ray_dir, edit_brush_, edit_brush_radius_, edit_brush_strength_);
		return;
	}

	// Object edit mode: select the object under the mouse cursor
	int pickedObject = PickObjectAtScreenPos(mouse_state_.prior_x_, mouse_state_.prior_y_);
	if (pickedObject >= Renderer::kAttaPickBase) {
		PromoteAttaToObject(pickedObject - Renderer::kAttaPickBase);
		return;
	}
	if (pickedObject >= 0 && pickedObject < (int)objects.size()) {
		selected_object_index_ = pickedObject;
		const LevelObject& obj = objects[pickedObject];

		// Auto-expand tree for the selected object
		int currentIdx = pickedObject;
		while (currentIdx != -1) {
			int parentIdx = objects[currentIdx].parentIndex;
			if (parentIdx != -1) objects[parentIdx].expanded = true;
			currentIdx = parentIdx;
		}

		Logger::Get().Log(LogLevel::INFO, "[App] Selected object index=" + std::to_string(pickedObject) +
		                  " model=" + obj.modelId + " type=" + (obj.isBuilding ? "building" : "object"));
		printf("Selected Object [%d]: %s (%s)\n", selected_object_index_,
			objects[selected_object_index_].name.c_str(), objects[selected_object_index_].modelId.c_str());
		printf("  Pos: (%.0f, %.0f, %.0f)\n", (double)obj.pos.x, (double)obj.pos.y, (double)obj.pos.z);
		printf("  Rot (Alpha/Beta/Gamma): (%.2f, %.2f, %.2f)\n", (double)obj.rot.x, (double)obj.rot.y, (double)obj.rot.z);
		printf("  Scale: %.2f\n", obj.scale);
		marker_manip_.start_x_   = mouse_state_.prior_x_;
		marker_manip_.start_y_   = mouse_state_.prior_y_;
		marker_manip_.start_pos_ = obj.pos;
		marker_manip_.start_rot_ = obj.rot;

		// Scroll the TaskTree so the newly selected object is visible.
		{
			auto visibleList = GetVisibleTreeNodes();
			int current_row = -1;
			for (int i = 0; i < (int)visibleList.size(); ++i) {
				if (visibleList[i] == selected_object_index_) { current_row = i; break; }
			}
			if (current_row >= 0) {
				const int row_h   = 16;
				const int start_y = 30;
				int max_rows = (window_state_.viewport_height_ - 50 - start_y) / row_h;
				if (max_rows > 0) {
					if (current_row < tree_scroll_offset_)
						tree_scroll_offset_ = current_row;
					else if (current_row >= tree_scroll_offset_ + max_rows)
						tree_scroll_offset_ = current_row - max_rows + 1;
				}
			}
		}
	} else {
		selected_object_index_ = -1;
		marker_manip_.mode_ = ManipulationMode::None;
		Logger::Get().Log(LogLevel::INFO, "[App] Object selection cleared");
	}
}

// ── App::SnapObjectsToTerrain ─────────────────────────────────────────────────
void App::SnapObjectsToTerrain() {
	auto& objects = level_.GetLevelObjects().GetObjects();
	Logger::Get().Log(LogLevel::INFO, "[App] Snapping " + std::to_string(objects.size()) + " objects to terrain...");

	int snapped = 0, skipped = 0, failed = 0;
	for (auto& obj : objects) {
		if (obj.modelId.empty() || (obj.pos.x == 0.0 && obj.pos.y == 0.0)) { skipped++; continue; }
		if (obj.modelId == "000_01_1") { skipped++; continue; }

		if (obj.type == "SCamera" || obj.type == "SCameraControl" || obj.type == "AlarmControl" ||
		    obj.type == "AIStationaryGunHolder" || obj.type == "StationaryGun" ||
		    obj.type == "Door" || obj.type == "Terminal" || obj.type == "SplineObjWaypoint" ||
		    obj.type == "AmbientArea" || obj.type == "Elevator" || obj.isWire || obj.type == "Train") {
			if (!obj.modified) {
				Logger::Get().Log(LogLevel::INFO, "[App] Snapping " + obj.type + " to original QSC Z: " + obj.modelId);
				obj.pos.z = obj.original_pos.z;
			} else {
				Logger::Get().Log(LogLevel::INFO, "[App] Preserving modified Z for " + obj.type + ": " + obj.modelId);
			}
			obj.snap_z_offset = 0.0;
			skipped++;
			continue;
		}

		bool isUnderground = Utils::IsUndergroundModel(obj.name, obj.modelId) || (obj.type == "Underground");
		if (!isUnderground && obj.parentIndex != -1 && obj.parentIndex < (int)objects.size()) {
			int pIdx = obj.parentIndex;
			while (pIdx != -1 && pIdx < (int)objects.size()) {
				const auto& parent = objects[pIdx];
				if (Utils::IsUndergroundModel(parent.name, parent.modelId) || (parent.type == "Underground")) {
					isUnderground = true; break;
				}
				pIdx = parent.parentIndex;
			}
		}
		if (isUnderground) {
			obj.snap_z_offset = 0.0;
			obj.pos.z = obj.original_pos.z;
			Logger::Get().Log(LogLevel::INFO, "[App] Underground context, preserving QSC Z for " + obj.modelId + " (" + obj.name + ") Z=" + std::to_string(obj.pos.z));
			skipped++;
			continue;
		}

		bool isIndoorChild = false;
		double buildingSnapZ = 0.0;
		if (obj.parentIndex != -1 && obj.parentIndex < (int)objects.size()) {
			int pIdx = obj.parentIndex;
			while (pIdx != -1 && pIdx < (int)objects.size()) {
				if (objects[pIdx].isBuilding) {
					isIndoorChild  = true;
					buildingSnapZ  = objects[pIdx].snap_z_offset;
					break;
				}
				pIdx = objects[pIdx].parentIndex;
			}
		}
		if (isIndoorChild) {
			obj.snap_z_offset = 0.0;
			obj.pos.z = obj.original_pos.z + buildingSnapZ;
			skipped++;
			continue;
		}

		float terrainZ = 0.0f;
		if (level_.GetTerrainZ(obj.pos.x, obj.pos.y, terrainZ, false)) {
			bool isHuman = (obj.type == "HumanSoldier" || obj.type == "HumanSoldierFemale");
			double zDelta = obj.original_pos.z - (double)terrainZ;

			if (!isHuman) {
				obj.snap_z_offset = 0.0;
				obj.pos.z = obj.original_pos.z;
				skipped++;
				continue;
			}
			if (zDelta > 100.0 || zDelta < 0.0) {
				obj.snap_z_offset = 0.0;
				obj.pos.z = obj.original_pos.z;
				skipped++;
				continue;
			}
			if (zDelta < -1000000.0) {
				obj.snap_z_offset = 0.0;
				obj.pos.z = obj.original_pos.z;
				Logger::Get().Log(LogLevel::INFO, "[App] Deep underground, preserving Z for " + obj.modelId + " (" + obj.name + ") Z=" + std::to_string(obj.pos.z));
				skipped++;
				continue;
			}
			float zOffset = renderer_.GetMeshZOffset(obj.modelId, obj.isBuilding);
			obj.snap_z_offset = (double)(zOffset * 40.96f * obj.scale);
			obj.pos.z = (double)terrainZ + obj.snap_z_offset;
			Logger::Get().Log(LogLevel::DEBUG, "[App] Snapped human " + obj.modelId + " to Z=" + std::to_string(obj.pos.z));
			snapped++;
		} else {
			Logger::Get().Log(LogLevel::WARNING, "[App] Snap FAILED for " + obj.modelId + " at (" + std::to_string(obj.pos.x) + ", " + std::to_string(obj.pos.y) + "). Outside terrain?");
			failed++;
		}
	}
	Logger::Get().Log(LogLevel::INFO, "[App] Snap complete. snapped=" + std::to_string(snapped) + " skipped=" + std::to_string(skipped) + " failed=" + std::to_string(failed));
}

// ── App::UpdateMarkerManipulation ─────────────────────────────────────────────
void App::UpdateMarkerManipulation() {
	if (selected_object_index_ < 0) return;
	auto& objects = level_.GetLevelObjects().GetObjects();
	if (selected_object_index_ >= (int)objects.size()) return;
	LevelObject& obj = objects[selected_object_index_];

	int mods  = glutGetModifiers();
	bool shift = (mods & GLUT_ACTIVE_SHIFT);
	bool ctrl  = (mods & GLUT_ACTIVE_CTRL);

	int dx = mouse_state_.prior_x_ - marker_manip_.start_x_;
	int dy = mouse_state_.prior_y_ - marker_manip_.start_y_;

	int fdx = input_.mouse_delta_x_;
	int fdy = input_.mouse_delta_y_;

	ManipulationMode current_mode = ManipulationMode::None;
	if (shift && ctrl)              current_mode = ManipulationMode::MoveXZ;
	else if (shift)                 current_mode = ManipulationMode::MoveXY;
	else if (ctrl)                  current_mode = ManipulationMode::MoveXZ;
	else if (input_.keys_ & OE_MK_MANIP_A) current_mode = ManipulationMode::RotateAlpha;
	else if (input_.keys_ & OE_MK_MANIP_B) current_mode = ManipulationMode::RotateBeta;
	else if (input_.keys_ & OE_MK_MANIP_G) current_mode = ManipulationMode::RotateGamma;
	else                            current_mode = ManipulationMode::None;

	if (marker_manip_.mode_ != ManipulationMode::None && current_mode != ManipulationMode::None &&
	    current_mode != marker_manip_.mode_) {
		marker_manip_.start_pos_ = obj.pos;
		marker_manip_.start_x_   = mouse_state_.prior_x_;
		marker_manip_.start_y_   = mouse_state_.prior_y_;
	}

	marker_manip_.mode_ = current_mode;

	if (marker_manip_.mode_ != ManipulationMode::None) {
		if (!undo_state_pushed_for_manip_) {
			PushUndoState();
			undo_state_pushed_for_manip_ = true;
		}
	} else {
		undo_state_pushed_for_manip_ = false;
	}

	const float moveSensitivity = 200.0f;
	const float rotSensitivity  = 0.008f;

	glm::dvec3 oldPos = obj.pos;
	glm::dvec3 oldRot = obj.rot;

	if (marker_manip_.mode_ == ManipulationMode::MoveXY) {
		glm::vec3 right   = viewer_.right_;
		glm::vec3 forward = glm::normalize(glm::vec3(viewer_.forward_.x, viewer_.forward_.y, 0.0f));
		obj.pos = marker_manip_.start_pos_ +
		          glm::dvec3(right   * (float)dx  * moveSensitivity +
		                     forward * (float)-dy * moveSensitivity);
	} else if (marker_manip_.mode_ == ManipulationMode::MoveXZ) {
		glm::vec3 right = viewer_.right_;
		glm::vec3 up    = glm::vec3(0, 0, 1);
		obj.pos = marker_manip_.start_pos_ +
		          glm::dvec3(right * (float)dx  * moveSensitivity +
		                     up   * (float)-dy  * moveSensitivity);
	} else if (marker_manip_.mode_ == ManipulationMode::RotateAlpha) {
		obj.rot.x += (float)fdx * rotSensitivity;
		marker_manip_.start_rot_.x = obj.rot.x;
	} else if (marker_manip_.mode_ == ManipulationMode::RotateBeta) {
		obj.rot.y += (float)fdx * rotSensitivity;
		marker_manip_.start_rot_.y = obj.rot.y;
	} else if (marker_manip_.mode_ == ManipulationMode::RotateGamma) {
		obj.rot.z += (float)fdx * rotSensitivity;
		marker_manip_.start_rot_.z = obj.rot.z;
	}

	if (input_.keys_ & OE_MK_MANIP_S) {
		bool isUnderground = Utils::IsUndergroundModel(obj.name, obj.modelId) || (obj.type == "Underground");
		float terrainZ = 0.0f;
		if (level_.GetTerrainZ(obj.pos.x, obj.pos.y, terrainZ, isUnderground)) {
			float zOffset     = renderer_.GetMeshZOffset(obj.modelId, obj.isBuilding);
			obj.snap_z_offset = isUnderground ? 0.0 : (double)(zOffset * 40.96f * obj.scale);
			obj.pos.z         = (double)terrainZ + obj.snap_z_offset;
			obj.modified      = true;
		}
	}

	if (input_.keys_ & OE_MK_MANIP_O) {
		int nearestIdx  = -1;
		double minDist  = 1e10;
		auto& objList   = level_.GetLevelObjects().GetObjects();
		for (int i = 0; i < (int)objList.size(); ++i) {
			if (i == selected_object_index_ || objList[i].deleted) continue;
			double d = glm::distance(obj.pos, objList[i].pos);
			if (d < minDist) { minDist = d; nearestIdx = i; }
		}
		if (nearestIdx >= 0) {
			const LevelObject& tgt = objList[nearestIdx];
			float tgtZOff   = renderer_.GetMeshZOffset(tgt.modelId, tgt.isBuilding);
			glm::vec3 tgtExt = renderer_.GetMeshExtents(tgt.modelId, tgt.isBuilding);
			double tgtTop   = tgt.pos.z + (double)((-tgtZOff + 2.0f * tgtExt.z) * 40.96f * tgt.scale);
			float selZOff   = renderer_.GetMeshZOffset(obj.modelId, obj.isBuilding);
			obj.snap_z_offset = (double)(selZOff * 40.96f * obj.scale);
			obj.pos.z         = tgtTop + obj.snap_z_offset;
			obj.modified      = true;
			Logger::Get().Log(LogLevel::INFO, "[App] Snapped object on top of: " + tgt.type);
		}
	}

	if (input_.keys_ & OE_MK_MANIP_SPACE) {
		obj.rot      = glm::vec3(0.0f);
		obj.modified = true;
	}

	glm::dvec3 deltaPos = obj.pos - oldPos;
	glm::dvec3 deltaRot = obj.rot - oldRot;

	bool changed = (std::abs(deltaPos.x) > 1e-6 || std::abs(deltaPos.y) > 1e-6 || std::abs(deltaPos.z) > 1e-6 ||
	                std::abs(deltaRot.x) > 1e-6 || std::abs(deltaRot.y) > 1e-6 || std::abs(deltaRot.z) > 1e-6);

	if (marker_manip_.mode_ != ManipulationMode::None) {
		char buf[128];
		if (marker_manip_.mode_ == ManipulationMode::MoveXY)
			snprintf(buf, sizeof(buf), "Moving to XY Plane with X: %.2f Y: %.2f Z: %.2f", obj.pos.x, obj.pos.y, obj.pos.z);
		else if (marker_manip_.mode_ == ManipulationMode::MoveXZ)
			snprintf(buf, sizeof(buf), "Moving to XZ Plane with X: %.2f Y: %.2f Z: %.2f", obj.pos.x, obj.pos.y, obj.pos.z);
		else if (marker_manip_.mode_ == ManipulationMode::RotateAlpha)
			snprintf(buf, sizeof(buf), "Rotation Alpha: %.6f", obj.rot.x);
		else if (marker_manip_.mode_ == ManipulationMode::RotateBeta)
			snprintf(buf, sizeof(buf), "Rotation Beta: %.6f", obj.rot.y);
		else
			snprintf(buf, sizeof(buf), "Rotation Gamma: %.6f", obj.rot.z);
		status_message_ = buf;
	} else {
		status_message_.clear();
	}

	if (changed || (input_.keys_ & OE_MK_MANIP_S) || (input_.keys_ & OE_MK_MANIP_O) || (input_.keys_ & OE_MK_MANIP_SPACE)) {
		glm::dmat3 deltaWorld = BuildRotMatZXY(obj.rot) * glm::transpose(BuildRotMatZXY(oldRot));
		PropagateTransformToChildren(selected_object_index_, deltaPos, deltaWorld, oldPos);
		obj.modified = true;
		level_.GetLevelObjects().UpdateCoordinatesInLine(obj);
		if (task_editor_open_) edit_string_ = obj.qscLine;
	}

	if (dx != 0 || dy != 0) obj.modified = true;
	if (obj.modified) level_.GetLevelObjects().UpdateCoordinatesInLine(obj);
}

// ── App::StripQuotes ──────────────────────────────────────────────────────────
std::string App::StripQuotes(const std::string& s) {
	if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
		return s.substr(1, s.size() - 2);
	return s;
}

// ── App::IsPropFieldMultiline ─────────────────────────────────────────────────
bool App::IsPropFieldMultiline(int field) const {
	if (field == PropPanel::kAIScriptTextField) return true;
	if (field < 0) return false;
	int oi = (prop_edit_obj_index_ >= 0) ? prop_edit_obj_index_ : selected_object_index_;
	if (oi < 0) return false;
	const auto& objects = level_.GetLevelObjects().GetObjects();
	if (oi >= (int)objects.size()) return false;
	const TaskSchema* scp = GetSchema(objects[oi].type);
	if (!scp) return false;
	int fi = field / 3;
	if (fi < 0 || fi >= (int)scp->size()) return false;
	const std::string& tn = (*scp)[fi].typeName;
	return tn == "VarString" || tn == "String256";
}

// ── App::UpdateAIScriptScroll / UpdateAIScriptPathHScroll ─────────────────────
// These helpers duplicate AiTextLineStarts/AiScriptMaxChars from imgui_panels.cpp
// (static helpers can't be shared across TUs without a header).
static std::vector<int> AiTextLineStarts_OE(const std::string& txt, int max_chars) {
	std::vector<int> s;
	s.push_back(0);
	for (int i = 0; i < (int)txt.size(); ) {
		if (txt[i] == '\n') { s.push_back(i + 1); i++; }
		else {
			int cnt = 0;
			while (i < (int)txt.size() && txt[i] != '\n' && cnt < max_chars) { i++; cnt++; }
			if (i < (int)txt.size() && txt[i] != '\n') s.push_back(i);
		}
	}
	return s;
}

static int AiScriptMaxChars_OE() {
	return std::max(1, (PropPanel::kWidth - 2 * PropPanel::kPad - 6) / 7);
}

void App::UpdateAIScriptScroll() {
	if (prop_text_edit_field_ != PropPanel::kAIScriptTextField) return;
	const int mc = AiScriptMaxChars_OE(), box_lines = 12;
	auto starts = AiTextLineStarts_OE(prop_text_buf_, mc);
	int cl = (int)(std::upper_bound(starts.begin(), starts.end(), prop_text_caret_) - starts.begin()) - 1;
	cl = std::max(0, std::min(cl, (int)starts.size() - 1));
	if (cl < ai_script_vscroll_)
		ai_script_vscroll_ = cl;
	else if (cl >= ai_script_vscroll_ + box_lines)
		ai_script_vscroll_ = cl - box_lines + 1;
}

void App::UpdateAIScriptPathHScroll() {
	if (prop_text_edit_field_ != PropPanel::kAIScriptPathField) return;
	const int mc = AiScriptMaxChars_OE();
	if (prop_text_caret_ < ai_script_path_hscroll_)
		ai_script_path_hscroll_ = prop_text_caret_;
	if (prop_text_caret_ >= ai_script_path_hscroll_ + mc)
		ai_script_path_hscroll_ = prop_text_caret_ - mc + 1;
}

// ── App::LoadAIScriptForSelected ──────────────────────────────────────────────
void App::LoadAIScriptForSelected() {
	if (ai_script_dirty_)
		status_message_ = "Warning: unsaved AI script edits discarded (save level first)";
	ai_script_path_.clear();
	ai_script_text_.clear();
	ai_script_dirty_        = false;
	ai_script_vscroll_      = 0;
	ai_script_path_hscroll_ = 0;

	if (selected_object_index_ < 0) return;
	const auto& objects = level_.GetLevelObjects().GetObjects();
	if (selected_object_index_ >= (int)objects.size()) return;
	const auto& obj = objects[selected_object_index_];

	if (ai_model_ids_.find(obj.modelId) == ai_model_ids_.end()) return;

	const LevelObject* aiTask = nullptr;
	if (obj.type == "HumanAI") {
		aiTask = &obj;
	} else {
		for (int ci : obj.childrenIndices) {
			if (ci < 0 || ci >= (int)objects.size()) continue;
			if (objects[ci].deleted) continue;
			if (objects[ci].type == "HumanAI") { aiTask = &objects[ci]; break; }
		}
	}
	if (!aiTask || aiTask->taskId.empty()) return;

	int levelNo = level_.GetLevelNo();
	std::string aiDir = Utils::GetIGIRootPath() +
	                    "\\missions\\location0\\level" + std::to_string(levelNo) + "\\ai";
	ai_script_path_ = aiDir + "\\" + aiTask->taskId + ".qvm";

	if (!std::filesystem::exists(ai_script_path_)) {
		ai_script_text_ = "// .qvm not found: " + ai_script_path_;
		return;
	}
	QVMFile qvm = QVM_Parse(ai_script_path_);
	if (!qvm.valid) {
		ai_script_text_ = "// decompile failed (invalid QVM): " + ai_script_path_;
		return;
	}
	ai_script_text_ = QVM_DecompileToString(qvm);
}

// ── App::CommitPropTextEdit ───────────────────────────────────────────────────
void App::CommitPropTextEdit() {
	if (prop_text_edit_field_ == -1) return;

	if (prop_text_edit_field_ == PropPanel::kAIScriptPathField) {
		prop_text_edit_field_ = -1;
		ai_script_path_ = prop_text_buf_;
		if (!ai_script_path_.empty() && std::filesystem::exists(ai_script_path_)) {
			QVMFile qvm = QVM_Parse(ai_script_path_);
			ai_script_text_ = qvm.valid ? QVM_DecompileToString(qvm)
			                            : "// decompile failed: " + ai_script_path_;
		} else {
			ai_script_text_ = "// file not found: " + ai_script_path_;
		}
		ai_script_dirty_ = false;
		return;
	}

	if (prop_text_edit_field_ == PropPanel::kAIScriptTextField) {
		prop_text_edit_field_ = -1;
		ai_script_text_  = prop_text_buf_;
		ai_script_dirty_ = true;
		return;
	}

	int field             = prop_text_edit_field_;
	prop_text_edit_field_ = -1;
	int oi                = (prop_edit_obj_index_ >= 0) ? prop_edit_obj_index_ : selected_object_index_;
	prop_edit_obj_index_  = -1;
	if (oi < 0) return;
	auto& objects = level_.GetLevelObjects().GetObjects();
	if (oi >= (int)objects.size()) return;
	auto& obj = objects[oi];

	if (field == -2) {
		obj.name = prop_text_buf_;
		if (obj.argTokens.size() > 2)
			obj.argTokens[2] = "\"" + StripQuotes(prop_text_buf_) + "\"";
		obj.modified = true;
		level_.GetLevelObjects().UpdateCoordinatesInLine(obj);
		return;
	}

	const TaskSchema* scp = GetSchema(obj.type);
	if (!scp) return;
	int fi = field / 3, comp = field % 3;
	if (fi >= (int)scp->size()) return;
	const FieldDef& fd = (*scp)[fi];
	int argIdx = fd.argOffset + comp;
	if (argIdx < 0 || argIdx >= (int)obj.argTokens.size()) return;

	const std::string& tn = fd.typeName;
	bool is_str = (tn.find("String") != std::string::npos || tn == "VarString" ||
	               tn == "EnumString32" || tn == "DropDownCombo");
	bool is_int = (tn == "Int16" || tn == "Int32" || tn == "EnumInt32");
	bool is_pos = (tn == "ObjectPos");

	std::string tokenVal;
	if (is_str) {
		bool hadQuotes = obj.argTokens[argIdx].size() >= 2 && obj.argTokens[argIdx].front() == '"';
		std::string body = StripQuotes(prop_text_buf_);
		tokenVal = (hadQuotes || tn.find("String") != std::string::npos || tn == "DropDownCombo")
		               ? ("\"" + body + "\"") : body;
	} else if (is_int) {
		long v = 0; try { v = std::lround(std::stod(prop_text_buf_)); } catch(...) {}
		char buf[64]; snprintf(buf, sizeof(buf), "%ld", v); tokenVal = buf;
	} else {
		double v = 0; try { v = std::stod(prop_text_buf_); } catch(...) {}
		char buf[64]; snprintf(buf, sizeof(buf), "%.6f", v); tokenVal = buf;
	}
	obj.argTokens[argIdx] = tokenVal;

	bool is_ori_field   = (tn == "Real32x9");
	bool is_gamma_field = ((tn == "Real32" || tn == "Angle" || tn == "Degrees") &&
	                       (fd.name == "Gamma" || fd.name == "Heading"));
	if (is_ori_field) {
		if (comp == 0 && fd.argOffset < (int)obj.argTokens.size())
			try { obj.rot.x = std::stod(obj.argTokens[fd.argOffset]); } catch(...) {}
		if (comp == 1 && fd.argOffset + 1 < (int)obj.argTokens.size())
			try { obj.rot.y = std::stod(obj.argTokens[fd.argOffset + 1]); } catch(...) {}
		if (comp == 2 && fd.argOffset + 2 < (int)obj.argTokens.size())
			try { obj.rot.z = std::stod(obj.argTokens[fd.argOffset + 2]); } catch(...) {}
	} else if (is_gamma_field) {
		if (fd.argOffset < (int)obj.argTokens.size())
			try { obj.rot.z = std::stod(obj.argTokens[fd.argOffset]); } catch(...) {}
	}

	if (is_pos) {
		double v = 0; try { v = std::stod(prop_text_buf_); } catch(...) {}
		if      (comp == 0) obj.pos.x = v;
		else if (comp == 1) obj.pos.y = v;
		else                obj.pos.z = v;
	}

	bool is_model_field = is_str && (fd.name == "Model" ||
	                                 fd.name.find("Model") != std::string::npos);
	if (is_model_field) {
		obj.modelId = StripQuotes(prop_text_buf_);
		if (!level_res_models_.Empty() && !obj.modelId.empty() &&
		    !level_res_models_.Contains(obj.modelId)) {
			obj.modelMissingInRes = true;
			status_message_ = "Model '" + obj.modelId +
				"' is not in this level's .res — it will be invisible in-game. "
				"Press Ctrl+Shift+A to add it.";
		} else {
			obj.modelMissingInRes = false;
		}
	}
	if (obj.type == "GunPickup" || obj.type == "AmmoPickup") {
		std::string enumStr = StripQuotes(prop_text_buf_);
		if (enumStr.rfind("WEAPON_ID_", 0) == 0 || enumStr.rfind("AMMO_ID_", 0) == 0)
			obj.modelId = level_.GetLevelObjects().ResolvePickupModelId(enumStr);
	}
	if (is_model_field && !obj.modelId.empty()) {
		DrawProgressOverlay(("Loading model '" + obj.modelId + "'").c_str(), 40, "mesh & textures");
		renderer_.PreloadModel(obj.modelId, obj.isBuilding);
	}

	obj.modified = true;
	level_.GetLevelObjects().UpdateCoordinatesInLine(obj);
}

// ── App::PushUndoState ────────────────────────────────────────────────────────
void App::PushUndoState() {
	auto& objects = level_.GetLevelObjects().GetObjects();
	object_undo_stack_.push_back(objects);
	object_redo_stack_.clear();
	if (object_undo_stack_.size() > 20)
		object_undo_stack_.erase(object_undo_stack_.begin());
}

// ── App::SaveAndReloadObjects ─────────────────────────────────────────────────
void App::SaveAndReloadObjects() {
	level_.SaveAndReloadObjects();
	EvaluateTrainTrackPositions();
	SnapObjectsToTerrain();
	RebuildLevelModelIds();
}

// ── App::RebuildLevelModelIds ─────────────────────────────────────────────────
void App::RebuildLevelModelIds() {
	level_model_ids_.clear();
	level_.GetLevelObjects().LoadModelNames();
	for (const auto& pair : level_.GetLevelObjects().GetModelNamesMap()) {
		const std::string& m = pair.first;
		bool ok = m.size() >= 7;
		if (ok) {
			for (char c : m) if (!isdigit(c) && c != '_') { ok = false; break; }
		}
		if (ok) level_model_ids_.insert(m);
	}
}

// ── App::Undo / Redo ──────────────────────────────────────────────────────────
void App::Undo() {
	if (object_undo_stack_.empty()) { status_message_ = "Nothing to undo"; return; }
	auto& objects = level_.GetLevelObjects().GetObjects();
	object_redo_stack_.push_back(objects);
	objects = object_undo_stack_.back();
	object_undo_stack_.pop_back();
	SaveAndReloadObjects();
	status_message_ = "Undo";
}

void App::Redo() {
	if (object_redo_stack_.empty()) { status_message_ = "Nothing to redo"; return; }
	auto& objects = level_.GetLevelObjects().GetObjects();
	object_undo_stack_.push_back(objects);
	objects = object_redo_stack_.back();
	object_redo_stack_.pop_back();
	SaveAndReloadObjects();
	status_message_ = "Redo";
}

// ── App::PropagateTransformToChildren ─────────────────────────────────────────
void App::PropagateTransformToChildren(int parentIdx, const glm::dvec3& deltaPos, const glm::dmat3& deltaWorld, const glm::dvec3& pivot) {
	auto& objects = level_.GetLevelObjects().GetObjects();
	std::vector<int> children = objects[parentIdx].childrenIndices;

	for (int childIdx : children) {
		LevelObject& child = objects[childIdx];
		glm::dvec3 relPos = child.pos - pivot;
		child.pos = pivot + glm::dvec3(deltaWorld * relPos) + deltaPos;
		child.rot = ExtractEulerZXY(deltaWorld * BuildRotMatZXY(child.rot));
		if (child.type == "HumanSoldier" || child.type == "HumanSoldierFemale")
			child.graphPos += deltaPos;
		child.modified = true;
		level_.GetLevelObjects().UpdateCoordinatesInLine(child);
		PropagateTransformToChildren(childIdx, deltaPos, deltaWorld, pivot);
	}
}

// ── App::PickObjectAtScreenPos ────────────────────────────────────────────────
int App::PickObjectAtScreenPos(int screen_x, int screen_y) {
	const auto& objects = level_.GetLevelObjects().GetObjects();
	if (objects.empty()) return -1;
	int w = window_state_.viewport_width_;
	int h = window_state_.viewport_height_;
	if (w == 0 || h == 0) return -1;
	return renderer_.PickObjectAtScreen(
		screen_x, screen_y, w, h,
		view_define_,
		objects,
		renderer_.DRAW_OBJECTS | renderer_.DRAW_BUILDINGS | renderer_.DRAW_PROPS
	);
}

// ── App::PromoteAttaToObject ──────────────────────────────────────────────────
void App::PromoteAttaToObject(int entry) {
	AttaPickEntry e;
	if (!renderer_.GetAttaPickEntry(entry, e)) return;

	auto& objects = level_.GetLevelObjects().GetObjects();
	PushUndoState();

	const glm::mat3& m = e.worldRot;
	float sx = std::max(-1.0f, std::min(1.0f, m[1][2]));
	float rx = std::asin(sx);
	float ry, rz;
	if (std::fabs(std::cos(rx)) > 1e-4f) {
		ry = std::atan2(-m[0][2], m[2][2]);
		rz = std::atan2(-m[1][0], m[1][1]);
	} else {
		ry = 0.0f;
		rz = std::atan2(m[0][1], m[0][0]);
	}

	LevelObject obj;
	obj.type      = "EditRigidObj";
	obj.name      = "ATTA_PROXY:" + e.immediateParentModelId + ":" + std::to_string(e.recordIndex);
	obj.taskId    = "-1";
	obj.modelId   = e.modelId;
	obj.pos       = glm::dvec3(e.worldPos);
	obj.rot       = glm::vec3(rx, ry, rz);
	obj.scale     = (e.scale > 0.f) ? e.scale : 1.0f;
	obj.isBuilding  = false;
	obj.deleted     = false;
	obj.modified    = false;
	obj.isAttaProxy       = true;
	obj.attaRecordIndex   = e.recordIndex;
	obj.attaParentModelId = e.immediateParentModelId;
	obj.attaIsBuilding    = false;
	obj.attaInvParentMat  = glm::inverse(e.parentWorldMat);

	int newIdx = (int)objects.size();
	objects.push_back(obj);
	renderer_.MarkAttaPromotedByRecord(e.immediateParentModelId, e.recordIndex);

	selected_object_index_ = newIdx;
	marker_manip_.start_pos_ = objects[newIdx].pos;
	marker_manip_.start_rot_ = objects[newIdx].rot;
	status_message_ = "Editing ATTA '" + e.modelId + "' — move then Save to apply to .res";
	Logger::Get().Log(LogLevel::INFO,
		"[App] ATTA '" + e.modelId + "' selected for direct MEF edit"
		" (record " + std::to_string(e.recordIndex) + " in " + e.immediateParentModelId + ".mef)");
}

// ── App::IsComputer / IsWaterTower / ValidateParentChildCompatibility ─────────
bool App::IsComputer(const LevelObject& obj) {
	auto to_lower = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
		return s;
	};
	std::string name         = to_lower(obj.name);
	std::string type         = to_lower(obj.type);
	std::string modelId      = to_lower(obj.modelId);
	std::string qscFuncName  = to_lower(obj.qscFuncName);
	std::string friendlyName = to_lower(level_.GetLevelObjects().GetModelName(obj.modelId));

	return (name.find("computer") != std::string::npos ||
	        type.find("computer") != std::string::npos ||
	        modelId.find("computer") != std::string::npos ||
	        qscFuncName.find("computer") != std::string::npos ||
	        friendlyName.find("computer") != std::string::npos);
}

bool App::IsWaterTower(const LevelObject& obj) {
	auto to_lower = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
		return s;
	};
	std::string name         = to_lower(obj.name);
	std::string type         = to_lower(obj.type);
	std::string modelId      = to_lower(obj.modelId);
	std::string qscFuncName  = to_lower(obj.qscFuncName);
	std::string friendlyName = to_lower(level_.GetLevelObjects().GetModelName(obj.modelId));

	return (name.find("watertower") != std::string::npos || name.find("water_tower") != std::string::npos ||
	        type.find("watertower") != std::string::npos || type.find("water_tower") != std::string::npos ||
	        modelId.find("watertower") != std::string::npos || modelId.find("water_tower") != std::string::npos ||
	        qscFuncName.find("watertower") != std::string::npos || qscFuncName.find("water_tower") != std::string::npos ||
	        friendlyName.find("watertower") != std::string::npos || friendlyName.find("water_tower") != std::string::npos);
}

bool App::ValidateParentChildCompatibility(const LevelObject& parent, const std::vector<LevelObject>& addedSubtree) {
	if (!IsWaterTower(parent)) return true;
	for (const auto& obj : addedSubtree) {
		if (IsComputer(obj)) return false;
	}
	return true;
}

// ── Model lookup / copy helpers ───────────────────────────────────────────────
void App::LookupSelectedModelName() {
	auto& objects = level_.GetLevelObjects().GetObjects();
	int idx = GetLookupObjectIndex(hover_object_index_, selected_object_index_);
	if (idx < 0 || idx >= (int)objects.size()) {
		SetLookupStatus(status_message_, "[App] Model lookup: no hovered/selected object");
		return;
	}
	const auto& obj = objects[idx];
	std::string name = level_.GetLevelObjects().GetModelName(obj.modelId);
	if (name.empty()) {
		SetLookupStatus(status_message_, "[App] Model lookup: no friendly name for model ID " + obj.modelId);
		return;
	}
	SetLookupStatus(status_message_, "[App] Model lookup: " + obj.modelId + " -> " + name);
}

void App::LookupSelectedModelId() {
	auto& objects = level_.GetLevelObjects().GetObjects();
	int idx = GetLookupObjectIndex(hover_object_index_, selected_object_index_);
	if (idx < 0 || idx >= (int)objects.size()) {
		SetLookupStatus(status_message_, "[App] Model lookup: no hovered/selected object");
		return;
	}
	const auto& obj = objects[idx];
	std::string name = level_.GetLevelObjects().GetModelName(obj.modelId);
	if (name.empty()) name = obj.name;
	if (name.empty()) {
		SetLookupStatus(status_message_, "[App] Model lookup: object has no readable model name");
		return;
	}
	std::string modelId = level_.GetLevelObjects().GetModelId(name);
	if (modelId.empty()) {
		SetLookupStatus(status_message_, "[App] Model lookup: no model id for name \"" + name + "\"");
		return;
	}
	SetLookupStatus(status_message_, "[App] Model lookup: " + name + " -> " + modelId);
}

void App::CopySelectedModelName() {
	auto& objects = level_.GetLevelObjects().GetObjects();
	int idx = GetLookupObjectIndex(hover_object_index_, selected_object_index_);
	if (idx < 0 || idx >= (int)objects.size()) {
		SetLookupStatus(status_message_, "[App] Model copy: no hovered/selected object");
		return;
	}
	const auto& obj = objects[idx];
	std::string name = level_.GetLevelObjects().GetModelName(obj.modelId);
	if (name.empty()) {
		SetLookupStatus(status_message_, "[App] Model copy: no friendly name for model ID " + obj.modelId);
		return;
	}
	Utils::SetClipboardText(name);
	SetLookupStatus(status_message_, "[App] Copied model name: " + name);
}

void App::CopySelectedModelId() {
	auto& objects = level_.GetLevelObjects().GetObjects();
	int idx = GetLookupObjectIndex(hover_object_index_, selected_object_index_);
	if (idx < 0 || idx >= (int)objects.size()) {
		SetLookupStatus(status_message_, "[App] Model copy: no hovered/selected object");
		return;
	}
	const auto& obj = objects[idx];
	std::string name = level_.GetLevelObjects().GetModelName(obj.modelId);
	if (name.empty()) name = obj.name;
	if (name.empty()) {
		SetLookupStatus(status_message_, "[App] Model copy: object has no readable model name");
		return;
	}
	std::string modelId = level_.GetLevelObjects().GetModelId(name);
	if (modelId.empty()) {
		SetLookupStatus(status_message_, "[App] Model copy: no model id for name \"" + name + "\"");
		return;
	}
	Utils::SetClipboardText(modelId);
	SetLookupStatus(status_message_, "[App] Copied model id: " + modelId);
}

void App::LookupHoveredModelName() { LookupSelectedModelName(); }
void App::LookupHoveredModelId()   { LookupSelectedModelId();   }

void App::SearchModelById(std::optional<std::string> query) {
	std::string searchId;
	if (query.has_value()) {
		searchId = query.value();
	} else {
		auto prompt = Utils::PromptForText("Search Model by ID", "Enter Model ID to search in IGIModels.json (e.g. 419_01_1):", "");
		if (!prompt.has_value()) return;
		searchId = prompt.value();
	}
	searchId = Utils::Trim(searchId);
	if (searchId.empty()) return;

	auto entries = LoadAllModelsFromJson();
	std::vector<ModelEntry> matches;
	for (const auto& entry : entries) {
		if (containsIgnoreCaseOE(entry.modelId, searchId)) matches.push_back(entry);
	}

	std::string resultMessage;
	if (matches.empty()) {
		resultMessage = "No matching models found in IGIModels.json for ID: " + searchId;
	} else {
		resultMessage = "Found " + std::to_string(matches.size()) + " matches in IGIModels.json:\n\n";
		int count = 0;
		for (const auto& match : matches) {
			if (count >= 25) { resultMessage += "... and " + std::to_string(matches.size() - count) + " more matches."; break; }
			resultMessage += "- ID: " + match.modelId + "  ->  Name: " + match.modelName + "\n";
			count++;
		}
	}
	MessageBoxA(NULL, resultMessage.c_str(), "IGIModels.json Search Results", MB_OK | MB_ICONINFORMATION);
}

void App::SearchModelByName(std::optional<std::string> query) {
	std::string searchName;
	if (query.has_value()) {
		searchName = query.value();
	} else {
		auto prompt = Utils::PromptForText("Search Model by Name", "Enter Model Name to search in IGIModels.json (e.g. Soldier):", "");
		if (!prompt.has_value()) return;
		searchName = prompt.value();
	}
	searchName = Utils::Trim(searchName);
	if (searchName.empty()) return;

	auto entries = LoadAllModelsFromJson();
	std::vector<ModelEntry> matches;
	for (const auto& entry : entries) {
		if (containsIgnoreCaseOE(entry.modelName, searchName)) matches.push_back(entry);
	}

	std::string resultMessage;
	if (matches.empty()) {
		resultMessage = "No matching models found in IGIModels.json for Name: " + searchName;
	} else {
		resultMessage = "Found " + std::to_string(matches.size()) + " matches in IGIModels.json:\n\n";
		int count = 0;
		for (const auto& match : matches) {
			if (count >= 25) { resultMessage += "... and " + std::to_string(matches.size() - count) + " more matches."; break; }
			resultMessage += "- Name: " + match.modelName + "  ->  ID: " + match.modelId + "\n";
			count++;
		}
	}
	MessageBoxA(NULL, resultMessage.c_str(), "IGIModels.json Search Results", MB_OK | MB_ICONINFORMATION);
}

// ── App::GetVisibleTreeNodes ──────────────────────────────────────────────────
std::vector<int> App::GetVisibleTreeNodes() {
	std::vector<int> visibleIndices;
	if (level_.GetLevelObjects().GetObjects().empty()) return visibleIndices;

	auto& objects = level_.GetLevelObjects().GetObjects();

	std::function<void(int)> traverse = [&](int idx) {
		if (idx < 0 || idx >= (int)objects.size()) return;
		const auto& obj = objects[idx];
		if (obj.deleted) return;
		visibleIndices.push_back(idx);
		if (obj.expanded) {
			for (int childIdx : obj.childrenIndices) traverse(childIdx);
		}
	};

	std::vector<int> root_decls, root_others;
	for (int i = 0; i < (int)objects.size(); ++i) {
		if (objects[i].parentIndex == -1 && !objects[i].deleted) {
			if (objects[i].type == "Task_DeclareParameters") root_decls.push_back(i);
			else root_others.push_back(i);
		}
	}

	if (!root_decls.empty()) {
		visibleIndices.push_back(-2);
		if (tree_decl_expanded_) {
			for (int idx : root_decls) traverse(idx);
		}
	}
	for (int idx : root_others) traverse(idx);

	return visibleIndices;
}
