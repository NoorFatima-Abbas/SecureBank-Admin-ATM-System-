/**
 * @file AtmConsole.hpp
 * @brief ATM-facing console UI — customer self-service terminal.
 * @details Handles customer login, balance inquiry, withdrawals, deposits,
 * fund transfers (with OTP for amounts > Rs. 50,000), mini-statements,
 * and automatic receipt generation after every successful transaction.
 *
 * Presentation only — all financial logic delegates to BankSystem.
 * Owned by: Ayesha Kamran
 */

#pragma once
#include "core/BankSystem.hpp"
#include "persistence/TransactionJournal.hpp"
#include "ui/OtpService.hpp"
#include <filesystem>

namespace securebank::ui {

class AtmConsole {
public:
    /// @param bankSystem   Shared BankSystem instance (owns account/txn state).
    /// @param journalPath  Path to transactions.txt — used for mini-statement reads.
    AtmConsole(core::BankSystem& bankSystem,
               std::filesystem::path journalPath) noexcept;

    /// Runs the ATM session loop until the customer logs out or exits.
    void run();

private:
    core::BankSystem& bankSystem_;
    std::filesystem::path journalPath_;
    OtpService otpService_;

    // ---- Login ----
    [[nodiscard]] bool handleLogin(long& outAccountNumber) const;

    // ---- Authenticated menu ----
    void showMenu() const;
    void handleBalanceInquiry(long accountNumber) const;
    void handleWithdrawal(long accountNumber);
    void handleDeposit(long accountNumber);
    void handleTransfer(long accountNumber);
    void handleMiniStatement(long accountNumber) const;

    // ---- OTP flow ----
    /// Prompts the customer to enter their OTP. Returns true if verified.
    [[nodiscard]] bool runOtpFlow();

    // ---- Helpers ----
    /// Prints the transaction summary and writes a receipt file.
    static void finaliseTransaction(const domain::Account& account,
                                    const domain::Transaction& transaction);

    /// Loads the last @p count transactions for @p accountNumber from disk.
    [[nodiscard]] std::vector<domain::Transaction>
        loadRecentTransactions(long accountNumber, int count) const;

    static void printSeparator();
};

} // namespace securebank::ui
