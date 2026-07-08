/******************************************************************************
 * @file    imgui_glut_backend.cpp
 * @brief   See imgui_glut_backend.h
 *****************************************************************************/
#include "pch.h"
#include "imgui_glut_backend.h"
#include <imgui.h>
#include <freeglut.h>

static int    g_LastFrameTimeMs = 0;
static double g_TimeAccumulator = 0.0;

static void UpdateKeyModifiers() {
	ImGuiIO& io = ImGui::GetIO();
	int mods = glutGetModifiers();
	io.AddKeyEvent(ImGuiMod_Ctrl,  (mods & GLUT_ACTIVE_CTRL)  != 0);
	io.AddKeyEvent(ImGuiMod_Shift, (mods & GLUT_ACTIVE_SHIFT) != 0);
	io.AddKeyEvent(ImGuiMod_Alt,   (mods & GLUT_ACTIVE_ALT)   != 0);
}

// Maps a non-printable ASCII control character (as delivered by GLUT's
// regular keyboard callback) to an ImGuiKey, or ImGuiKey_None if it should
// just be treated as text input instead.
static ImGuiKey AsciiToImGuiKey(unsigned char key) {
	switch (key) {
		case 8:   return ImGuiKey_Backspace;
		case 9:   return ImGuiKey_Tab;
		case 13:  return ImGuiKey_Enter;
		case 27:  return ImGuiKey_Escape;
		case 127: return ImGuiKey_Delete;
		default:  return ImGuiKey_None;
	}
}

static ImGuiKey SpecialToImGuiKey(int key) {
	switch (key) {
		case GLUT_KEY_F1:        return ImGuiKey_F1;
		case GLUT_KEY_F2:        return ImGuiKey_F2;
		case GLUT_KEY_F3:        return ImGuiKey_F3;
		case GLUT_KEY_F4:        return ImGuiKey_F4;
		case GLUT_KEY_F5:        return ImGuiKey_F5;
		case GLUT_KEY_F6:        return ImGuiKey_F6;
		case GLUT_KEY_F7:        return ImGuiKey_F7;
		case GLUT_KEY_F8:        return ImGuiKey_F8;
		case GLUT_KEY_F9:        return ImGuiKey_F9;
		case GLUT_KEY_F10:       return ImGuiKey_F10;
		case GLUT_KEY_F11:       return ImGuiKey_F11;
		case GLUT_KEY_F12:       return ImGuiKey_F12;
		case GLUT_KEY_LEFT:      return ImGuiKey_LeftArrow;
		case GLUT_KEY_RIGHT:     return ImGuiKey_RightArrow;
		case GLUT_KEY_UP:        return ImGuiKey_UpArrow;
		case GLUT_KEY_DOWN:      return ImGuiKey_DownArrow;
		case GLUT_KEY_PAGE_UP:   return ImGuiKey_PageUp;
		case GLUT_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
		case GLUT_KEY_HOME:      return ImGuiKey_Home;
		case GLUT_KEY_END:       return ImGuiKey_End;
		case GLUT_KEY_INSERT:    return ImGuiKey_Insert;
		default:                 return ImGuiKey_None;
	}
}

void ImGui_ImplGlut_Init() {
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = "imgui_impl_glut (custom)";
	io.DisplaySize = ImVec2((float)glutGet(GLUT_WINDOW_WIDTH), (float)glutGet(GLUT_WINDOW_HEIGHT));
	g_LastFrameTimeMs = glutGet(GLUT_ELAPSED_TIME);
}

void ImGui_ImplGlut_Shutdown() {
}

void ImGui_ImplGlut_NewFrame() {
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)glutGet(GLUT_WINDOW_WIDTH), (float)glutGet(GLUT_WINDOW_HEIGHT));

	int nowMs = glutGet(GLUT_ELAPSED_TIME);
	double deltaSecs = (nowMs - g_LastFrameTimeMs) / 1000.0;
	g_LastFrameTimeMs = nowMs;
	io.DeltaTime = deltaSecs > 0.0 ? (float)deltaSecs : (1.0f / 60.0f);
}

void ImGui_ImplGlut_MouseButtonCallback(int button, int state, int x, int y) {
	ImGuiIO& io = ImGui::GetIO();
	UpdateKeyModifiers();
	io.AddMousePosEvent((float)x, (float)y);

	int imguiButton = -1;
	if (button == GLUT_LEFT_BUTTON) imguiButton = ImGuiMouseButton_Left;
	else if (button == GLUT_RIGHT_BUTTON) imguiButton = ImGuiMouseButton_Right;
	else if (button == GLUT_MIDDLE_BUTTON) imguiButton = ImGuiMouseButton_Middle;

	if (imguiButton >= 0) {
		io.AddMouseButtonEvent(imguiButton, state == GLUT_DOWN);
	}
}

void ImGui_ImplGlut_MouseWheelCallback(int /*wheel*/, int direction, int x, int y) {
	ImGuiIO& io = ImGui::GetIO();
	io.AddMousePosEvent((float)x, (float)y);
	io.AddMouseWheelEvent(0.0f, direction > 0 ? 1.0f : -1.0f);
}

void ImGui_ImplGlut_MotionCallback(int x, int y) {
	ImGui::GetIO().AddMousePosEvent((float)x, (float)y);
}

void ImGui_ImplGlut_KeyboardCallback(unsigned char key, int /*x*/, int /*y*/) {
	ImGuiIO& io = ImGui::GetIO();
	UpdateKeyModifiers();

	ImGuiKey mapped = AsciiToImGuiKey(key);
	if (mapped != ImGuiKey_None) {
		io.AddKeyEvent(mapped, true);
	} else if (key >= 32 && key < 127) {
		io.AddInputCharacter(key);
	}
}

void ImGui_ImplGlut_KeyboardUpCallback(unsigned char key, int /*x*/, int /*y*/) {
	ImGuiIO& io = ImGui::GetIO();
	UpdateKeyModifiers();

	ImGuiKey mapped = AsciiToImGuiKey(key);
	if (mapped != ImGuiKey_None) {
		io.AddKeyEvent(mapped, false);
	}
}

void ImGui_ImplGlut_SpecialCallback(int key, int /*x*/, int /*y*/) {
	ImGuiIO& io = ImGui::GetIO();
	UpdateKeyModifiers();

	ImGuiKey mapped = SpecialToImGuiKey(key);
	if (mapped != ImGuiKey_None) {
		io.AddKeyEvent(mapped, true);
	}
}

void ImGui_ImplGlut_SpecialUpCallback(int key, int /*x*/, int /*y*/) {
	ImGuiIO& io = ImGui::GetIO();
	UpdateKeyModifiers();

	ImGuiKey mapped = SpecialToImGuiKey(key);
	if (mapped != ImGuiKey_None) {
		io.AddKeyEvent(mapped, false);
	}
}

void ImGui_ImplGlut_ReshapeCallback(int width, int height) {
	ImGui::GetIO().DisplaySize = ImVec2((float)width, (float)height);
}
