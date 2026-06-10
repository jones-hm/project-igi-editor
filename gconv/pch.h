#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#include <windows.h>
#endif

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cmath>
#include <assert.h>
#include <memory>
#include <optional>
#include <filesystem>

// Project common types (no GL/app dependencies)
#include "common.h"
#include "level/level_common.h"
