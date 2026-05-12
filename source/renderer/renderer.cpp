/******************************************************************************
 * @file    renderer.cpp
 * @brief   main renderer
 *   GL 4.1: need manually set binding point of uniform blocks
 *                             texture unit of sample object
 *   GL 4.5: binding point, texture unit can specified in shaders
 *****************************************************************************/

#include "pch.h"
#include "logger.h"
#include <glm/gtc/type_ptr.hpp>


#include <freeglut.h>

/*
================================================================================
 Renderer
================================================================================
*/
Renderer::Renderer():
	ubo_mats_(0),
	ubo_fog_(0)
{
	LoadBuildingNames();
}

Renderer::~Renderer() {
	Shutdown();
}

void Renderer::LoadBuildingNames() {
	char appDataPath[1024];
	GetEnvironmentVariableA("APPDATA", appDataPath, 1024);
	std::string jsonPath = std::string(appDataPath) + "\\QEditor\\IGIModelsLevel.json";

	FILE* f = fopen(jsonPath.c_str(), "rb");
	if (!f) {
		std::cerr << "[Renderer] Failed to open IGIModelsLevel.json at: " << jsonPath << std::endl;
		return;
	}

	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = new char[fileSize + 1];
	fread(buf, 1, fileSize, f);
	buf[fileSize] = '\0';
	fclose(f);

	std::string content(buf);
	delete[] buf;

	// Parse IGIModelsLevel.json structure
	// Format: { "Level 1": { "Objects": [...], "Buildings": [...] }, ... }
	size_t pos = 0;
	while ((pos = content.find("\"Level ", pos)) != std::string::npos) {
		size_t levelEnd = content.find("\"", pos + 7);
		if (levelEnd == std::string::npos) break;
		std::string levelStr = content.substr(pos + 7, levelEnd - pos - 7);
		int levelNum = std::stoi(levelStr);

		// Find Objects array for this level
		size_t objectsPos = content.find("\"Objects\"", levelEnd);
		if (objectsPos != std::string::npos) {
			size_t arrayStart = content.find("[", objectsPos);
			size_t arrayEnd = content.find("]", arrayStart);
			if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
				std::string objectsArray = content.substr(arrayStart, arrayEnd - arrayStart + 1);
				ParseLevelObjects(objectsArray, levelNum, false);
			}
		}

		// Find Buildings array for this level
		size_t buildingsPos = content.find("\"Buildings\"", levelEnd);
		if (buildingsPos != std::string::npos) {
			size_t arrayStart = content.find("[", buildingsPos);
			size_t arrayEnd = content.find("]", arrayStart);
			if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
				std::string buildingsArray = content.substr(arrayStart, arrayEnd - arrayStart + 1);
				ParseLevelObjects(buildingsArray, levelNum, true);
			}
		}

		pos = levelEnd + 1;
	}

	Logger::Get().Log(LogLevel::INFO, "[Renderer] Loaded " + std::to_string(building_names_.size()) + " building names and " + std::to_string(task_ids_.size()) + " task IDs from IGIModelsLevel.json");
}

