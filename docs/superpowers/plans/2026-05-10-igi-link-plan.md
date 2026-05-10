# IGI Real-Time Link Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate real-time memory reading from `igi.exe` using `gtlibcpp` and display player coordinates/altitude on a HUD overlay in the Project-IGI-Terrain viewer.

**Architecture:** 
1. `IGIBridge`: A dedicated module using `gtlibcpp` to interface with `igi.exe` in a background thread.
2. `App`: Manages the `IGIBridge` lifecycle and coordinates data transfer between the bridge and the HUD.
3. `HUD`: A UI layer in `Renderer` using `glutBitmapString` to display the connected status and coordinates.

**Tech Stack:** C++, OpenGL, GLUT, GLM, GTLibCpp

---

### Task 1: Build System and GTLibCpp Integration

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `source/pch.h`

- [ ] **Step 1: Update `CMakeLists.txt` to include `gtlibcpp`**

Add the `third_party/gtlibcpp` directory to include paths and add the source files to the project. Note that `gtlibcpp` relies on some Windows-specific headers.

```cmake
# Add gtlibcpp to includes
include_directories(${PROJECT_SOURCE_DIR}/third_party/gtlibcpp)

# Add source files
set(GTLIBCPP_SOURCES
    ${PROJECT_SOURCE_DIR}/third_party/gtlibcpp/GTLibc.cpp
)

# Update target_sources
target_sources(terrain PRIVATE 
    ${GTLIBCPP_SOURCES}
    # ... other sources
)
```

- [ ] **Step 2: Add `GTLibc.hpp` to `pch.h`**

```cpp
#ifdef _WIN32
#include "GTLibc.hpp"
#endif
```

- [ ] **Step 3: Build and Verify**

Run: `cd vcbuild; cmake ..; cmake --build . --config Release`
Expected: Successful compilation of `GTLibc.cpp`.

---

### Task 2: Create IGIBridge Module

**Files:**
- Create: `source/igi_bridge.h`
- Create: `source/igi_bridge.cpp`

- [ ] **Step 1: Define `IGIBridge` class in `igi_bridge.h`**

```cpp
#pragma once
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "GTLibc.hpp"

class IGIBridge {
public:
    struct PositionData {
        glm::vec3 raw_pos;
        glm::vec3 meters_pos;
        bool connected = false;
        std::string status_msg = "SEARCHING FOR IGI.EXE...";
    };

    IGIBridge();
    ~IGIBridge();

    void Start();
    void Stop();
    PositionData GetLatestData();

private:
    void ThreadLoop();
    void UpdateData();

    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::mutex data_mutex_;
    PositionData current_data_;

    GTLIBC::GTLibc gt_;
    DWORD game_base_addr_ = 0;
};
```

- [ ] **Step 2: Implement `IGIBridge` in `igi_bridge.cpp`**

Implement the logic to attach to `igi.exe` and read the pointer chain: `[[[IGI.exe + 0x16E210] + 0x8] + 0x7CC] + 0x14] + {0x24, 0x2C, 0x34}`.

```cpp
#include "pch.h"
#include "igi_bridge.h"

IGIBridge::IGIBridge() : running_(false) {}
IGIBridge::~IGIBridge() { Stop(); }

void IGIBridge::Start() {
    running_ = true;
    worker_thread_ = std::thread(&IGIBridge::ThreadLoop, this);
}

void IGIBridge::Stop() {
    running_ = false;
    if (worker_thread_.joinable()) worker_thread_.join();
}

void IGIBridge::ThreadLoop() {
    while (running_) {
        UpdateData();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

void IGIBridge::UpdateData() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    try {
        if (!gt_.GetGameHandle()) {
            if (gt_.FindGameProcess("igi")) {
                game_base_addr_ = gt_.GetGameBaseAddress();
            } else {
                current_data_.connected = false;
                current_data_.status_msg = "SEARCHING FOR IGI.EXE...";
                return;
            }
        }

        // Pointer chain from CT
        DWORD human_static = 0x0016E210;
        std::vector<DWORD> offsets = {0x8, 0x7CC, 0x14};
        
        DWORD base_ptr = gt_.ReadPointerOffset<DWORD>(game_base_addr_, human_static);
        DWORD human_addr = gt_.ReadPointerOffsets<DWORD>(base_ptr, offsets);

        if (human_addr) {
            current_data_.raw_pos.x = gt_.ReadAddress<float>(human_addr + 0x24);
            current_data_.raw_pos.y = gt_.ReadAddress<float>(human_addr + 0x2C);
            current_data_.raw_pos.z = gt_.ReadAddress<float>(human_addr + 0x34);
            
            current_data_.meters_pos = current_data_.raw_pos / 4096.0f;
            current_data_.connected = true;
            current_data_.status_msg = "IGI LINK: CONNECTED";
        } else {
            current_data_.connected = false;
            current_data_.status_msg = "LINK ERROR: INVALID POINTER";
        }
    } catch (...) {
        current_data_.connected = false;
        current_data_.status_msg = "LINK ERROR: EXCEPTION";
    }
}

IGIBridge::PositionData IGIBridge::GetLatestData() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_data_;
}
```

