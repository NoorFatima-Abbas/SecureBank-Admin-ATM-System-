/**
 * @file main.cpp
 * @brief Application entry point.
 *
 * Compile with -DGUI_MODE -mwindows for the windowed ATM GUI.
 * Compile without those flags for the original console portal menu.
 */

#include "core/BankSystem.hpp"
#include "persistence/AccountLedgerStore.hpp"
#include "persistence/TransactionJournal.hpp"
#include <iostream>
#include <memory>

#if defined(_WIN32)
    #include <windows.h>
#endif

#if defined(GUI_MODE) && defined(_WIN32)
    #include "ui/GuiAtm.hpp"
#else
    #include "ui/AdminConsole.hpp"
    #include "ui/AtmConsole.hpp"
    #include <limits>
    #include <string>
#endif

int main() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    using namespace securebank;

    auto accountStore = std::make_unique<persistence::AccountLedgerStore>("data/accounts.txt");
    auto journal      = std::make_unique<persistence::TransactionJournal>("data/transactions.txt");

    core::BankSystem bankSystem(std::move(accountStore), std::move(journal));

    if (auto init = bankSystem.initialize(); !init) {
        std::cerr << "Fatal: failed to initialise bank system — "
                  << init.error().message << '\n';
        return 1;
    }

#if defined(GUI_MODE) && defined(_WIN32)
    // ── Windowed GUI ATM ──────────────────────────────────────────────
    return ui::runGui(bankSystem);

#else
    // ── Console portal menu ──────────────────────────────────────────
    bool running = true;
    while (running) {
        std::cout << "\n"
                  << "  ==================================================\n"
                  << "          WELCOME TO SECUREBANK\n"
                  << "  ==================================================\n"
                  << "  [1] Admin Portal\n"
                  << "  [2] Customer ATM\n"
                  << "  [0] Exit\n"
                  << "  ==================================================\n"
                  << "  Select: ";

        int choice = 0;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
            case 1: {
                ui::AdminConsole admin(bankSystem);
                admin.run();
                break;
            }
            case 2: {
                ui::AtmConsole atm(bankSystem, "data/transactions.txt");
                atm.run();
                break;
            }
            case 0:
                std::cout << "\n  Thank you for using SecureBank. Goodbye!\n\n";
                running = false;
                break;
            default:
                std::cout << "  Invalid choice. Please enter 0, 1, or 2.\n";
                break;
        }
    }
    return 0;
#endif
}
