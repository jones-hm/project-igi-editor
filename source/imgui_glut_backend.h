/******************************************************************************
 * @file    imgui_glut_backend.h
 * @brief   Custom Dear ImGui platform glue for freeglut (no official ImGui
 *          GLUT backend exists). Pairs with the official imgui_impl_opengl3
 *          render backend. Feeds ImGuiIO from the app's existing GLUT
 *          callbacks; does not replace or alter any existing input handling.
 *****************************************************************************/
#pragma once

void ImGui_ImplGlut_Init();
void ImGui_ImplGlut_Shutdown();
void ImGui_ImplGlut_NewFrame();

// Call these from the app's existing GLUT callbacks, in addition to (not
// instead of) the app's own Input_On... handlers.
void ImGui_ImplGlut_MouseButtonCallback(int button, int state, int x, int y);
void ImGui_ImplGlut_MouseWheelCallback(int wheel, int direction, int x, int y);
void ImGui_ImplGlut_MotionCallback(int x, int y);
void ImGui_ImplGlut_KeyboardCallback(unsigned char key, int x, int y);
void ImGui_ImplGlut_KeyboardUpCallback(unsigned char key, int x, int y);
void ImGui_ImplGlut_SpecialCallback(int key, int x, int y);
void ImGui_ImplGlut_SpecialUpCallback(int key, int x, int y);
void ImGui_ImplGlut_ReshapeCallback(int width, int height);
