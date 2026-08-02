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
    /**
     * @brief Constructs a new active account.
     * @param accountNumber Unique numeric identifier.
     * @param customerName  Full name of the account holder.
     * @param cnic          National ID number.
     * @param phone         Contact phone number.
     * @param type          Savings or Current.
     * @param hashedPin     Pre-hashed 4-digit PIN (never stored in plaintext).
     * @param openingBalance Initial deposit amount (must be >= 0).
     */
    Account(long accountNumber,
            std::string customerName,
            std::string cnic,
            std::string phone,
            AccountType type,
            std::string hashedPin,
            double openingBalance);

    // Read-only accessors 
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

    //Controlled mutators 

    /// Adds funds to the account. Throws std::invalid_argument if amount <= 0.
    void credit(double amount);

    /// Attempts to remove funds. Returns false (no state change) if insufficient balance.
    [[nodiscard]] bool debit(double amount);

    /// Records a wrong-PIN attempt (used by AuthGuard).
    void registerFailedPinAttempt() noexcept;

    /// Clears the failed-attempt counter (called after a successful login).
    void resetPinAttempts() noexcept;

    /// Locks the account (called by AuthGuard after too many failed attempts).
    void lock() noexcept;

    /// Unlocks the account. Only ever called from admin-authorized code paths.
    void unlock() noexcept;

    /// Marks the account permanently closed.
    void close() noexcept;

    /// Tracks cumulative same-day withdrawals against the daily limit.
    void accumulateDailyWithdrawal(double amount);

    /// Resets the daily withdrawal counter if the stored date differs from today.
    void resetDailyWithdrawalIfNewDay(std::string_view todayDate);

    /// Compares a hashed PIN attempt against the stored hash.
    [[nodiscard]] bool verifyPin(std::string_view hashedAttempt) const noexcept;

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