/******************************************************************************
 * @file    camera.cpp
 * @brief   Camera movement, viewer vectors, collision — App methods extracted
 *          from app.cpp. Includes pch.h for the full header graph.
 *****************************************************************************/

#include "../pch.h"
#include <freeglut.h>
#include "../logger.h"
#include "../utils.h"
#include "../config.h"
#include <map>
#include <cmath>

// ── File-scope constants used by ProcessInput ────────────────────────────────
constexpr float MOUSE_SENSITIVE_CAM = 0.2f;  // local alias; app.cpp defines MOUSE_SENSITIVE

// movement key flags (mirrored from app.cpp – same values, local to this TU)
constexpr int CAM_MK_FORWARD       = FLAG_BIT(0);
constexpr int CAM_MK_BACKWARD      = FLAG_BIT(1);
constexpr int CAM_MK_LEFT          = FLAG_BIT(2);
constexpr int CAM_MK_RIGHT         = FLAG_BIT(3);
constexpr int CAM_MK_STRAIGHT_UP   = FLAG_BIT(4);
constexpr int CAM_MK_STRAIGHT_DOWN = FLAG_BIT(5);
constexpr int CAM_MK_JUMP          = FLAG_BIT(6);
constexpr int CAM_MK_ROLL_INC      = FLAG_BIT(7);
constexpr int CAM_MK_ROLL_DEC      = FLAG_BIT(8);

constexpr float VIEW_HEIGHT_CAM    = 7000.0f;
constexpr float GRAVITE_CAM        = 10.0f * WORLD_UNITS_PER_METER;
constexpr float MIN_MOVE_SPEED_CAM = 8.0f   * WORLD_UNITS_PER_METER;
constexpr float MAX_MOVE_SPEED_CAM = 8192.0f * WORLD_UNITS_PER_METER;
constexpr float MIN_JUMP_SPEED_CAM = 4.0f   * WORLD_UNITS_PER_METER;

// ── BuildRotMatZXY / ExtractEulerZXY — shared with object_editor.cpp ─────────
// Forward-declared in app.cpp; definitions duplicated here and in object_editor.cpp
// would cause linker errors, so they are defined only once in camera.cpp and
// declared extern in object_editor.cpp.
glm::dmat3 BuildRotMatZXY(const glm::dvec3& euler) {
	glm::dmat4 m(1.0);
	m = glm::rotate(m, euler.z, glm::dvec3(0, 0, 1));
	m = glm::rotate(m, euler.x, glm::dvec3(1, 0, 0));
	m = glm::rotate(m, euler.y, glm::dvec3(0, 1, 0));
	return glm::dmat3(m);
}

// R[1][2]=sin(x); R[0][2]=-cx*sy, R[2][2]=cx*cy; R[1][0]=-sz*cx, R[1][1]=cz*cx
glm::dvec3 ExtractEulerZXY(const glm::dmat3& R) {
	double sin_x = glm::clamp((double)R[1][2], -1.0, 1.0);
	double angle_x = std::asin(sin_x);
	double angle_y, angle_z;
	if (std::abs(std::cos(angle_x)) > 1e-6) {
		angle_y = std::atan2(-R[0][2], R[2][2]);
		angle_z = std::atan2(-R[1][0], R[1][1]);
	} else {
		angle_y = 0.0;
		angle_z = std::atan2(R[0][1], R[0][0]);
	}
	return glm::dvec3(angle_x, angle_y, angle_z);
}