void Renderer::ParseLevelObjects(const std::string& arrayContent, int levelNum, bool isBuilding) {
	size_t pos = 0;
	while ((pos = arrayContent.find("\"ModelID\"", pos)) != std::string::npos) {
		size_t idStart = arrayContent.find("\"", pos + 10) + 1;
		size_t idEnd = arrayContent.find("\"", idStart);
		if (idStart == std::string::npos || idEnd == std::string::npos) break;

		std::string modelId = arrayContent.substr(idStart, idEnd - idStart);

		// Find boundary of current JSON object (next ModelID or end)
		size_t nextModelIdPos = arrayContent.find("\"ModelID\"", idEnd);
		size_t objectEnd = (nextModelIdPos != std::string::npos) ? nextModelIdPos : arrayContent.length();

		// Find Name within this object only
		size_t namePos = arrayContent.find("\"Name\"", idEnd);
		if (namePos != std::string::npos && namePos < objectEnd) {
			size_t nameStart = arrayContent.find("\"", namePos + 7) + 1;
			size_t nameEnd = arrayContent.find("\"", nameStart);
			if (nameStart != std::string::npos && nameEnd != std::string::npos) {
				std::string name = arrayContent.substr(nameStart, nameEnd - nameStart);
				std::string key = std::to_string(levelNum) + "_" + modelId;
				building_names_[key] = name;
			}
		}

		// Find TaskID within this object only
		size_t taskIdPos = arrayContent.find("\"TaskID\"", idEnd);
		if (taskIdPos != std::string::npos && taskIdPos < objectEnd) {
			size_t taskIdStart = arrayContent.find("\"", taskIdPos + 9) + 1;
			size_t taskIdEnd = arrayContent.find("\"", taskIdStart);
			if (taskIdStart != std::string::npos && taskIdEnd != std::string::npos) {
				std::string taskId = arrayContent.substr(taskIdStart, taskIdEnd - taskIdStart);
				std::string key = std::to_string(levelNum) + "_" + modelId;
				task_ids_[key] = taskId;
			}
		}

		pos = idEnd + 1;
	}
}

std::string Renderer::GetBuildingName(const std::string& modelId) {
	std::string key = std::to_string(current_level_) + "_" + modelId;
	auto it = building_names_.find(key);
	if (it != building_names_.end()) {
		return it->second;
	}
	// Fallback to non-level-specific lookup for compatibility
	it = building_names_.find(modelId);
	if (it != building_names_.end()) {
		return it->second;
	}
	return "";
}

std::string Renderer::GetTaskId(const std::string& modelId) {
	std::string key = std::to_string(current_level_) + "_" + modelId;
	auto it = task_ids_.find(key);
	if (it != task_ids_.end()) {
		return it->second;
	}
	return "0";
}

bool Renderer::Init() {
	ubo_mats_ = GL_CreateBuffer(GL_UNIFORM_BUFFER, sizeof(ubo_mats_s), nullptr, GL_DYNAMIC_DRAW);
	ubo_fog_ = GL_CreateBuffer(GL_UNIFORM_BUFFER, sizeof(ubo_fog_s), nullptr, GL_STATIC_DRAW);

	if (!skydome_.Init()) {
		return false;
	}

	if (!flat_sky_layers_.Init()) {
		return false;
	}

	if (!terrain_.Init()) {
		return false;
	}

	if (!objects_.Init()) {
		return false;
	}


	// init default state
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glDepthRangef(RENDER_DEPTH_MIN, RENDER_DEPTH_MAX);

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

#if defined(_WIN32)
	glPolygonOffset(-1.0f, -1.0f);
#else
	glPolygonOffset(-64.0f, -64.0f);	// tune this
#endif
	glDisable(GL_POLYGON_OFFSET_LINE);

	GL_TryEnableVSync();

	return true;
}

void Renderer::Shutdown() {
	terrain_.Shutdown();
	objects_.Shutdown();
	flat_sky_layers_.Shutdown();

	skydome_.Shutdown();

	GL_DeleteBuffer(ubo_fog_);
	GL_DeleteBuffer(ubo_mats_);
}

void Renderer::BeginLoadLevel() {
	flat_sky_layers_.UnloadAllTexs();
	terrain_.UnloadAllTexs();
}

void Renderer::SetupClearColor(const glm::vec4& color) {
	glClearColor(color.r, color.g, color.b, 1.0f);
}

void Renderer::SetupFog(const glm::vec4& color, float fog_far) {
	ubo_fog_s ubo_fog;

	ubo_fog.color_ = color;
	ubo_fog.far_ = fog_far;

	GL_BufferData(ubo_fog_, GL_UNIFORM_BUFFER, sizeof(ubo_fog), &ubo_fog, GL_STATIC_DRAW);
}

void Renderer::SetupSkydome(const skydome_define_s& d) {
	skydome_.UpdateVertices(d);
}

void Renderer::LoadFlatSkyLayerTex(int layer_no, const pic_s* pic) {
	flat_sky_layers_.LoadLayerTex(layer_no, pic);
}

