/**
 * @file AtmConsole.cpp
 * @brief Implementation of the customer-facing ATM console UI.
 *
 * Owned by: Ayesha Kamran
 */

#include "ui/AtmConsole.hpp"
#include "ui/ReceiptWriter.hpp"
#include "domain/Enums.hpp"
#include "security/ConsoleMask.hpp"
#include "security/PinHasher.hpp"
#include "validation/FieldValidator.hpp"
#include "persistence/TransactionJournal.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
    #include <unistd.h>
#endif

namespace securebank::ui {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AtmConsole::AtmConsole(core::BankSystem& bankSystem,
                       std::filesystem::path journalPath) noexcept
    : bankSystem_(bankSystem),
      journalPath_(std::move(journalPath)) {}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

void AtmConsole::run() {
    printSeparator();
    std::cout << "     SECUREBANK — CUSTOMER ATM\n";
    printSeparator();

    long accountNumber = 0;

    if (!handleLogin(accountNumber)) {
        std::cout << "\nReturning to main menu.\n";
        return;
    }

    bool sessionActive = true;
    while (sessionActive) {
        showMenu();

        int choice = 0;
        if (!(std::cin >> choice)) {
            // stdin exhausted (piped input ended or stream error) — exit cleanly
            std::cout << "\n  Session ended.\n";
            break;
        }
        validation::FieldValidator::clearInputBuffer();

        switch (choice) {
            case 1: handleBalanceInquiry(accountNumber);  break;
            case 2: handleWithdrawal(accountNumber);      break;
            case 3: handleDeposit(accountNumber);         break;
            case 4: handleTransfer(accountNumber);        break;
            case 5: handleMiniStatement(accountNumber);   break;
            case 6:
                std::cout << "\nYou have been logged out. Thank you for using SecureBank.\n";
                sessionActive = false;
                break;
            default:
                std::cout << "Invalid choice. Please select 1–6.\n";
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Login
// ---------------------------------------------------------------------------

bool AtmConsole::handleLogin(long& outAccountNumber) const {
    constexpr int MaxLoginAttempts = 3;

    for (int attempt = 1; attempt <= MaxLoginAttempts; ++attempt) {
        // If stdin is exhausted, don't spin — exit cleanly
        if (std::cin.eof()) return false;

        printSeparator();
        std::cout << "  LOGIN  (Attempt " << attempt << " of " << MaxLoginAttempts << ")\n";
        printSeparator();

        // Read account number
        const auto accountResult =
            validation::FieldValidator::readAccountNumber("  Enter Account Number: ");
        if (!accountResult) {
            std::cout << "  " << accountResult.error().message << "\n";
            if (std::cin.eof()) return false;
            continue;
        }
        const long accountNo = accountResult.value();

        // Read PIN — use masked input on an interactive terminal, plain
        // std::getline on a piped/redirected stream.  ConsoleMask reads
        // directly from STDIN_FILENO; mixing that with std::cin's internal
        // buffer when stdin is not a TTY causes the two to desync and
        // corrupts subsequent menu reads.
        std::cout << "  Enter PIN: ";
        std::string rawPin;
#if defined(_WIN32)
        rawPin = security::ConsoleMask::readMaskedInput();
#else
        if (isatty(STDIN_FILENO)) {
            rawPin = security::ConsoleMask::readMaskedInput();
        } else {
            std::getline(std::cin, rawPin);
        }
#endif
        std::cout << "\n";

        // Authenticate via BankSystem
        const auto authResult = bankSystem_.authenticate(accountNo, rawPin);
        if (!authResult) {
            std::cout << "  Error: " << authResult.error().message << "\n\n";

            // If the account is now locked, no point retrying
            using core::BankErrorCode;
            if (authResult.error().code == BankErrorCode::AccountLocked) {
                return false;
            }
            continue;
        }

        // Successful login — retrieve the account to greet customer
        const auto accountLookup = bankSystem_.findAccountByNumber(accountNo);
        if (accountLookup) {
            std::cout << "\n  Welcome, " << accountLookup.value().customerName() << "!\n";
        }

        outAccountNumber = accountNo;
        return true;
    }

    std::cout << "\n  Too many failed login attempts. Session terminated.\n";
    return false;
}

// ---------------------------------------------------------------------------
// Authenticated menu
// ---------------------------------------------------------------------------

void AtmConsole::showMenu() const {
    std::cout << "\n";
    printSeparator();
    std::cout << "  ATM MENU\n";
    printSeparator();
    std::cout << "  [1] Balance Inquiry\n";
    std::cout << "  [2] Cash Withdrawal\n";
    std::cout << "  [3] Cash Deposit\n";
    std::cout << "  [4] Fund Transfer\n";
    std::cout << "  [5] Mini-Statement (last 5 transactions)\n";
    std::cout << "  [6] Logout\n";
    printSeparator();
    std::cout << "  Select: ";
}

// ---------------------------------------------------------------------------
// Balance Inquiry
// ---------------------------------------------------------------------------

void AtmConsole::handleBalanceInquiry(long accountNumber) const {
    const auto result = bankSystem_.findAccountByNumber(accountNumber);
    if (!result) {
        std::cout << "\n  Error: " << result.error().message << "\n";
        return;
    }
    const domain::Account& account = result.value();

    printSeparator();
    std::cout << "  BALANCE INQUIRY\n";
    printSeparator();
    std::cout << "  Account : " << account.accountNumber() << "\n";
    std::cout << "  Holder  : " << account.customerName()  << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Balance : Rs. " << account.balance() << "\n";
    printSeparator();
}

// ---------------------------------------------------------------------------
// Cash Withdrawal
// ---------------------------------------------------------------------------

void AtmConsole::handleWithdrawal(long accountNumber) {
    printSeparator();
    std::cout << "  CASH WITHDRAWAL\n";
    printSeparator();

    const auto amountResult =
        validation::FieldValidator::readPositiveAmount("  Enter amount to withdraw: Rs. ");
    if (!amountResult) {
        std::cout << "  " << amountResult.error().message << "\n";
        return;
    }
    const double amount = amountResult.value();

    // OTP required for withdrawals above the threshold
    if (amount > OtpService::OtpThreshold) {
        std::cout << "\n  This withdrawal exceeds Rs. 50,000 and requires OTP verification.\n";
        if (!runOtpFlow()) {
            std::cout << "  Transaction cancelled: OTP verification failed.\n";
            return;
        }
    }

    const auto result = bankSystem_.withdraw(accountNumber, amount);
    if (!result) {
        std::cout << "\n  Transaction failed: " << result.error().message << "\n";
        return;
    }

    const domain::Transaction& txn = result.value();

    // Look up account for receipt
    const auto accountResult = bankSystem_.findAccountByNumber(accountNumber);
    if (accountResult) {
        finaliseTransaction(accountResult.value(), txn);
    }
}

// ---------------------------------------------------------------------------
// Cash Deposit
// ---------------------------------------------------------------------------

void AtmConsole::handleDeposit(long accountNumber) {
    printSeparator();
    std::cout << "  CASH DEPOSIT\n";
    printSeparator();

    const auto amountResult =
        validation::FieldValidator::readPositiveAmount("  Enter amount to deposit: Rs. ");
    if (!amountResult) {
        std::cout << "  " << amountResult.error().message << "\n";
        return;
    }
    const double amount = amountResult.value();

    const auto result = bankSystem_.deposit(accountNumber, amount);
    if (!result) {
        std::cout << "\n  Transaction failed: " << result.error().message << "\n";
        return;
    }

    const domain::Transaction& txn = result.value();

    const auto accountResult = bankSystem_.findAccountByNumber(accountNumber);
    if (accountResult) {
        finaliseTransaction(accountResult.value(), txn);
    }
}

// ---------------------------------------------------------------------------
// Fund Transfer
// ---------------------------------------------------------------------------

void AtmConsole::handleTransfer(long accountNumber) {
    printSeparator();
    std::cout << "  FUND TRANSFER\n";
    printSeparator();

    const auto toResult =
        validation::FieldValidator::readAccountNumber("  Enter destination account number: ");
    if (!toResult) {
        std::cout << "  " << toResult.error().message << "\n";
        return;
    }
    const long toAccount = toResult.value();

    if (toAccount == accountNumber) {
        std::cout << "  Cannot transfer to your own account.\n";
        return;
    }

    const auto amountResult =
        validation::FieldValidator::readPositiveAmount("  Enter amount to transfer: Rs. ");
    if (!amountResult) {
        std::cout << "  " << amountResult.error().message << "\n";
        return;
    }
    const double amount = amountResult.value();

    // OTP required for transfers above the threshold
    if (amount > OtpService::OtpThreshold) {
        std::cout << "\n  This transfer exceeds Rs. 50,000 and requires OTP verification.\n";
        if (!runOtpFlow()) {
            std::cout << "  Transaction cancelled: OTP verification failed.\n";
            return;
        }
    }

    const auto result = bankSystem_.transfer(accountNumber, toAccount, amount);
    if (!result) {
        std::cout << "\n  Transaction failed: " << result.error().message << "\n";
        return;
    }

    const domain::Transaction& txn = result.value();

    const auto accountResult = bankSystem_.findAccountByNumber(accountNumber);
    if (accountResult) {
        finaliseTransaction(accountResult.value(), txn);
    }
}

// ---------------------------------------------------------------------------
// Mini-Statement
// ---------------------------------------------------------------------------

void AtmConsole::handleMiniStatement(long accountNumber) const {
    printSeparator();
    std::cout << "  MINI-STATEMENT — Last 5 Transactions\n";
    printSeparator();

    const std::vector<domain::Transaction> recent =
        loadRecentTransactions(accountNumber, 5);

    if (recent.empty()) {
        std::cout << "  No transactions found for this account.\n";
        return;
    }

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& txn : recent) {
        std::string typeStr;
        switch (txn.type()) {
            case domain::TransactionType::Deposit:     typeStr = "Deposit       "; break;
            case domain::TransactionType::Withdrawal:  typeStr = "Withdrawal    "; break;
            case domain::TransactionType::TransferOut: typeStr = "Transfer Out  "; break;
            case domain::TransactionType::TransferIn:  typeStr = "Transfer In   "; break;
        }
        std::cout << "  " << txn.timestamp()
                  << "  " << typeStr
                  << "  Rs. " << std::setw(12) << txn.amount()
                  << "  Bal: Rs. " << txn.balanceAfter()
                  << "\n";
    }
    printSeparator();
}

// ---------------------------------------------------------------------------
// OTP flow
// ---------------------------------------------------------------------------

bool AtmConsole::runOtpFlow() {
    const std::string otp = otpService_.generate();

    // In a real system this would be sent via SMS. For simulation we display it.
    std::cout << "\n  [SIMULATION] Your OTP has been sent to your registered mobile.\n";
    std::cout << "  OTP: " << otp << "\n\n";

    constexpr int MaxOtpAttempts = 3;
    for (int attempt = 1; attempt <= MaxOtpAttempts; ++attempt) {
        std::cout << "  Enter OTP (Attempt " << attempt << " of " << MaxOtpAttempts << "): ";
        std::string entered;
        std::cin >> entered;
        validation::FieldValidator::clearInputBuffer();

        if (otpService_.verify(entered)) {
            std::cout << "  OTP verified successfully.\n";
            return true;
        }
        std::cout << "  Incorrect OTP.\n";
    }

    return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void AtmConsole::finaliseTransaction(const domain::Account& account,
                                     const domain::Transaction& transaction) {
    printSeparator();
    std::cout << "  TRANSACTION SUCCESSFUL\n";
    printSeparator();
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Transaction ID : " << transaction.id()          << "\n";
    std::cout << "  Date & Time    : " << transaction.timestamp()   << "\n";
    std::cout << "  Amount         : Rs. " << transaction.amount()  << "\n";
    std::cout << "  Balance After  : Rs. " << transaction.balanceAfter() << "\n";
    printSeparator();

    ReceiptWriter::write(account, transaction);
    std::cout << "  Receipt saved to receipts/ folder.\n";
}

std::vector<domain::Transaction>
AtmConsole::loadRecentTransactions(long accountNumber, int count) const {
    persistence::TransactionJournal reader(journalPath_);
    const auto allResult = reader.loadAll();
    if (!allResult) {
        return {};
    }

    // Filter by account number
    std::vector<domain::Transaction> filtered;
    for (const auto& txn : allResult.value()) {
        if (txn.accountNumber() == accountNumber) {
            filtered.push_back(txn);
        }
    }

    // Return the last `count` entries (already in chronological order)
    if (static_cast<int>(filtered.size()) > count) {
        filtered.erase(filtered.begin(),
                       filtered.begin() +
                           static_cast<std::ptrdiff_t>(filtered.size() - count));
    }
    return filtered;
}

void AtmConsole::printSeparator() {
    std::cout << "  --------------------------------------------------\n";
}

} // namespace securebank::ui
