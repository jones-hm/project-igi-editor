/******************************************************************************
 * @file    utils.cpp
 * @brief   Utility functions for the IGI Editor
 *****************************************************************************/

#include "pch.h"
#include "utils.h"
#include "logger.h"
#include <windows.h>
#include <sstream>
#include <algorithm>

namespace Utils {

static bool g_logEnabled = true;

// Logging functions
void LogInfo(const std::string& message) {
	if (g_logEnabled) {
		Logger::Get().Log(LogLevel::INFO, message);
	}
}

void LogError(const std::string& message) {
	if (g_logEnabled) {
		Logger::Get().Log(LogLevel::ERR, message);
	}
}

void LogWarning(const std::string& message) {
	if (g_logEnabled) {
		Logger::Get().Log(LogLevel::WARNING, message);
	}
}

void SetLogEnabled(bool enabled) {
	g_logEnabled = enabled;
}

// UI message box functions
void ShowError(const std::string& message, const std::string& title) {
	MessageBoxA(NULL, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

void ShowWarning(const std::string& message, const std::string& title) {
	MessageBoxA(NULL, message.c_str(), title.c_str(), MB_OK | MB_ICONWARNING);
}

void ShowInfo(const std::string& message, const std::string& title) {
	MessageBoxA(NULL, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

// Combined log + message box functions
void LogAndShowError(const std::string& message, const std::string& title) {
	LogError(message);
	ShowError(message, title);
}

// Process elevation check
bool IsElevatedProcess() {
	bool isElevated = false;
	HANDLE token = NULL;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
		TOKEN_ELEVATION elevation;
		DWORD token_sz = sizeof(TOKEN_ELEVATION);
		if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &token_sz)) {
			isElevated = elevation.TokenIsElevated;
		}
	}
	if (token) {
		CloseHandle(token);
	}
	return isElevated;
}

// String utilities
std::string GetLastErrorAsString() {
	DWORD error = GetLastError();
	if (error == 0)
		return "No error";

	std::ostringstream ss;
	ss << "Error code: " << error << " - " << std::system_category().message(error);
	return ss.str();
}

std::string Trim(const std::string& str) {
	auto start = str.find_first_not_of(" \t\n\r");
	if (start == std::string::npos) return "";
	auto end = str.find_last_not_of(" \t\n\r");
	return str.substr(start, end - start + 1);
}

std::vector<std::string> Split(const std::string& str, char delimiter) {
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;
	while (std::getline(ss, token, delimiter)) {
		tokens.push_back(token);
	}
	return tokens;
}

// Template implementations
template<typename T>
std::optional<T> TryParse(const std::string& str) {
	std::stringstream ss(str);
	T value;
	if (ss >> value) {
		if (ss.eof()) {
			return value;
		}
	}
	return {};
}

template<typename T>
std::string ToString(const T& value) {
	if constexpr (std::is_arithmetic_v<T>) {
		return std::to_string(value);
	}
	else if constexpr (std::is_same_v<T, std::string>) {
		return value;
	}
	else {
		throw std::invalid_argument("Unsupported data type for ToString.");
	}
}

// Explicit template instantiations
template std::optional<int> TryParse<int>(const std::string&);
template std::optional<float> TryParse<float>(const std::string&);
template std::optional<double> TryParse<double>(const std::string&);
template std::string ToString<int>(const int&);
template std::string ToString<float>(const float&);
template std::string ToString<double>(const double&);

// Key handling functions (Windows API)
bool HotKeysDown(const std::vector<int>& keys) {
	if (keys.empty()) return false;

	short result = GetAsyncKeyState(keys[0]);
	for (const auto& key : keys) {
		result &= GetAsyncKeyState(key);
	}
	return ((result & 0x8000) != 0);
}

bool IsKeyPressed(int keycode) {
	try {
		return (GetAsyncKeyState(keycode) & 0x8000);
	}
	catch (const std::exception& e) {
		LogError("IsKeyPressed: " + std::string(e.what()));
		return false;
	}
}

bool IsKeyToggled(int keycode) {
	try {
		return (GetAsyncKeyState(keycode) & 0x1);
	}
	catch (const std::exception& e) {
		LogError("IsKeyToggled: " + std::string(e.what()));
		return false;
	}
}

} // namespace Utils
