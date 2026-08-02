/**
 * @file Account.hpp
 * @brief Represents a single bank account record and enforces its own
 * internal invariants (balance never negative, controlled state transitions).
 * @details Account is a pure domain object — it performs no file or console
 * I/O and has no knowledge of BankSystem. This isolation makes it trivially
 * unit-testable and keeps business rules out of the persistence layer.
 */

#pragma once
#include <string>
#include <string_view>
#include "domain/Enums.hpp"

namespace securebank::domain {

class Account {
public:
    /// Constructs a new active account (used when creating a fresh customer record).
    Account(long accountNumber,
            std::string customerName,
            std::string cnic,
            std::string phone,
            AccountType type,
            std::string hashedPin,
            double openingBalance);

    /// Reconstructs an account from persisted storage with full historical state.
    /// @details Used exclusively by AccountLedgerStore when loading accounts.txt.
    Account(long accountNumber,
            std::string customerName,
            std::string cnic,
            std::string phone,
            AccountType type,
            AccountStatus status,
            std::string hashedPin,
            double balance,
            int failedPinAttempts,
            double dailyWithdrawn,
            double dailyLimit,
            std::string lastWithdrawalDate);

    // --- Read-only accessors ---
    [[nodiscard]] long accountNumber() const noexcept;
    [[nodiscard]] std::string_view customerName() const noexcept;
    [[nodiscard]] std::string_view cnic() const noexcept;
    [[nodiscard]] std::string_view phone() const noexcept;
    [[nodiscard]] AccountType type() const noexcept;
    [[nodiscard]] AccountStatus status() const noexcept;
    [[nodiscard]] double balance() const noexcept;
    [[nodiscard]] int failedPinAttempts() const noexcept;
    [[nodiscard]] double dailyWithdrawn() const noexcept;
    [[nodiscard]] double dailyLimit() const noexcept;
    [[nodiscard]] std::string_view hashedPin() const noexcept;
    [[nodiscard]] std::string_view lastWithdrawalDate() const noexcept;

    // --- Controlled mutators ---
    void credit(double amount);
    [[nodiscard]] bool debit(double amount);
    void registerFailedPinAttempt() noexcept;
    void resetPinAttempts() noexcept;
    void lock() noexcept;
    void unlock() noexcept;
    void close() noexcept;
    void accumulateDailyWithdrawal(double amount);
    void resetDailyWithdrawalIfNewDay(std::string_view todayDate);
    [[nodiscard]] bool verifyPin(std::string_view hashedAttempt) const noexcept;

    /// Serializes this account into the pipe-delimited accounts.txt line format.
    [[nodiscard]] std::string toRecordLine() const;

private:
    long accountNumber_;
    std::string customerName_;
    std::string cnic_;
    std::string phone_;
    AccountType type_;
    AccountStatus status_{AccountStatus::Active};
    std::string hashedPin_;
    double balance_;
    int failedPinAttempts_{0};
    double dailyWithdrawn_{0.0};
    double dailyLimit_{50000.0};
    std::string lastWithdrawalDate_{};
};

} // namespace securebank::domain