void App::UpdateMarkerManipulation() {
	if (selected_object_index_ < 0) return;
	auto& objects = level_.GetLevelObjects().GetObjects();
	if (selected_object_index_ >= (int)objects.size()) return;
	LevelObject& obj = objects[selected_object_index_];

	int mods = glutGetModifiers();
	bool shift = (mods & GLUT_ACTIVE_SHIFT);
	bool ctrl = (mods & GLUT_ACTIVE_CTRL);

	// Delta in screen space from start of drag
	int dx = mouse_state_.prior_x_ - marker_manip_.start_x_;
	int dy = mouse_state_.prior_y_ - marker_manip_.start_y_;

	// Mode selection logic (matches IGI 2 editor behavior)
	if (shift && ctrl) marker_manip_.mode_ = ManipulationMode::MoveXZ;
	else if (shift) marker_manip_.mode_ = ManipulationMode::MoveXY;
	else if (ctrl) marker_manip_.mode_ = ManipulationMode::MoveXZ;
	else if (input_.keys_ & MK_MANIP_A) marker_manip_.mode_ = ManipulationMode::RotateAlpha;
	else if (input_.keys_ & MK_MANIP_B) marker_manip_.mode_ = ManipulationMode::RotateBeta;
	else if (input_.keys_ & MK_MANIP_G) marker_manip_.mode_ = ManipulationMode::RotateGamma;
	else marker_manip_.mode_ = ManipulationMode::None;

	// Sensitivity constants
	const float moveSensitivity = 200.0f; // World units per pixel drag
	const float rotSensitivity = 0.01f;   // Radians per pixel drag

	if (marker_manip_.mode_ == ManipulationMode::MoveXY) {
		// Move on XY plane relative to camera view
		glm::vec3 right = viewer_.right_;
		glm::vec3 forward = glm::normalize(glm::vec3(viewer_.forward_.x, viewer_.forward_.y, 0.0f));
		obj.pos = marker_manip_.start_pos_ + glm::dvec3(right * (float)dx * moveSensitivity + forward * (float)-dy * moveSensitivity);
	}
	else if (marker_manip_.mode_ == ManipulationMode::MoveXZ) {
		// Move on Screen-Right and Screen-Up (approximates XZ plane relative to camera)
		glm::vec3 right = viewer_.right_;
		glm::vec3 up = glm::vec3(0, 0, 1);
		obj.pos = marker_manip_.start_pos_ + glm::dvec3(right * (float)dx * moveSensitivity + up * (float)-dy * moveSensitivity);
	}
	else if (marker_manip_.mode_ == ManipulationMode::RotateAlpha) {
		obj.rot.x = marker_manip_.start_rot_.x + (float)dx * rotSensitivity;
	}
	else if (marker_manip_.mode_ == ManipulationMode::RotateBeta) {
		obj.rot.y = marker_manip_.start_rot_.y + (float)dx * rotSensitivity;
	}
	else if (marker_manip_.mode_ == ManipulationMode::RotateGamma) {
		obj.rot.z = marker_manip_.start_rot_.z + (float)dx * rotSensitivity;
	}

	// Instantaneous actions (one-off checks while dragging)
	if (input_.keys_ & MK_MANIP_S) {
		float terrainZ = 0.0f;
		if (level_.GetTerrainZ(glm::vec3(obj.pos), terrainZ)) {
			float zOffset = renderer_.GetMeshZOffset(obj.modelId);
			obj.pos.z = (double)terrainZ + (double)(zOffset * 40.96f * obj.scale);
		}
	}
	if (input_.keys_ & MK_MANIP_SPACE) {
		obj.rot = glm::vec3(0.0f);
	}
}
