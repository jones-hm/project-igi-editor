#pragma once

#include "pch.h"

struct mef_vert_s {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv0;
	glm::vec2 uv1;
};

struct mef_face_s {
	uint16_t v0, v1, v2;
};

struct mef_submesh_s {
	glm::vec3 offset;
	std::vector<mef_face_s> faces;
	int vertex_offset;
	int vertex_count;
};

struct mef_model_s {
	uint32_t type;
	std::vector<mef_vert_s> vertices;
	std::vector<mef_submesh_s> submeshes;
};

class MefLoader {
public:
	static bool LoadMEF(const char* filepath, mef_model_s& out_model);
};
