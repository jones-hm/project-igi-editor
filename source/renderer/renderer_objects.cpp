#include "pch.h"
#include "renderer_objects.h"
#include <glm/gtc/type_ptr.hpp>

Renderer_Objects::Renderer_Objects() : shader_prog_(0) {
}

Renderer_Objects::~Renderer_Objects() {
	Shutdown();
}

bool Renderer_Objects::Init() {
	char vert_path[1024];
	char frag_path[1024];

	Str_SPrintf(vert_path, 1024, "%s/object.vert", g_folders.shader_folder_);
	Str_SPrintf(frag_path, 1024, "%s/object.frag", g_folders.shader_folder_);

	const char* shader_filenames[SUPPORT_SHADER_COUNT] = {0};
	shader_filenames[VERTEX_SHADER] = vert_path;
	shader_filenames[FRAGMENT_SHADER] = frag_path;

	gl_program_s prog;
	if (!GL_CreateProgram(shader_filenames, prog)) {
		return false;
	}
	shader_prog_ = prog.program_;

	return true;
}

void Renderer_Objects::Shutdown() {
	if (shader_prog_) {
		glDeleteProgram(shader_prog_);
		shader_prog_ = 0;
	}

	for (auto& pair : cached_models_) {
		glDeleteBuffers(1, &pair.second.vbo);
		glDeleteBuffers(1, &pair.second.ibo);
		glDeleteVertexArrays(1, &pair.second.vao);
	}
	cached_models_.clear();
}

void Renderer_Objects::AddObject(const glm::vec3& pos, float yaw, const char* model_id, int level_no) {
	render_object_s obj;
	obj.pos = pos;
	obj.yaw = yaw;
	obj.model_id = model_id;
	objects_.push_back(obj);

	if (cached_models_.find(model_id) == cached_models_.end()) {
		LoadModel(model_id, level_no);
	}
}

void Renderer_Objects::ClearObjects() {
	objects_.clear();
}

bool Renderer_Objects::LoadModel(const std::string& model_id, int level_no) {
	char path[1024];
	mef_model_s mef;
	bool loaded = false;

	// Location 1: res/missions/location0/level<N>/models/<model>.mef
	Str_SPrintf(path, 1024, "%s/missions/location0/level%d/models/%s.mef", g_folders.res_folder_, level_no, model_id.c_str());
	if (MefLoader::LoadMEF(path, mef)) {
		loaded = true;
	}

	if (!loaded) {
		// Location 2: res/missions/location0/common/models/<model>.mef
		Str_SPrintf(path, 1024, "%s/missions/location0/common/models/%s.mef", g_folders.res_folder_, model_id.c_str());
		if (MefLoader::LoadMEF(path, mef)) {
			loaded = true;
		}
	}

	if (!loaded) {
		// Location 3: Config fallback
		Str_SPrintf(path, 1024, "%s%s.mef", g_folders.mef_folder_, model_id.c_str());
		if (MefLoader::LoadMEF(path, mef)) {
			loaded = true;
		}
	}

	if (!loaded) {
		return false;
	}

	cached_model_s cached;
	cached.index_count = 0;

	std::vector<uint16_t> indices;
	for (const auto& submesh : mef.submeshes) {
		for (const auto& face : submesh.faces) {
			indices.push_back(face.v0 + submesh.vertex_offset);
			indices.push_back(face.v1 + submesh.vertex_offset);
			indices.push_back(face.v2 + submesh.vertex_offset);
		}
	}
	cached.index_count = indices.size();

	if (cached.index_count == 0 || mef.vertices.empty()) {
		return false;
	}

	glGenVertexArrays(1, &cached.vao);
	glGenBuffers(1, &cached.vbo);
	glGenBuffers(1, &cached.ibo);

	glBindVertexArray(cached.vao);

	glBindBuffer(GL_ARRAY_BUFFER, cached.vbo);
	glBufferData(GL_ARRAY_BUFFER, mef.vertices.size() * sizeof(mef_vert_s), mef.vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cached.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t), indices.data(), GL_STATIC_DRAW);

	// pos
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(mef_vert_s), (void*)offsetof(mef_vert_s, pos));

	// normal
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(mef_vert_s), (void*)offsetof(mef_vert_s, normal));

	// uv0
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(mef_vert_s), (void*)offsetof(mef_vert_s, uv0));

	glBindVertexArray(0);

	cached_models_[model_id] = cached;

	return true;
}

void Renderer_Objects::Draw(GLuint ubo_mats) {
	if (objects_.empty()) return;

	glUseProgram(shader_prog_);

	// binding point 0 is used for ubo_mats (as configured in app)
	GLint ubo_idx = glGetUniformBlockIndex(shader_prog_, "ubo_mats");
	if (ubo_idx != GL_INVALID_INDEX) {
		glUniformBlockBinding(shader_prog_, ubo_idx, 0);
	}

	GLint u_model_mat = glGetUniformLocation(shader_prog_, "u_model_mat");

	for (const auto& obj : objects_) {
		auto it = cached_models_.find(obj.model_id);
		if (it == cached_models_.end()) continue;

		glm::mat4 model_mat = glm::translate(glm::mat4(1.0f), obj.pos);
		model_mat = glm::rotate(model_mat, obj.yaw, glm::vec3(0.0f, 0.0f, 1.0f));

		glUniformMatrix4fv(u_model_mat, 1, GL_FALSE, glm::value_ptr(model_mat));

		glBindVertexArray(it->second.vao);
		glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_SHORT, 0);
	}

	glBindVertexArray(0);
	glUseProgram(0);
}
