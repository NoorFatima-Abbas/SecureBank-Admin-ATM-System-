/**
 * @file Account.cpp
 * @brief Implementation of Account's invariant-enforcing mutators and serialization.
 */

#include "domain/Account.hpp"
#include <stdexcept>
#include <sstream>

namespace securebank::domain {

namespace {
    [[nodiscard]] std::string typeToString(AccountType type) {
        return type == AccountType::Savings ? "SAVINGS" : "CURRENT";
    }
    [[nodiscard]] std::string statusToString(AccountStatus status) {
        switch (status) {
            case AccountStatus::Active: return "ACTIVE";
            case AccountStatus::Locked: return "LOCKED";
            case AccountStatus::Closed: return "CLOSED";
        }
        return "ACTIVE";
    }
}

Account::Account(long accountNumber,
                  std::string customerName,
                  std::string cnic,
                  std::string phone,
                  AccountType type,
                  std::string hashedPin,
                  double openingBalance)
    : accountNumber_(accountNumber),
      customerName_(std::move(customerName)),
      cnic_(std::move(cnic)),
      phone_(std::move(phone)),
      type_(type),
      hashedPin_(std::move(hashedPin)),
      balance_(openingBalance) {
    if (openingBalance < 0.0) {
        throw std::invalid_argument("Opening balance cannot be negative.");
    }
}

Account::Account(long accountNumber,
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
                  std::string lastWithdrawalDate)
    : accountNumber_(accountNumber),
      customerName_(std::move(customerName)),
      cnic_(std::move(cnic)),
      phone_(std::move(phone)),
      type_(type),
      status_(status),
      hashedPin_(std::move(hashedPin)),
      balance_(balance),
      failedPinAttempts_(failedPinAttempts),
      dailyWithdrawn_(dailyWithdrawn),
      dailyLimit_(dailyLimit),
      lastWithdrawalDate_(std::move(lastWithdrawalDate)) {}

long Account::accountNumber() const noexcept { return accountNumber_; }
std::string_view Account::customerName() const noexcept { return customerName_; }
std::string_view Account::cnic() const noexcept { return cnic_; }
std::string_view Account::phone() const noexcept { return phone_; }
AccountType Account::type() const noexcept { return type_; }
AccountStatus Account::status() const noexcept { return status_; }
double Account::balance() const noexcept { return balance_; }
int Account::failedPinAttempts() const noexcept { return failedPinAttempts_; }
double Account::dailyWithdrawn() const noexcept { return dailyWithdrawn_; }
double Account::dailyLimit() const noexcept { return dailyLimit_; }
std::string_view Account::hashedPin() const noexcept { return hashedPin_; }
std::string_view Account::lastWithdrawalDate() const noexcept { return lastWithdrawalDate_; }

void Account::credit(double amount) {
    if (amount <= 0.0) throw std::invalid_argument("Credit amount must be positive.");
    balance_ += amount;
}

bool Account::debit(double amount) {
    if (amount <= 0.0) throw std::invalid_argument("Debit amount must be positive.");
    if (amount > balance_) return false;
    balance_ -= amount;
    return true;
}

void Account::registerFailedPinAttempt() noexcept { ++failedPinAttempts_; }
void Account::resetPinAttempts() noexcept { failedPinAttempts_ = 0; }
void Account::lock() noexcept { status_ = AccountStatus::Locked; }
void Account::unlock() noexcept { status_ = AccountStatus::Active; failedPinAttempts_ = 0; }
void Account::close() noexcept { status_ = AccountStatus::Closed; }

void Account::accumulateDailyWithdrawal(double amount) {
    if (amount <= 0.0) throw std::invalid_argument("Withdrawal accumulation must be positive.");
    dailyWithdrawn_ += amount;
}

void Account::resetDailyWithdrawalIfNewDay(std::string_view todayDate) {
    if (lastWithdrawalDate_ != todayDate) {
        dailyWithdrawn_ = 0.0;
        lastWithdrawalDate_ = std::string(todayDate);
    }
}

bool Account::verifyPin(std::string_view hashedAttempt) const noexcept {
    return hashedPin_ == hashedAttempt;
}

std::string Account::toRecordLine() const {
    std::ostringstream oss;
    oss << accountNumber_ << '|'
        << customerName_ << '|'
        << cnic_ << '|'
        << phone_ << '|'
        << typeToString(type_) << '|'
        << statusToString(status_) << '|'
        << hashedPin_ << '|'
        << balance_ << '|'
        << failedPinAttempts_ << '|'
        << dailyWithdrawn_ << '|'
        << dailyLimit_ << '|'
        << lastWithdrawalDate_;
    return oss.str();
}

} // namespace securebank::domain