// ── App::ProcessInput ─────────────────────────────────────────────────────────
void App::ProcessInput(float delta_seconds) {
	// Safety check: ensure level is loaded before processing movement
	if (level_.GetLevelNo() == 0) {
		// Level not loaded, skip input processing
		input_.mouse_delta_x_ = 0;
		input_.mouse_delta_y_ = 0;
		return;
	}

	bool enableCameraMode = Utils::IsKeyBindingPressed(Config::Get().keyEnableCamera);

	// Update cursor — always NONE so SPR sprite cursor is the only visible cursor
	glutSetCursor(GLUT_CURSOR_NONE);

	if (!edit_mode_ || enableCameraMode) {
		if (orbit_active_) {
			// Horizontal orbit (yaw only) around selected object
			viewer_.yaw_ += -input_.mouse_delta_x_ * MOUSE_SENSITIVE_CAM;

			glm::vec3 new_forward;
			glm::vec3 dummy_right, dummy_up;
			AngleToVectors(viewer_.yaw_, viewer_.pitch_, viewer_.roll_, new_forward, dummy_right, dummy_up);

			// Recalculate position based on distance and new forward vector
			viewer_.pos_ = orbit_target_pos_ - new_forward * orbit_distance_;
		} else {
			// Standard free-look camera movement in-place
			viewer_.yaw_   += -input_.mouse_delta_x_ * MOUSE_SENSITIVE_CAM;
			viewer_.pitch_ += -input_.mouse_delta_y_ * MOUSE_SENSITIVE_CAM;
		}
	}

	input_.mouse_delta_x_ = 0;
	input_.mouse_delta_y_ = 0;

	UpdateViewerVectors();

	// Gradually increase/decrease camera movement speed based on active movement
	{
		bool is_fwd   = (input_.keys_ & CAM_MK_FORWARD)  || Utils::IsKeyBindingPressed(Config::Get().keyMoveCameraForward);
		bool is_bwd   = (input_.keys_ & CAM_MK_BACKWARD) || Utils::IsKeyBindingPressed(Config::Get().keyMoveCameraBackward);
		bool is_left  = (input_.keys_ & CAM_MK_LEFT);
		bool is_right = (input_.keys_ & CAM_MK_RIGHT);
		bool is_up    = (input_.keys_ & CAM_MK_STRAIGHT_UP);
		bool is_down  = (input_.keys_ & CAM_MK_STRAIGHT_DOWN);

		bool is_moving = is_fwd || is_bwd || is_left || is_right || is_up || is_down;

		if (is_moving) {
			viewer_.move_speed_ += (viewer_.move_speed_ + 8.0f * WORLD_UNITS_PER_METER) * 0.8f * delta_seconds;
			if (viewer_.move_speed_ > MAX_MOVE_SPEED_CAM)
				viewer_.move_speed_ = MAX_MOVE_SPEED_CAM;
		} else {
			viewer_.move_speed_ -= (viewer_.move_speed_ - MIN_MOVE_SPEED_CAM) * 3.0f * delta_seconds;
			if (viewer_.move_speed_ < MIN_MOVE_SPEED_CAM)
				viewer_.move_speed_ = MIN_MOVE_SPEED_CAM;
		}
	}

	if (viewer_.clip_to_z_ && !enableCameraMode) {

		float z0 = viewer_.pos_.z - VIEW_HEIGHT_CAM;
		float friction = viewer_.move_speed_ * 2.0f;

		bool is_fwd   = (input_.keys_ & CAM_MK_FORWARD)  || Utils::IsKeyBindingPressed(Config::Get().keyMoveCameraForward);
		bool is_bwd   = (input_.keys_ & CAM_MK_BACKWARD) || Utils::IsKeyBindingPressed(Config::Get().keyMoveCameraBackward);
		bool is_left  = (input_.keys_ & CAM_MK_LEFT);
		bool is_right = (input_.keys_ & CAM_MK_RIGHT);

		if (is_fwd) {
			viewer_.velocity_.x = viewer_.forward_.x * viewer_.move_speed_;
			viewer_.velocity_.y = viewer_.forward_.y * viewer_.move_speed_;
		}
		if (is_bwd) {
			viewer_.velocity_.x = viewer_.forward_.x * -viewer_.move_speed_;
			viewer_.velocity_.y = viewer_.forward_.y * -viewer_.move_speed_;
		}
		if (is_left) {
			viewer_.velocity_.x = viewer_.right_.x * -viewer_.move_speed_;
			viewer_.velocity_.y = viewer_.right_.y * -viewer_.move_speed_;
		}
		if (is_right) {
			viewer_.velocity_.x = viewer_.right_.x * viewer_.move_speed_;
			viewer_.velocity_.y = viewer_.right_.y * viewer_.move_speed_;
		}

		if (input_.keys_ & CAM_MK_JUMP && viewer_.clip_to_z_) {
			if (viewer_.velocity_.z <= 0.0f && viewer_.on_ground_) {
				viewer_.velocity_.z = viewer_.jump_speed_;
			}
		}

		if (!input_.keys_ && viewer_.on_ground_) {

			float speed = (float)std::sqrt(viewer_.velocity_.x * viewer_.velocity_.x + viewer_.velocity_.y * viewer_.velocity_.y);
			if (speed > 1.0f) {
				float one_over_speed = 1.0f / speed;
				float dir_x = viewer_.velocity_.x * one_over_speed;
				float dir_y = viewer_.velocity_.y * one_over_speed;

				float speed_drop = friction * delta_seconds;
				speed -= speed_drop;

				if (speed <= 1.0f) speed = 0.0f;

				viewer_.velocity_.x = dir_x * speed;
				viewer_.velocity_.y = dir_y * speed;
			} else {
				viewer_.velocity_.x = 0.0f;
				viewer_.velocity_.y = 0.0f;
			}
		}

		glm::vec3 move_delta = viewer_.velocity_ * delta_seconds;
		glm::vec3 next_pos   = viewer_.pos_ + move_delta;

		if (!CheckCollision(next_pos)) {
			viewer_.pos_ = next_pos;
		} else {
			// Sliding logic
			viewer_.velocity_.y = 0.0f;
		}

		if (viewer_.velocity_.z <= 0.0f) {

			float ret_z = 0.0f;
			bool ok = level_.GetTerrainZ(viewer_.pos_.x, viewer_.pos_.y, ret_z);

			if (ok) {
				float view_z = ret_z + VIEW_HEIGHT_CAM;
				if (viewer_.pos_.z < view_z) {
					viewer_.pos_.z      = view_z;
					viewer_.velocity_.z = 0.0f;
					viewer_.on_ground_  = true;
				} else {
					viewer_.on_ground_ = false;
				}
			} else {
				viewer_.on_ground_ = false;
			}
		} else {
			viewer_.on_ground_ = false; // moving upward
		}

		if (!viewer_.on_ground_) {
			viewer_.velocity_.z -= GRAVITE_CAM * delta_seconds;
		}

	} else {

		bool is_fwd   = (input_.keys_ & CAM_MK_FORWARD)  || Utils::IsKeyBindingPressed(Config::Get().keyMoveCameraForward);
		bool is_bwd   = (input_.keys_ & CAM_MK_BACKWARD) || Utils::IsKeyBindingPressed(Config::Get().keyMoveCameraBackward);
		bool is_left  = (input_.keys_ & CAM_MK_LEFT);
		bool is_right = (input_.keys_ & CAM_MK_RIGHT);

		if (is_fwd)  viewer_.pos_ += viewer_.forward_ * viewer_.move_speed_ * delta_seconds;
		if (is_bwd)  viewer_.pos_ -= viewer_.forward_ * viewer_.move_speed_ * delta_seconds;
		if (is_left) viewer_.pos_ -= viewer_.right_   * viewer_.move_speed_ * delta_seconds;
		if (is_right)viewer_.pos_ += viewer_.right_   * viewer_.move_speed_ * delta_seconds;

		if (input_.keys_ & CAM_MK_STRAIGHT_UP   && !viewer_.clip_to_z_)
			viewer_.pos_ += VEC3_Z_DIR * viewer_.move_speed_ * delta_seconds;
		if (input_.keys_ & CAM_MK_STRAIGHT_DOWN && !viewer_.clip_to_z_)
			viewer_.pos_ -= VEC3_Z_DIR * viewer_.move_speed_ * delta_seconds;
	}

	// check rotation
	bool update_orientation = false;
	if (input_.keys_ & CAM_MK_ROLL_INC) { update_orientation = true; viewer_.roll_ += 1.0f; }
	if (input_.keys_ & CAM_MK_ROLL_DEC) { update_orientation = true; viewer_.roll_ -= 1.0f; }
	if (update_orientation) UpdateViewerVectors();

	// CameraStrafeLeft/Right — lateral movement (held keys)
	{
		auto& ev = Config::Get().eventBindings_;
		auto CheckCont = [&](const std::string& n) {
			auto it = ev.find(n);
			return (it != ev.end()) && Utils::IsKeyBindingPressed(it->second);
		};
		if (CheckCont("CameraStrafeLeft"))
			viewer_.pos_ -= viewer_.right_ * viewer_.move_speed_ * delta_seconds;
		if (CheckCont("CameraStrafeRight"))
			viewer_.pos_ += viewer_.right_ * viewer_.move_speed_ * delta_seconds;
	}
}

