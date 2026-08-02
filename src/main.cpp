/**
 * @file main.cpp
 * @brief Application entry point — wires up BankSystem and launches the Admin menu.
 * @details The ATM entry point (AtmConsole) will be added here by Ayesha
 * once her module is ready, as a second menu option.
 */

#include "core/BankSystem.hpp"
#include "persistence/AccountLedgerStore.hpp"
#include "persistence/TransactionJournal.hpp"
#include "ui/AdminConsole.hpp"
#include <iostream>
#include <memory>
#include <windows.h>
int main() {
    SetConsoleOutputCP(CP_UTF8);
    using namespace securebank;

    auto accountStore = std::make_unique<persistence::AccountLedgerStore>("data/accounts.txt");
    auto journal = std::make_unique<persistence::TransactionJournal>("data/transactions.txt");

    core::BankSystem bankSystem(std::move(accountStore), std::move(journal));

    if (auto init = bankSystem.initialize(); !init) {
        std::cerr << "Fatal: failed to initialize bank system — " << init.error().message << '\n';
        return 1;
    }

    ui::AdminConsole adminConsole(bankSystem);
    adminConsole.run();

    return 0;
}