/**
 * @file BankSystem.hpp
 * @brief Central orchestration engine shared by the Admin and ATM modules.
 * @details Single source of truth: holds in-memory account/transaction state,
 * delegates persistence to the store classes, and enforces cross-cutting
 * business rules (uniqueness, lockout policy, transfer validity). Neither
 * AdminConsole nor AtmConsole ever touch files or vectors directly.
 */

#pragma once
#include <vector>
#include <memory>
#include "core/Expected.hpp"
#include "domain/Account.hpp"
#include "domain/Transaction.hpp"
#include "persistence/AccountLedgerStore.hpp"
#include "persistence/TransactionJournal.hpp"
#include "security/AuthGuard.hpp"
#include "core/BankError.hpp"

namespace securebank::core {

class BankSystem {
public:
    BankSystem(std::unique_ptr<persistence::AccountLedgerStore> accountStore,
               std::unique_ptr<persistence::TransactionJournal> journal);

    /// Loads accounts, transactions, and the account-number counter from disk. Call once at startup.
    [[nodiscard]] core::Expected<void, BankError> initialize();

    // --- Admin-facing operations ---
    [[nodiscard]] core::Expected<domain::Account, BankError>
        createAccount(std::string customerName, std::string cnic, std::string phone,
                      domain::AccountType type, std::string hashedPin, double openingBalance);

    [[nodiscard]] core::Expected<void, BankError> deleteAccount(long accountNumber);
    [[nodiscard]] core::Expected<void, BankError> unlockAccount(long accountNumber);
    [[nodiscard]] core::Expected<domain::Account, BankError> findAccountByNumber(long accountNumber) const;
    [[nodiscard]] const std::vector<domain::Account>& allAccounts() const noexcept;

    // --- Shared / ATM-facing operations ---
    [[nodiscard]] core::Expected<security::AuthOutcome, BankError>
        authenticate(long accountNumber, std::string_view rawPin);

    [[nodiscard]] core::Expected<domain::Transaction, BankError>
        withdraw(long accountNumber, double amount);

    [[nodiscard]] core::Expected<domain::Transaction, BankError>
        deposit(long accountNumber, double amount);

    [[nodiscard]] core::Expected<domain::Transaction, BankError>
        transfer(long fromAccount, long toAccount, double amount);

private:
    std::unique_ptr<persistence::AccountLedgerStore> accountStore_;
    std::unique_ptr<persistence::TransactionJournal> journal_;
    security::AuthGuard authGuard_;
    std::vector<domain::Account> accounts_;
    long nextTransactionId_{1};
    long nextAccountNumber_{1001};

    /// Internal, non-owning lookup helper. Never exposed publicly.
    [[nodiscard]] domain::Account* findAccountMutable(long accountNumber) noexcept;

    /// Persists the full in-memory account vector back to disk.
    [[nodiscard]] core::Expected<void, BankError> saveAccounts() const;

    /// Loads the monotonically increasing account-number counter from disk.
    [[nodiscard]] core::Expected<void, BankError> loadAccountNumberCounter();

    /// Persists the current account-number counter to disk.
    [[nodiscard]] core::Expected<void, BankError> saveAccountNumberCounter() const;

    [[nodiscard]] static std::string currentTimestamp();
    [[nodiscard]] static std::string currentDate();
};

} // namespace securebank::core