// ── App::UpdateViewerVectors ──────────────────────────────────────────────────
void App::UpdateViewerVectors() {
	while (viewer_.yaw_ < 0.0f)   viewer_.yaw_ += 360.0f;
	while (viewer_.yaw_ > 360.0f) viewer_.yaw_ -= 360.0f;

	if (viewer_.pitch_ < -89.0f) viewer_.pitch_ = -89.0f;
	if (viewer_.pitch_ >  89.0f) viewer_.pitch_ =  89.0f;

	while (viewer_.roll_ < 0.0f)   viewer_.roll_ += 360.0f;
	while (viewer_.roll_ > 360.0f) viewer_.roll_ -= 360.0f;

	AngleToVectors(viewer_.yaw_, viewer_.pitch_, viewer_.roll_,
		viewer_.forward_, viewer_.right_, viewer_.up_);
}

// ── App::UpdateViewDefine ─────────────────────────────────────────────────────
void App::UpdateViewDefine() {
	view_define_.pos_     = viewer_.pos_;
	view_define_.forward_ = viewer_.forward_;
	view_define_.right_   = viewer_.right_;
	view_define_.up_      = viewer_.up_;
	view_define_.render_z_near_ = RENDER_Z_NEAR;

	view_define_.mat_rot_[0][0] =  view_define_.right_.x;
	view_define_.mat_rot_[1][0] =  view_define_.right_.y;
	view_define_.mat_rot_[2][0] =  view_define_.right_.z;

	view_define_.mat_rot_[0][1] = -view_define_.up_.x;
	view_define_.mat_rot_[1][1] = -view_define_.up_.y;
	view_define_.mat_rot_[2][1] = -view_define_.up_.z;

	view_define_.mat_rot_[0][2] = view_define_.forward_.x;
	view_define_.mat_rot_[1][2] = view_define_.forward_.y;
	view_define_.mat_rot_[2][2] = view_define_.forward_.z;
}

