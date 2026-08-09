/**
 * @file ConsoleMask.hpp
 * @brief Bonus Feature: Hidden PIN Entry.
 * @details Reads console input character-by-character, echoing '*' instead
 * of the actual character, with backspace support. Windows implementation
 * uses conio.h; a POSIX fallback is provided for portability.
 */

#pragma once
#include <string>

namespace securebank::security {

class ConsoleMask {
public:
    /// Reads a masked line of input (e.g. a PIN) from stdin, echoing '*' per keystroke.
    [[nodiscard]] static std::string readMaskedInput();
};

} // namespace securebank::security