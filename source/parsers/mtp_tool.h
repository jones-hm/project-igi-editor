/******************************************************************************
 * @file    mtp_tool.h
 * @brief   Drive the external mtp_decoder.exe to (re)generate a level .mtp
 *          from its text .dat (the game accepts the tool's output).
 *****************************************************************************/

#pragma once
#include <string>

// Run content/tools/mtp_decoder.exe on datPath to (re)generate the sibling .mtp.
// The tool is an interactive Pascal crt console app; we spawn it in its own console
// and attempt to inject the 'M' (Packed MTP) keystroke via the Windows console input
// buffer. Returns true if the expected .mtp was regenerated (mtime advanced) within
// `timeoutMs`. On failure leaves the console open and returns false (caller tells the
// user to press M manually). Windows-only; no-op returning false elsewhere.
bool RunMtpDecoder(const std::string& exePath, const std::string& datPath,
                   const std::string& expectedMtpPath, std::string& err,
                   unsigned timeoutMs = 15000);
