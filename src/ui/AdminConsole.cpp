/**
 * @file AdminConsole.cpp
 * @brief Implementation of the admin menu-driven console interface.
 */

#include "ui/AdminConsole.hpp"
#include "validation/FieldValidator.hpp"
#include "security/PinHasher.hpp"
#include <iostream>
#include <iomanip>

namespace securebank::ui {

using validation::FieldValidator;
using domain::AccountType;
using domain::AccountStatus;

AdminConsole::AdminConsole(core::BankSystem& bankSystem) noexcept : bankSystem_(bankSystem) {}

void AdminConsole::showMenu() const {
    std::cout << "\n=========================================\n"
              << "        SECUREBANK - ADMIN PORTAL\n"
              << "=========================================\n"
              << " 1. Create Account\n"
              << " 2. View All Accounts\n"
              << " 3. Search Account\n"
              << " 4. Delete Account\n"
              << " 5. Unlock Account\n"
              << " 0. Exit\n"
              << "=========================================\n"
              << "Select an option: ";
}

void AdminConsole::run() {
    while (true) {
        showMenu();
        int choice{};
        if (!(std::cin >> choice)) {
            FieldValidator::clearInputBuffer();
            std::cout << "Invalid option. Try again.\n";
            continue;
        }
        FieldValidator::clearInputBuffer();

        switch (choice) {
            case 1: handleCreateAccount(); break;
            case 2: handleViewAllAccounts(); break;
            case 3: handleSearchAccount(); break;
            case 4: handleDeleteAccount(); break;
            case 5: handleUnlockAccount(); break;
            case 0: std::cout << "Exiting Admin Portal.\n"; return;
            default: std::cout << "Invalid option. Try again.\n";
        }
    }
}

void AdminConsole::handleCreateAccount() {
    auto name = FieldValidator::readNonEmptyName("Customer Name: ");
    if (!name) { std::cout << name.error().message << '\n'; return; }

    auto cnic = FieldValidator::readCnic("CNIC (13 digits): ");
    if (!cnic) { std::cout << cnic.error().message << '\n'; return; }

    std::cout << "Phone: ";
    std::string phone;
    std::getline(std::cin, phone);

    std::cout << "Account Type (1 = Savings, 2 = Current): ";
    int typeChoice{};
    std::cin >> typeChoice;
    FieldValidator::clearInputBuffer();
    const AccountType type = (typeChoice == 2) ? AccountType::Current : AccountType::Savings;

    auto pin = FieldValidator::readNumericPin("Set 4-digit PIN: ");
    if (!pin) { std::cout << pin.error().message << '\n'; return; }

    auto balance = FieldValidator::readPositiveAmount("Opening Balance (Rs.): ");
    if (!balance) { std::cout << balance.error().message << '\n'; return; }

    const std::string hashedPin = security::PinHasher::hash(pin.value());

    auto result = bankSystem_.createAccount(name.value(), cnic.value(), phone, type,
                                             hashedPin, balance.value());
    if (!result) {
        std::cout << "Error: " << result.error().message << '\n';
        return;
    }
    std::cout << "Account created successfully. Account Number: "
              << result.value().accountNumber() << '\n';
}

void AdminConsole::handleViewAllAccounts() const {
    const auto& accounts = bankSystem_.allAccounts();
    if (accounts.empty()) {
        std::cout << "No accounts on record.\n";
        return;
    }
    std::cout << std::left
              << std::setw(10) << "AccNo"
              << std::setw(22) << "Name"
              << std::setw(10) << "Type"
              << std::setw(10) << "Status"
              << "Balance\n";
    std::cout << std::string(60, '-') << '\n';
    for (const auto& acc : accounts) {
        std::cout << std::left
                  << std::setw(10) << acc.accountNumber()
                  << std::setw(22) << acc.customerName()
                  << std::setw(10) << (acc.type() == AccountType::Savings ? "Savings" : "Current")
                  << std::setw(10) << (acc.status() == AccountStatus::Active ? "Active" :
                                        acc.status() == AccountStatus::Locked ? "Locked" : "Closed")
                  << acc.balance() << '\n';
    }
}

void AdminConsole::handleSearchAccount() const {
    auto accNo = FieldValidator::readAccountNumber("Enter Account Number to search: ");
    if (!accNo) { std::cout << accNo.error().message << '\n'; return; }

    auto account = bankSystem_.findAccountByNumber(accNo.value());
    if (!account) {
        std::cout << "Error: " << account.error().message << '\n';
        return;
    }
    const auto& acc = account.value();
    std::cout << "Name: " << acc.customerName() << '\n'
              << "CNIC: " << acc.cnic() << '\n'
              << "Balance: " << acc.balance() << '\n'
              << "Status: " << (acc.status() == AccountStatus::Active ? "Active" :
                                 acc.status() == AccountStatus::Locked ? "Locked" : "Closed") << '\n';
}

void AdminConsole::handleDeleteAccount() {
    auto accNo = FieldValidator::readAccountNumber("Enter Account Number to delete: ");
    if (!accNo) { std::cout << accNo.error().message << '\n'; return; }

    auto result = bankSystem_.deleteAccount(accNo.value());
    if (!result) {
        std::cout << "Error: " << result.error().message << '\n';
        return;
    }
    std::cout << "Account deleted successfully.\n";
}

void AdminConsole::handleUnlockAccount() {
    auto accNo = FieldValidator::readAccountNumber("Enter Account Number to unlock: ");
    if (!accNo) { std::cout << accNo.error().message << '\n'; return; }

    auto result = bankSystem_.unlockAccount(accNo.value());
    if (!result) {
        std::cout << "Error: " << result.error().message << '\n';
        return;
    }
    std::cout << "Account unlocked successfully.\n";
}

} // namespace securebank::ui