void Renderer::LoadTerrainMatTex(const pic_s* pic) {
	terrain_.LoadMatTex(pic);
}

void Renderer::LoadTerrainLMPTex(const pic_s* pic) {
	terrain_.LoadLMPTex(pic);
}

vert_flat_sky_layer_s* Renderer::MapFlatSkyLayersVB() {
	return flat_sky_layers_.MapVB();
}

void Renderer::UnmapFlatSkyLayersVB() {
	flat_sky_layers_.UnmapVB();
}

vert_pos_a_uv_s* Renderer::MapTerrainVB() {
	return terrain_.MapVB();
}

void Renderer::UnmapTerrainVB() {
	terrain_.UnmapVB();
}

uint32_t* Renderer::MapTerrainIB() {
	return terrain_.MapIB();
}

void Renderer::UnmapTerrainIB() {
	terrain_.UnmapIB();
}

render_chunk_s* Renderer::GetTerrainRenderChunckBuffer() {
	return terrain_.GetRenderChunckBuffer();
}

void Renderer::Draw(const draw_params_s& params, const hud_params_s& hud) {
    static bool logged_params = false;
    if (!logged_params) {
        Logger::Get().Log(LogLevel::INFO, "[Renderer] Draw Params: draw_parts=" + std::to_string(params.draw_parts_) + " level_objects=" + (params.level_objects_ ? "VALID" : "NULL"));
        logged_params = true;
    }
    SetupUBOMats(*params.view_define_);


        // start draw
        glDepthMask(GL_TRUE);   // insure clear depth buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, params.view_define_->viewport_width_, params.view_define_->viewport_height_);

        if (params.draw_parts_ & DRAW_SKYDOME) {
                skydome_.Draw(ubo_mats_, params.overlay_wireframe_);
        }

        if ((params.draw_parts_ & DRAW_FLAT_SKY_LAYER) && params.flat_sky_layer_is_visible_) {
                flat_sky_layers_.Draw(ubo_mats_, params.overlay_wireframe_);
        }

        if (params.draw_parts_ & DRAW_TERRAIN) {
                terrain_.Draw(ubo_mats_, ubo_fog_, params.overlay_wireframe_, params.draw_terrain_options_, params.num_terrain_render_chunk_);      
        }

        if ((params.draw_parts_ & (DRAW_OBJECTS | DRAW_BUILDINGS | DRAW_PROPS)) && params.level_objects_) {
                glMatrixMode(GL_PROJECTION);
                glLoadMatrixf(glm::value_ptr(mat_proj_));
                glMatrixMode(GL_MODELVIEW);
                glLoadMatrixf(glm::value_ptr(mat_view_));
                objects_.Draw(ubo_mats_, params.overlay_wireframe_, params.level_objects_->GetObjects(), params.selected_object_index_, params.draw_parts_);
        }



        if (hud.show_hud_) {
                glUseProgram(0); // Disable any active shaders for fixed-function HUD
                glBindVertexArray(0); // UNBIND VAO to prevent state leak
                glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0); // UNBIND UBO
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
                GL_CHECK_ERROR;

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glDisable(GL_LIGHTING);
                glDisable(GL_TEXTURE_2D);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, params.view_define_->viewport_width_, 0, params.view_define_->viewport_height_, -1, 1);

                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();

                auto draw_text = [&](int x, int y, const char* str, float r, float g, float b) {
                        // Draw shadow
                        glColor3f(0.0f, 0.0f, 0.0f);
                        glRasterPos2i(x + 1, params.view_define_->viewport_height_ - y - 1);
                        for (const char* c = str; *c != '\0'; c++) {
                                glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
                        }
                        // Draw main text
                        glColor3f(r, g, b);
                        glRasterPos2i(x, params.view_define_->viewport_height_ - y);
                        for (const char* c = str; *c != '\0'; c++) {
                                glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
                        }
                };

                int line_y = 30;
                draw_text(20, line_y, "--- IGI EDITOR ---", 1.0f, 1.0f, 1.0f); line_y += 15;
                
                float status_r = 1.0f, status_g = 1.0f, status_b = 0.0f; // Yellow
                if (hud.status_msg_.find("CONNECTED") != std::string::npos) { status_r = 0.0f; status_g = 1.0f; status_b = 0.0f; } // Green
                draw_text(20, line_y, hud.status_msg_.c_str(), status_r, status_g, status_b); line_y += 15;

                // Show Editor (Player) Position
                char buf[256];
                sprintf(buf, "EDITOR POSITION X: %.2f Y: %.2f Z: %.2f", hud.raw_pos_.x, hud.raw_pos_.y, hud.raw_pos_.z);
                draw_text(20, line_y, buf, 1.0f, 1.0f, 1.0f); line_y += 15;

                sprintf(buf, "EDITOR YAW: %.3f PITCH: %.3f ROLL: %.3f", hud.cam_yaw_, hud.cam_pitch_, hud.cam_roll_);
                draw_text(20, line_y, buf, 0.5f, 1.0f, 0.5f); line_y += 15;

                // Show Selected Object Position if an object is selected
                if (hud.selected_object_index_ >= 0 && hud.level_objects_) {
                        const auto& objects = hud.level_objects_->GetObjects();
                        if (hud.selected_object_index_ < (int)objects.size()) {
                                const auto& obj = objects[hud.selected_object_index_];
                                sprintf(buf, "SELECTED OBJECT X: %.2f Y: %.2f Z: %.2f", obj.pos.x, obj.pos.y, obj.pos.z);
                                draw_text(20, line_y, buf, 1.0f, 1.0f, 0.0f); line_y += 15;

                                sprintf(buf, "OBJECT YAW: %.2f PITCH: %.2f ROLL: %.2f", obj.rot.z, obj.rot.x, obj.rot.y);
                                draw_text(20, line_y, buf, 0.5f, 1.0f, 0.5f); line_y += 15;
                        }
                }
                
                sprintf(buf, "ANGLE H: %.3f V: %.3f", hud.view_h_, hud.view_v_);
                draw_text(20, line_y, buf, 1.0f, 0.5f, 0.0f); line_y += 15;

                sprintf(buf, "LEVEL: %d | FOV: %.1f", hud.game_level_, hud.cam_fov_);
                draw_text(20, line_y, buf, 0.7f, 0.7f, 1.0f); line_y += 15;
                
                draw_text(20, line_y, "Checks: 0", 1.0f, 1.0f, 1.0f);

                // Display object info at mouse position
                int info_object_index = hud.edit_mode_ ? hud.selected_object_index_ : hud.hover_object_index_;
                if (info_object_index >= 0 && hud.level_objects_) {
                        const auto& objects = hud.level_objects_->GetObjects();
                        if (info_object_index < (int)objects.size()) {
                                const auto& obj = objects[info_object_index];

                                char buf[512];
                                std::string display_name = obj.name;
                                if (display_name.empty()) {
                                        display_name = GetBuildingName(obj.modelId);
                                }
                                if (display_name.empty()) {
                                        display_name = obj.modelId;
                                }

                                std::string task_id = obj.taskId.empty() ? "-1" : obj.taskId;

                                int text_x = hud.mouse_x_;
                                int text_y = hud.mouse_y_ + 25;
                                if (text_x < 0) text_x = 0;
                                if (text_x > params.view_define_->viewport_width_ - 200) text_x = params.view_define_->viewport_width_ - 200;
                                if (text_y < 0) text_y = 0;
                                if (text_y > params.view_define_->viewport_height_ - 50) text_y = params.view_define_->viewport_height_ - 50;

                                // Line 1: ModelName ID: Task_New first param
                                snprintf(buf, sizeof(buf), "%s ID: %s", display_name.c_str(), task_id.c_str());
                                draw_text(text_x, text_y, buf, 1.0f, 1.0f, 1.0f);

                                // Line 2: Full ModelID
                                text_y += 15;
                                snprintf(buf, sizeof(buf), "%s", obj.modelId.c_str());
                                draw_text(text_x, text_y, buf, obj.isBuilding ? 1.0f : 0.0f, 1.0f, 0.0f);
                        }
                }

                if (hud.edit_mode_) {
                        // Flip Y because glOrtho has y=0 at bottom, but mouse_y is top-down (GLUT)
                        float cx = (float)(hud.mouse_x_);
                        float cy = (float)(params.view_define_->viewport_height_ - hud.mouse_y_);

                        if (hud.terrain_edit_enabled_) {
                                // Terrain edit mode: orange circle brush cursor
                                float radius = 10.0f;
                                float th = 2.0f;
                                glColor3f(1.0f, 0.5f, 0.0f); // Orange
                                glLineWidth(th);
                                glBegin(GL_LINE_LOOP);
                                int segments = 16;
                                for (int i = 0; i < segments; ++i) {
                                        float angle = (float)i * 6.283185f / (float)segments;
                                        glVertex2f(cx + cosf(angle) * radius, cy + sinf(angle) * radius);
                                }
                                glEnd();
                                // Small cross inside circle
                                float csz = 4.0f;
                                glBegin(GL_LINES);
                                glVertex2f(cx - csz, cy); glVertex2f(cx + csz, cy);
                                glVertex2f(cx, cy - csz); glVertex2f(cx, cy + csz);
                                glEnd();
                                glLineWidth(1.0f);
                        } else {
                                // Object edit mode: green + icon at mouse cursor
                                float sz = 7.0f;
                                float th = 2.5f;
                                glColor3f(0.0f, 1.0f, 0.0f);
                                glLineWidth(th);
                                glBegin(GL_LINES);
                                glVertex2f(cx - sz, cy); glVertex2f(cx + sz, cy);
                                glVertex2f(cx, cy - sz); glVertex2f(cx, cy + sz);
                                glEnd();
                                glLineWidth(1.0f);
                        }
                } else {
                        // Draw a small blue camera icon at the screen center
                        float cx = (float)(params.view_define_->viewport_width_ / 2 + 12);
                        float cy = (float)(params.view_define_->viewport_height_ / 2 + 12);
                        float w = 12.0f, h = 8.0f;
                        glColor3f(0.4f, 0.7f, 1.0f);
                        glLineWidth(2.0f);
                        // Camera body
                        glBegin(GL_LINE_LOOP);
                        glVertex2f(cx - w/2, cy - h/2);
                        glVertex2f(cx + w/2, cy - h/2);
                        glVertex2f(cx + w/2, cy + h/2);
                        glVertex2f(cx - w/2, cy + h/2);
                        glEnd();
                        // Lens (inner rect)
                        glBegin(GL_LINE_LOOP);
                        glVertex2f(cx - w/5, cy - h/5);
                        glVertex2f(cx + w/5, cy - h/5);
                        glVertex2f(cx + w/5, cy + h/5);
                        glVertex2f(cx - w/5, cy + h/5);
                        glEnd();
                        // Viewfinder bump on top
                        glBegin(GL_LINE_LOOP);
                        glVertex2f(cx - w/5, cy - h/2);
                        glVertex2f(cx + w/5, cy - h/2);
                        glVertex2f(cx + w/5 + 2, cy - h/2 - 4);
                        glVertex2f(cx - w/5 - 2, cy - h/2 - 4);
                        glEnd();
                        glLineWidth(1.0f);
                }

                if (hud.pause_mode_) {
                        int menu_w = 350;
                        int menu_h = 200;
                        int menu_x = (params.view_define_->viewport_width_ - menu_w) / 2;
                        int menu_y = (params.view_define_->viewport_height_ - menu_h) / 2;

                        // Draw semi-transparent background
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
                        glBegin(GL_QUADS);
                        glVertex2i(menu_x, menu_y);
                        glVertex2i(menu_x + menu_w, menu_y);
                        glVertex2i(menu_x + menu_w, menu_y + menu_h);
                        glVertex2i(menu_x, menu_y + menu_h);
                        glEnd();
                        glDisable(GL_BLEND);

                        // Draw outline with thicker lines
                        glLineWidth(2.0f);
                        glColor3f(1.0f, 1.0f, 0.0f);
                        glBegin(GL_LINE_LOOP);
                        glVertex2i(menu_x, menu_y);
                        glVertex2i(menu_x + menu_w, menu_y);
                        glVertex2i(menu_x + menu_w, menu_y + menu_h);
                        glVertex2i(menu_x, menu_y + menu_h);
                        glEnd();
                        glLineWidth(1.0f);

                        // Title
                        draw_text(menu_x + menu_w/2 - 50, menu_y + menu_h - 30, "PAUSE MENU", 1.0f, 1.0f, 0.0f);

                        // Menu items
                        draw_text(menu_x + 30, menu_y + menu_h - 60, "[ESC] RESUME", 1.0f, 1.0f, 1.0f);
                        draw_text(menu_x + 30, menu_y + menu_h - 85, "[S]   SAVE LEVEL", 1.0f, 1.0f, 1.0f);
                        draw_text(menu_x + 30, menu_y + menu_h - 110, "[=]   RESET LEVEL", 0.5f, 1.0f, 1.0f);
                        draw_text(menu_x + 30, menu_y + menu_h - 135, "[R]   RESET SCRIPT", 1.0f, 0.7f, 0.3f);
                        draw_text(menu_x + 30, menu_y + menu_h - 160, "[D]   DEBUG", 0.0f, 1.0f, 0.0f);
                        draw_text(menu_x + 30, menu_y + menu_h - 185, "[Q]   EXIT", 1.0f, 0.5f, 0.5f);


                }

                if (hud.show_debug_) {
                        const auto& entries = Logger::Get().GetEntries();
                        int startY = 250;
                        int count = 0;
                        for (auto it = entries.rbegin(); it != entries.rend() && count < 15; ++it, ++count) {
                                float r = 1.0f, g = 1.0f, b = 1.0f;
                                if (it->level == LogLevel::ERR) { r = 1.0f; g = 0.2f; b = 0.2f; }
                                else if (it->level == LogLevel::FATAL) { r = 1.0f; g = 0.0f; b = 0.0f; }
                                else if (it->level == LogLevel::WARNING) { r = 1.0f; g = 1.0f; b = 0.0f; }

                                draw_text(10, startY - count * 15, it->message.c_str(), r, g, b);
                        }
                }


                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();

                // Restore all states
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
                glEnable(GL_LIGHTING);
                glEnable(GL_TEXTURE_2D);
                glDisable(GL_BLEND);
        }

	GL_CHECK_ERROR;

	glFlush();
}

