/******************************************************************************
 * @file    utils.h
 * @brief   Utility functions for the IGI Editor
 *****************************************************************************/

#pragma once

#include <string>
#include <optional>
#include <vector>

namespace Utils {

// Logging functions
void LogInfo(const std::string& message);
void LogError(const std::string& message);
void LogWarning(const std::string& message);
void SetLogEnabled(bool enabled);

// UI message box functions
void ShowError(const std::string& message, const std::string& title = "Error");
void ShowWarning(const std::string& message, const std::string& title = "Warning");
void ShowInfo(const std::string& message, const std::string& title = "Information");

// Combined log + message box functions
void LogAndShowError(const std::string& message, const std::string& title = "Error");

// Process elevation check
bool IsElevatedProcess();

// String utilities
std::string GetLastErrorAsString();
std::string Trim(const std::string& str);
std::vector<std::string> Split(const std::string& str, char delimiter);

// Data type conversion utilities
template<typename T>
std::optional<T> TryParse(const std::string& str);

template<typename T>
std::string ToString(const T& value);

// Key handling functions (Windows API)
bool HotKeysDown(const std::vector<int>& keys);
bool IsKeyPressed(int keycode);
bool IsKeyToggled(int keycode);

} // namespace Utils
