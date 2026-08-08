/**
 * @file ConsoleMask.cpp
 * @brief Platform-specific masked console input implementation.
 */

#include "security/ConsoleMask.hpp"
#include <iostream>

#if defined(_WIN32)
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

namespace securebank::security {

#if defined(_WIN32)

std::string ConsoleMask::readMaskedInput() {
    std::string input;
    char ch = '\0';

    while ((ch = static_cast<char>(_getch())) != '\r') {
        if (ch == '\b') { // backspace
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";
            }
            continue;
        }
        input += ch;
        std::cout << '*';
    }
    std::cout << '\n';
    return input;
}

#else // POSIX fallback (Linux/Mac)

std::string ConsoleMask::readMaskedInput() {
    std::string input;
    termios oldSettings{};
    termios newSettings{};

    tcgetattr(STDIN_FILENO, &oldSettings);
    newSettings = oldSettings;
    newSettings.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);

    char ch = '\0';
    while (read(STDIN_FILENO, &ch, 1) == 1 && ch != '\n') {
        if (ch == 127 || ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";
            }
            continue;
        }
        input += ch;
        std::cout << '*';
    }
    std::cout << '\n';

    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    return input;
}

#endif

} // namespace securebank::security