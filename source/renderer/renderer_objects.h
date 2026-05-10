#pragma once

#include "pch.h"
#include "mef.h"
#include <unordered_map>
#include <string>
#include <vector>

struct render_object_s {
	glm::vec3 pos;
	float yaw;
	std::string model_id;
};

class Renderer_Objects {
public:
	Renderer_Objects();
	~Renderer_Objects();

	bool Init();
	void Shutdown();

	void AddObject(const glm::vec3& pos, float yaw, const char* model_id);
	void ClearObjects();

	void Draw(GLuint ubo_mats);

private:
	struct cached_model_s {
		GLuint vao;
		GLuint vbo;
		GLuint ibo;
		int index_count;
	};

	std::vector<render_object_s> objects_;
	std::unordered_map<std::string, cached_model_s> cached_models_;

	GLuint shader_prog_;

	bool LoadModel(const std::string& model_id);
};
