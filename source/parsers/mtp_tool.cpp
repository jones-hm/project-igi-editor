/******************************************************************************
 * @file    mtp_tool.cpp
 * @brief   Drive the external mtp_decoder.exe to (re)generate a level .mtp.
 *****************************************************************************/

#include "mtp_tool.h"
#include "../logger.h"
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>

namespace {

// Returns the .mtp's last-write time as a comparable count, or -1 if absent.
long long MtpMtime(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return -1;
    auto t = std::filesystem::last_write_time(path, ec);
    if (ec)
        return -1;
    return t.time_since_epoch().count();
}

// Build an INPUT_RECORD for a single key press (down then up appended by caller).
INPUT_RECORD KeyRecord(WORD vk, char ascii, bool down) {
    INPUT_RECORD r{};
    r.EventType = KEY_EVENT;
    r.Event.KeyEvent.bKeyDown = down ? TRUE : FALSE;
    r.Event.KeyEvent.wRepeatCount = 1;
    r.Event.KeyEvent.wVirtualKeyCode = vk;
    r.Event.KeyEvent.wVirtualScanCode = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    r.Event.KeyEvent.uChar.AsciiChar = ascii;
    r.Event.KeyEvent.dwControlKeyState = 0;
    return r;
}

// Best-effort injection of 'M' + Enter into the child's console input buffer.
// Any failure is swallowed; the user can press M manually.
void TryInjectChoice(DWORD childPid) {
    // Detach from our (likely none) console and attach to the child's.
    FreeConsole();
    if (!AttachConsole(childPid)) {
        // Restore parent console attachment and give up.
        FreeConsole();
        AttachConsole(ATTACH_PARENT_PROCESS);
        return;
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr) {
        INPUT_RECORD recs[4] = {
            KeyRecord('M', 'M', true),
            KeyRecord('M', 'M', false),
            KeyRecord(VK_RETURN, '\r', true),
            KeyRecord(VK_RETURN, '\r', false),
        };
        DWORD written = 0;
        WriteConsoleInputA(hIn, recs, 4, &written);
    }

    FreeConsole();
    AttachConsole(ATTACH_PARENT_PROCESS);
}

} // namespace

bool RunMtpDecoder(const std::string& exePath, const std::string& datPath,
                   const std::string& expectedMtpPath, std::string& err,
                   unsigned timeoutMs) {
    if (!std::filesystem::exists(exePath)) {
        err = "mtp_decoder.exe not found: " + exePath;
        Logger::Get().Log(LogLevel::WARNING, "[MTPTool] " + err);
        return false;
    }
    if (!std::filesystem::exists(datPath)) {
        err = ".dat not found: " + datPath;
        Logger::Get().Log(LogLevel::WARNING, "[MTPTool] " + err);
        return false;
    }

    const long long beforeMtime = MtpMtime(expectedMtpPath);

    // The tool reads the .dat filename relative to its CWD: pass just the filename
    // and set CWD to the .dat's directory.
    std::filesystem::path datP(datPath);
    const std::string cwd = datP.parent_path().string();
    const std::string datName = datP.filename().string();

    std::string cmd = "\"" + exePath + "\" \"" + datName + "\"";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(exePath.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE, nullptr, cwd.c_str(), &si, &pi)) {
        err = "CreateProcess failed (" + std::to_string(GetLastError()) + ") for " + exePath;
        Logger::Get().Log(LogLevel::WARNING, "[MTPTool] " + err);
        return false;
    }

    // Give the child console a moment to initialize before injecting the keystroke.
    for (int i = 0; i < 15; ++i) {
        if (WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0)
            break; // already exited (unlikely)
    }
    TryInjectChoice(pi.dwProcessId);

    // Poll for the .mtp mtime to advance (tool prints "File Saved.." after writing).
    bool ok = false;
    const DWORD deadline = GetTickCount() + timeoutMs;
    while (GetTickCount() < deadline) {
        long long now = MtpMtime(expectedMtpPath);
        if (now != -1 && (beforeMtime == -1 || now > beforeMtime)) {
            ok = true;
            break;
        }
        if (WaitForSingleObject(pi.hProcess, 250) == WAIT_OBJECT_0) {
            // Process exited; one final check.
            long long fin = MtpMtime(expectedMtpPath);
            if (fin != -1 && (beforeMtime == -1 || fin > beforeMtime))
                ok = true;
            break;
        }
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (ok) {
        Logger::Get().Log(LogLevel::INFO, "[MTPTool] mtp_decoder regenerated " + expectedMtpPath);
    } else {
        err = "mtp_decoder did not regenerate .mtp within timeout; press M in its window manually";
        Logger::Get().Log(LogLevel::WARNING, "[MTPTool] " + err);
    }
    return ok;
}

#else // !_WIN32

bool RunMtpDecoder(const std::string&, const std::string&,
                   const std::string&, std::string& err, unsigned) {
    err = "RunMtpDecoder is Windows-only";
    return false;
}

#endif
