/**
 * @file Account.cpp
 * @brief Implementation of Account's invariant-enforcing mutators.
 */

#include "domain/Account.hpp"
#include <stdexcept>

namespace securebank::domain {

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

long Account::accountNumber() const noexcept {
     return accountNumber_; 
    }
std::string_view Account::customerName() const noexcept {
     return customerName_;
     }
std::string_view Account::cnic() const noexcept {
     return cnic_; 
    }
std::string_view Account::phone() const noexcept {
     return phone_; 
    }
AccountType Account::type() const noexcept {
     return type_;
     }
AccountStatus Account::status() const noexcept { 
    return status_; 
}
double Account::balance() const noexcept {
     return balance_; 
    }
int Account::failedPinAttempts() const noexcept {
     return failedPinAttempts_; 
    }
double Account::dailyWithdrawn() const noexcept { 
    return dailyWithdrawn_; 
}
double Account::dailyLimit() const noexcept { 
    return dailyLimit_; 
}
std::string_view Account::hashedPin() const noexcept { 
    return hashedPin_; 
}
std::string_view Account::lastWithdrawalDate() const noexcept {
     return lastWithdrawalDate_; 
    }

void Account::credit(double amount) {
    if (amount <= 0.0) {
        throw std::invalid_argument("Credit amount must be positive.");
    }
    balance_ += amount;
}

bool Account::debit(double amount) {
    if (amount <= 0.0) {
        throw std::invalid_argument("Debit amount must be positive.");
    }
    if (amount > balance_) {
        return false;
    }
    balance_ -= amount;
    return true;
}

void Account::registerFailedPinAttempt() noexcept {
    ++failedPinAttempts_;
}

void Account::resetPinAttempts() noexcept {
    failedPinAttempts_ = 0;
}

void Account::lock() noexcept {
    status_ = AccountStatus::Locked;
}

void Account::unlock() noexcept {
    status_ = AccountStatus::Active;
    failedPinAttempts_ = 0;
}

void Account::close() noexcept {
    status_ = AccountStatus::Closed;
}

void Account::accumulateDailyWithdrawal(double amount) {
    if (amount <= 0.0) {
        throw std::invalid_argument("Withdrawal accumulation must be positive.");
    }
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

} // namespace securebank::domain