- [ ] **Step 3: Commit**

```bash
git add source/igi_bridge.h source/igi_bridge.cpp
git commit -m "feat: Add IGIBridge module for memory reading"
```

---

### Task 3: HUD UI in Renderer

**Files:**
- Modify: `source/renderer/renderer.h`
- Modify: `source/renderer/renderer.cpp`

- [ ] **Step 1: Add HUD data struct to `renderer.h`**

```cpp
	struct hud_params_s {
		bool show_hud_;
		std::string status_msg_;
		glm::vec3 raw_pos_;
		glm::vec3 meters_pos_;
		float ground_offset_;
	};
```

Update `Draw` signature:
```cpp
	void					Draw(const draw_params_s& params, const hud_params_s& hud);
```

- [ ] **Step 2: Implement HUD drawing in `renderer.cpp`**

```cpp
void Renderer::Draw(const draw_params_s& params, const hud_params_s& hud) {
    // ... existing terrain draw code ...

    if (hud.show_hud_) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, params.view_define_->viewport_width_, 0, params.view_define_->viewport_height_);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor3f(1.0f, 1.0f, 0.0f); // Yellow
        if (hud.status_msg_.find("CONNECTED") != std::string::npos) glColor3f(0.0f, 1.0f, 0.0f); // Green

        auto draw_text = [&](int x, int y, const char* str) {
            glRasterPos2i(x, params.view_define_->viewport_height_ - y);
            for (const char* c = str; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
            }
        };

        int line_y = 20;
        draw_text(10, line_y, hud.status_msg_.c_str()); line_y += 15;
        
        if (hud.status_msg_.find("CONNECTED") != std::string::npos) {
            char buf[128];
            sprintf(buf, "RAW: %.0f, %.0f, %.0f", hud.raw_pos_.x, hud.raw_pos_.y, hud.raw_pos_.z);
            draw_text(10, line_y, buf); line_y += 15;
            sprintf(buf, "MTR: %.2fm, %.2fm, %.2fm", hud.meters_pos_.x, hud.meters_pos_.y, hud.meters_pos_.z);
            draw_text(10, line_y, buf); line_y += 15;
            sprintf(buf, "GND OFFSET: %.2fm", hud.ground_offset_ / 4096.0f);
            draw_text(10, line_y, buf); line_y += 15;
        }
        draw_text(10, line_y, "Checks: 0");

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}
```

---

### Task 4: Integrate with App and Input

**Files:**
- Modify: `source/app.h`
- Modify: `source/app.cpp`

- [ ] **Step 1: Add `IGIBridge` to `App`**

Include `igi_bridge.h` and add `bridge_` instance. Add `show_hud_` flag. Initialize in constructor, start in `Init`, stop in `Shutdown`.

- [ ] **Step 2: Update `App::Frame` to pass HUD data**

```cpp
	float ground_z = 0.0f;
	IGIBridge::PositionData data = bridge_.GetLatestData();
	level_.GetTerrainZ(data.raw_pos, ground_z);

	Renderer::hud_params_s hud = {
		.show_hud_ = show_hud_,
		.status_msg_ = data.status_msg,
		.raw_pos_ = data.raw_pos,
		.meters_pos_ = data.meters_pos,
		.ground_offset_ = data.raw_pos.z - ground_z
	};

	renderer_.Draw(draw_params_, hud);
```

- [ ] **Step 3: Handle 'L' key in `Input_OnKeyboard`**

```cpp
	if (key == 'l' || key == 'L') {
		show_hud_ = !show_hud_;
		return;
	}
```

- [ ] **Step 4: Final Build and Test**

Build and run. Verify the HUD appears and updates coordinates correctly when `igi.exe` is running.
