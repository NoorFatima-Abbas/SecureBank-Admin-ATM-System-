/**
 * @file GuiAtm.hpp
 * @brief Custom Win32 ATM GUI — hand-drawn, no external libraries.
 * @details Declares the single public entry point that launches the
 * windowed ATM interface. Everything else lives in GuiAtm.cpp.
 *
 * Owned by: Ayesha Kamran
 */

#pragma once
#include "core/BankSystem.hpp"

namespace securebank::ui {

/// Initialises the Win32 window, runs the message loop, and returns
/// the exit code when the user closes the ATM window.
/// Only call this on Windows; guarded by #ifdef _WIN32 in main.cpp.
int runGui(core::BankSystem& bankSystem);

} // namespace securebank::ui
