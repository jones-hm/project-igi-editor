/******************************************************************************
 * @file    app_devtools_ui.cpp
 * @brief   App: ImGui Dev Tools panel — Developer Mode/Debug Commands +
 *          qedconfig.qsc Settings. Split from app.cpp; shares app_internal.h.
 *****************************************************************************/
#include "app_internal.h"
#include <imgui.h>

void App::DrawDevToolsUI(bool* p_open) {
	ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Dev Tools", p_open)) {
		ImGui::End();
		return;
	}

	if (ImGui::CollapsingHeader("Developer Mode / Debug Commands", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool dev_mode = developer_mode_;
		if (ImGui::Checkbox("Developer Mode (Debug Command Watcher)", &dev_mode)) {
			developer_mode_ = dev_mode;
			if (developer_mode_) {
				debug_cmd_mgr_.Start();
				Logger::Get().Log(LogLevel::INFO, "[App] Developer Mode ON - Command Watcher Started");
			} else {
				debug_cmd_mgr_.Stop();
				Logger::Get().Log(LogLevel::INFO, "[App] Developer Mode OFF - Command Watcher Stopped");
			}
		}
		ImGui::TextDisabled("Watches an external commands file for GotoModel/CaptureModel/DeleteModel.");

		bool anim_debug = show_anim_debug_;
		if (ImGui::Checkbox("Animation Debug Overlay", &anim_debug)) {
			show_anim_debug_ = anim_debug;
		}
		ImGui::TextDisabled("Skeleton wireframe + status panel (also toggled together with the above by F10).");
	}

	if (ImGui::CollapsingHeader("Settings (qedconfig.qsc)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ConfigData& cfg = Config::Get();

		if (ImGui::TreeNodeEx("UI / Font", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::SliderFloat("Font Size", &cfg.fontSize, 8.0f, 32.0f, "%.0f")) Config::Save();
			float col[3] = { cfg.fontColorR / 255.0f, cfg.fontColorG / 255.0f, cfg.fontColorB / 255.0f };
			if (ImGui::ColorEdit3("Font Color", col)) {
				cfg.fontColorR = (int)(col[0] * 255.0f);
				cfg.fontColorG = (int)(col[1] * 255.0f);
				cfg.fontColorB = (int)(col[2] * 255.0f);
				Config::Save();
			}
			if (ImGui::Checkbox("Use Editor Font", &cfg.useEditorFont)) Config::Save();
			if (ImGui::SliderInt("System Font Size", &cfg.systemFontSize, 8, 32)) Config::Save();
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Checkbox("Enable Logging", &cfg.enableLogging)) Config::Save();
			if (ImGui::Checkbox("Debug Logging", &cfg.debugLogging)) Config::Save();
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Renderer / Visual", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Checkbox("Lightmaps", &cfg.enableLightmaps)) Config::Save();
			if (ImGui::Checkbox("Fog", &cfg.enableFog)) Config::Save();
			if (ImGui::SliderInt("Fog Intensity", &cfg.fogIntensity, 0, 200, "%d%%")) Config::Save();
			if (ImGui::Checkbox("Music", &cfg.musicEnabled)) Config::Save();
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Advanced")) {
			if (ImGui::InputInt("Console Auto-Activate", &cfg.consoleAutoActivate)) Config::Save();
			if (ImGui::InputInt("Search Type", &cfg.searchType)) Config::Save();
			if (ImGui::Checkbox("Invert Mouse", &cfg.invertMouse)) Config::Save();
			if (ImGui::Checkbox("Display Task Note", &cfg.displayTaskNote)) Config::Save();
			if (ImGui::Checkbox("Allow Dynamic Switching", &cfg.allowDynamicSwitching)) Config::Save();
			if (ImGui::Checkbox("Save Config On Exit", &cfg.saveConfigOnExit)) Config::Save();
			if (ImGui::Checkbox("Auto Save Enabled", &cfg.auto_save_enabled)) Config::Save();
			if (ImGui::SliderInt("Auto Save Interval (s)", &cfg.auto_save_interval_seconds, 10, 3600)) Config::Save();
			if (ImGui::Checkbox("Run Event", &cfg.runEvent)) Config::Save();
			if (ImGui::Checkbox("Camera Lock", &cfg.cameraLock)) Config::Save();
			if (ImGui::Checkbox("Enable Backup", &cfg.enableBackup)) Config::Save();
			if (ImGui::InputInt("Interpolation", &cfg.interpolation)) Config::Save();
			if (ImGui::InputFloat("Render Z-Near", &cfg.renderZNear)) Config::Save();
			if (ImGui::InputInt("Graph Node Size", &cfg.graphNodeSize)) Config::Save();
			ImGui::TreePop();
		}
	}

	ImGui::End();
}
