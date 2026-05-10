# IGI Real-Time Link: Memory Reader Design

## Overview
This feature establishes a real-time data link between the Project-IGI-Terrain viewer and the original `igi.exe` game. It uses the `gtlibcpp` library to read player coordinates from memory in a background thread and displays them on a HUD overlay.

## Architecture & Components

### 1. `IGIBridge` Class (New Module)
- **Thread Management**: Encapsulates a `std::thread` that runs a continuous loop.
- **Process Attachment**: Periodically attempts to find the `igi.exe` process using `GTLibc`.
- **Data Capture**:
  - Uses the pointer chain from the provided Cheat Table: `[[[IGI.exe + 0x16E210] + 0x8] + 0x7CC] + 0x14]` for the base human address.
  - Reads `float` values for X (Offset `0x24`), Y (Offset `0x2C`), and Z (Offset `0x34`).
- **Thread Safety**: Uses a `std::mutex` to protect the shared `PositionData` struct accessed by both the background thread and the UI render thread.
- **Error Handling**: Implements `try-catch` blocks around all memory read operations to prevent crashes if the game closes or addresses become invalid.

### 2. HUD Overlay (`Renderer` / `App`)
- **Render Location**: Top-left corner of the screen.
- **Display Content**:
  - Link Status (Connected/Disconnected).
  - Raw Units: `X, Y, Z`.
  - Meters: `X, Y, Z` (converted via `/ 4096.0`).
  - Ground Offset: Difference between Player Z and Terrain Z at the current X,Y.
  - Static Label: `Checks: 0`.
- **Text Rendering**: Uses `glutBitmapString` for efficient, simple text display over the 3D viewport.

### 3. State Management (`App` class)
- Stores an instance of `IGIBridge`.
- Provides a toggle `show_hud_` (Hotkey: 'L').
- In the `Frame()` loop, it retrieves the latest `PositionData` from the bridge and passes it to the renderer.

## Data Flow
1. **Background**: `IGIBridge` thread finds `igi.exe`.
2. **Background**: Reads memory -> Converts to meters -> Stores in shared struct (mutex-locked).
3. **Frontend**: `App::Frame()` queries `IGIBridge` for latest data.
4. **Frontend**: `App` queries `Terrain::GetZ()` for ground height at player X/Y.
5. **Frontend**: `Renderer` draws the HUD text using OpenGL.

## Error Handling & Edge Cases
- **Game Not Running**: HUD displays `SEARCHING FOR IGI.EXE...`. Memory reads are skipped.
- **Pointer Invalidation**: If the pointer chain breaks (e.g., during level load), the bridge catches the exception and resets the attachment state.
- **Precision**: Uses `float` as stored in the game memory, converted to `double` for meter calculations to ensure accuracy.

## Testing Strategy
- Run `igi.exe` alongside the viewer and verify "CONNECTED" status.
- Move the player in-game and verify the HUD coordinates update instantly.
- Verify that closing the game does not crash the terrain viewer.
- Verify that 'L' toggles the HUD visibility.