void Renderer::SetupUBOMats(const view_define_s& vd) {
	glm::mat4 mat_proj_persp = glm::perspective(vd.fovy_, (float)vd.viewport_width_ / vd.viewport_height_, vd.render_z_near_, vd.render_z_far_);

	glm::vec3 scaled_down_pos = vd.pos_ * RENDERER_MODEL_SCALE_DOWN;
	glm::mat4 mat_view = glm::lookAt(scaled_down_pos, scaled_down_pos + vd.forward_ * WORLD_UNITS_PER_METER, vd.up_);
	glm::mat4 mat_follow_view = glm::lookAt(VEC3_ORIGIN, vd.forward_, vd.up_);
	glm::mat4 mat_scale = glm::scale(glm::mat4(1.0f), glm::vec3(RENDERER_MODEL_SCALE_DOWN));

	mat_proj_ = mat_proj_persp;
	mat_view_ = mat_view;

	// setup uniform buffer

	ubo_mats_s ubo_mats;

	// skydome
	ubo_mats.mvp_mat_follow_view_ = mat_proj_persp * mat_follow_view * mat_scale;

	// flat sky layer
	ubo_mats.mvp_flat_sky_layer_ = glm::ortho(0.0f, (float)vd.viewport_width_, 0.0f, (float)vd.viewport_height_, -1.0f, 1.0f);

	// terrain
	ubo_mats.mvp_objects_ = mat_proj_persp * mat_view * mat_scale;

	// flush ubo buffer
	GL_BufferData(ubo_mats_, GL_UNIFORM_BUFFER, sizeof(ubo_mats), &ubo_mats, GL_DYNAMIC_DRAW);
}
