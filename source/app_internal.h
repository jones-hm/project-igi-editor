/******************************************************************************
 * @file    app_internal.h
 * @brief   Private shared declarations for the App implementation modules
 *          (app.cpp core + app_input.cpp). Common includes, tuning constants,
 *          movement-key table, and small math/text helpers used by both.
 *****************************************************************************/
#pragma once

/******************************************************************************
 * @file    app.cpp
 * @brief   application class
 *****************************************************************************/

#include "pch.h"
#include <cstdlib>
#include <stdexcept>
#include <freeglut.h>
#include "logger.h"
#include "utils.h"
#include "parsers/qsc_lexer.h"
#include "parsers/qsc_parser.h"
#include "parsers/qvm_compiler.h"
#include "parsers/qvm_parser.h"
#include "parsers/qvm_decompiler.h"
#include "cli/asset_extractor.h"
#include "parsers/dat_parser.h"
#include "parsers/tex_parser.h"
#include "parsers/res_parser.h"
#include "renderer/gl_helper.h"
#include "level/task_schema.h"
using namespace TaskSchemaNS;
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <atomic>


// ── game-process monitor + global-hotkey id (shared: core + app_editor) ──
struct GameMonitorParam {
	HANDLE             hProcess;
	std::atomic<bool>* pExited;
};

inline DWORD WINAPI GameMonitorProc(LPVOID param) {
	auto* p = static_cast<GameMonitorParam*>(param);
	HANDLE h      = p->hProcess;
	auto*  pExited = p->pExited;
	delete p;
	WaitForSingleObject(h, INFINITE);
	pExited->store(true, std::memory_order_release);
	return 0;
}

constexpr int HOTKEY_ID_TOGGLE_GAME = 0x47; // arbitrary non-conflicting ID


// ── tuning constants ──
constexpr float	MOUSE_SENSITIVE = 0.2f;

// movement
constexpr float		VIEW_HEIGHT = 7000.0f;
constexpr float		GRAVITE = 10.0f * WORLD_UNITS_PER_METER;
constexpr float		MIN_MOVE_SPEED = 8.0f * WORLD_UNITS_PER_METER;
constexpr float		MAX_MOVE_SPEED = 8192.0f * WORLD_UNITS_PER_METER;
constexpr float		MIN_JUMP_SPEED = 4.0f * WORLD_UNITS_PER_METER;
constexpr float		MAX_JUMP_SPEED = 512.0f * WORLD_UNITS_PER_METER;

// movement key down flags
constexpr int MK_FORWARD		= FLAG_BIT(0);
constexpr int MK_BACKWARD		= FLAG_BIT(1);
constexpr int MK_LEFT			= FLAG_BIT(2);
constexpr int MK_RIGHT			= FLAG_BIT(3);
constexpr int MK_STRAIGHT_UP	= FLAG_BIT(4);
constexpr int MK_STRAIGHT_DOWN	= FLAG_BIT(5);
constexpr int MK_JUMP			= FLAG_BIT(6);
constexpr int MK_ROLL_INC		= FLAG_BIT(7);
constexpr int MK_ROLL_DEC		= FLAG_BIT(8);

// IGI 2 Style Manipulation Flags
constexpr int MK_MANIP_A		= FLAG_BIT(10);
constexpr int MK_MANIP_B		= FLAG_BIT(11);
constexpr int MK_MANIP_G		= FLAG_BIT(12);
constexpr int MK_MANIP_S		= FLAG_BIT(13);
constexpr int MK_MANIP_O		= FLAG_BIT(14);
constexpr int MK_MANIP_SPACE	= FLAG_BIT(15);

// ── AI-script text helpers ──
inline std::vector<int> AiTextLineStarts(const std::string& txt, int max_chars) {
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

inline int AiScriptMaxChars() {
	return std::max(1, (PropPanel::kWidth - 2 * PropPanel::kPad - 6) / 7);
}

// ── movement key table ──
struct movement_key_s {
	char	lower_case_;
	char	upper_case_;
	int		key_flag_;
};

inline constexpr movement_key_s MOVEMENT_KEYS[] = {
	'q', 'Q', MK_STRAIGHT_UP,
	'z', 'Z', MK_STRAIGHT_DOWN
};

// ── ZXY euler helpers ──
inline glm::dmat3 BuildRotMatZXY(const glm::dvec3& euler) {
	glm::dmat4 m(1.0);
	m = glm::rotate(m, euler.z, glm::dvec3(0, 0, 1));
	m = glm::rotate(m, euler.x, glm::dvec3(1, 0, 0));
	m = glm::rotate(m, euler.y, glm::dvec3(0, 1, 0));
	return glm::dmat3(m);
}

// R[1][2]=sin(x); R[0][2]=-cx*sy, R[2][2]=cx*cy; R[1][0]=-sz*cx, R[1][1]=cz*cx
inline glm::dvec3 ExtractEulerZXY(const glm::dmat3& R) {
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
