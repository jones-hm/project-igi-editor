---
phase: igi-link
plan: 2026-05-10-igi-link-plan
subsystem: IGI-Link
tags: [integration, memory-reading, hud, ui]
requirements: [IGI-LINK-01, IGI-LINK-02, IGI-LINK-03]
dependency-graph:
  requires: [gtlibcpp]
  provides: [real-time-igi-sync]
  affects: [App, Renderer]
tech-stack: [C++, OpenGL, GLUT, GTLibCpp]
key-files: [source/igi_bridge.h, source/igi_bridge.cpp, source/app.cpp, source/renderer/renderer.cpp]
decisions:
  - Use std::mutex for IGIBridge thread-safety.
  - Use 2D GLUT overlay for HUD in Renderer.
metrics:
  duration: 45m
  completed-date: 2026-05-10
---

# Phase IGI-Link Plan: IGI Real-Time Link Summary

Implemented real-time memory reading from `igi.exe` to sync player coordinates and altitude with the terrain viewer, displaying the data on a green/yellow HUD overlay.

## Key Accomplishments

### 1. GTLibCpp Integration
Integrated `gtlibcpp` library into the build system (`CMakeLists.txt`) to enable memory reading capabilities on Windows.

### 2. IGIBridge Threaded Module
Created a thread-safe `IGIBridge` module that runs in the background, searching for the `igi.exe` process and reading the player position via a pointer chain (`[[[IGI.exe + 0x16E210] + 0x8] + 0x7CC] + 0x14] + {0x24, 0x2C, 0x34}`).

### 3. HUD Overlay System
Extended the `Renderer` to support a 2D HUD overlay using GLUT bitmap strings. The HUD displays connection status, raw coordinates, converted meters, and ground offset.

### 4. App Integration
- Lifecycle management: Bridge starts on app init and stops on shutdown.
- Data Flow: `App::Frame` fetches latest data from the bridge, calculates terrain height at that position, and passes it to the renderer.
- Input: Added 'L' key toggle to show/hide the HUD.

## Deviations from Plan

None - plan executed exactly as written.

## Self-Check: PASSED

- [x] `source/igi_bridge.h` and `source/igi_bridge.cpp` created.
- [x] `Renderer` Draw method updated with HUD params.
- [x] `App` starts/stops the bridge.
- [x] Build successful: `bin/Release/terrain.exe` generated.

## Commits

- `e0ea4ae`: feat(igi-link): integrate IGIBridge with App and input handling
- (Previous tasks committed by prior agents)