// ── App::CheckCollision ───────────────────────────────────────────────────────
bool App::CheckCollision(const glm::vec3& nextPos) {
	if (noclip_mode_) return false;
	if (level_.GetLevelNo() == 0) return false;

	auto& objects = level_.GetLevelObjects().GetObjects();

	float playerRadius = 400.0f;
	constexpr float BASE_SCALE            = 40.96f;
	constexpr float FALLBACK_RADIUS_MODEL = 200.0f;

	for (const auto& obj : objects) {
		float dist = glm::distance(nextPos, glm::vec3(obj.pos));
		if (dist > 150000.0f) continue;

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(obj.pos.x, obj.pos.y, obj.pos.z));
		model = glm::rotate(model, (float)obj.rot.z, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::rotate(model, (float)obj.rot.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, (float)obj.rot.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::scale(model, glm::vec3(BASE_SCALE * obj.scale));
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		glm::vec4 localPos = glm::inverse(model) * glm::vec4(nextPos, 1.0f);
		glm::vec3 extents  = renderer_.GetMeshExtents(obj.modelId, obj.isBuilding);

		float ex = extents.x, ey = extents.y, ez = extents.z;
		if (ex < 1.0f && ey < 1.0f && ez < 1.0f)
			ex = ey = ez = FALLBACK_RADIUS_MODEL;

		if (std::abs(localPos.x) < (ex + playerRadius / BASE_SCALE) &&
		    std::abs(localPos.y) < (ey + playerRadius / BASE_SCALE) &&
		    std::abs(localPos.z) < (ez + playerRadius / BASE_SCALE))
		{
			static int collisionLogCount = 0;
			if (collisionLogCount < 50) {
				Logger::Get().Log(LogLevel::INFO, "[App] Collision with model=" + obj.modelId +
				                  " type=" + (obj.isBuilding ? "building" : "object"));
				collisionLogCount++;
			}
			return true;
		}
	}
	return false;
}

// ── App::EvaluateTrainTrackPositions ─────────────────────────────────────────
void App::EvaluateTrainTrackPositions() {
	auto& objects = level_.GetLevelObjects().GetObjects();

	std::map<std::string, int> taskToIdx;
	for (int i = 0; i < (int)objects.size(); ++i) {
		if (!objects[i].taskId.empty())
			taskToIdx[objects[i].taskId] = i;
	}

	struct SplineData {
		std::vector<glm::dvec3> pts;
		std::vector<double>     cumDist;
		double                  totalLen = 0.0;
	};
	std::map<std::string, SplineData> splineCache;

	auto getSpline = [&](const std::string& id) -> const SplineData* {
		auto cached = splineCache.find(id);
		if (cached != splineCache.end()) return &cached->second;
		auto it = taskToIdx.find(id);
		if (it == taskToIdx.end()) return nullptr;
		const auto& spline = objects[it->second];
		if (spline.childrenIndices.size() < 2) return nullptr;
		SplineData sd;
		for (int ci : spline.childrenIndices)
			sd.pts.push_back(objects[ci].pos);
		sd.cumDist.resize(sd.pts.size(), 0.0);
		for (int i = 1; i < (int)sd.pts.size(); ++i)
			sd.cumDist[i] = sd.cumDist[i-1] + glm::length(sd.pts[i] - sd.pts[i-1]);
		sd.totalLen = sd.cumDist.back();
		splineCache[id] = sd;
		return &splineCache[id];
	};

	auto evalOnSpline = [](const SplineData& sd, double arcLen, glm::dvec3& outPos, glm::dvec3& outRot) {
		double clamped = glm::clamp(arcLen, 0.0, sd.totalLen);
		int seg = 0;
		for (int i = 1; i < (int)sd.cumDist.size(); ++i) {
			if (sd.cumDist[i] >= clamped) { seg = i - 1; break; }
			if (i == (int)sd.cumDist.size() - 1) { seg = i - 1; break; }
		}
		int segNext = std::min(seg + 1, (int)sd.pts.size() - 1);
		double segLen = sd.cumDist[segNext] - sd.cumDist[seg];
		double t = (segLen > 0.0) ? (clamped - sd.cumDist[seg]) / segLen : 0.0;
		outPos = sd.pts[seg] + t * (sd.pts[segNext] - sd.pts[seg]);
		glm::dvec3 fwd = glm::normalize(sd.pts[segNext] - sd.pts[seg]);
		outRot.z = atan2(-fwd.y, -fwd.x);
		outRot.x = asin(glm::clamp(-fwd.z, -1.0, 1.0));
		outRot.y = 0.0;
	};

	std::vector<double> rawRailPos(objects.size(), 0.0);
	for (int i = 0; i < (int)objects.size(); ++i) {
		if (objects[i].type == "Train" && !objects[i].deleted && !objects[i].splineTaskId.empty())
			rawRailPos[i] = objects[i].pos.x;
	}

	std::map<int, double> arcByObjIdx;
	int evaluated = 0;

	// Pass 1: trains with explicit non-zero 1D position from QSC
	for (int i = 0; i < (int)objects.size(); ++i) {
		auto& obj = objects[i];
		if (obj.type != "Train" || obj.deleted || obj.splineTaskId.empty()) continue;
		if (rawRailPos[i] == 0.0) continue;

		const SplineData* sd = getSpline(obj.splineTaskId);
		if (!sd || sd->totalLen <= 0.0) continue;

		double rawPos = rawRailPos[i];
		double arcPos = (rawPos < 0.0) ? sd->totalLen + rawPos : rawPos;
		arcPos = glm::clamp(arcPos, 0.0, sd->totalLen);
		arcByObjIdx[i] = arcPos;

		glm::dvec3 newPos, newRot;
		evalOnSpline(*sd, arcPos, newPos, newRot);
		obj.pos = newPos; obj.original_pos = newPos;
		obj.rot = newRot; obj.original_rot = newRot;
		++evaluated;
	}

	// Pass 2: child wagons with position=0 — place them behind parent lead car
	const double WAGON_SPACING = 52756.0;
	std::map<int, int> wagonCountByParent;

	for (int i = 0; i < (int)objects.size(); ++i) {
		auto& obj = objects[i];
		if (obj.type != "Train" || obj.deleted || obj.splineTaskId.empty()) continue;
		if (rawRailPos[i] != 0.0) continue;
		if (obj.parentIndex < 0 || objects[obj.parentIndex].type != "Train") continue;

		auto parentArcIt = arcByObjIdx.find(obj.parentIndex);
		if (parentArcIt == arcByObjIdx.end()) continue;

		const SplineData* sd = getSpline(obj.splineTaskId);
		if (!sd || sd->totalLen <= 0.0) continue;

		int wagonIdx = wagonCountByParent[obj.parentIndex]++;
		double dir   = (rawRailPos[obj.parentIndex] < 0.0) ? 1.0 : -1.0;
		double arcPos = glm::clamp(parentArcIt->second + dir * (wagonIdx + 1) * WAGON_SPACING,
		                           0.0, sd->totalLen);
		arcByObjIdx[i] = arcPos;

		glm::dvec3 newPos, newRot;
		evalOnSpline(*sd, arcPos, newPos, newRot);
		obj.pos = newPos; obj.original_pos = newPos;
		obj.rot = newRot; obj.original_rot = newRot;
		++evaluated;
	}

	if (evaluated > 0)
		Logger::Get().Log(LogLevel::INFO, "[App] Evaluated track positions for " +
		                  std::to_string(evaluated) + " train objects.");
}
