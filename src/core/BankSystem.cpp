/**
 * @file BankSystem.cpp
 * @brief Implementation of the BankSystem orchestration facade.
 */

#include "core/BankSystem.hpp"
#include <algorithm>
#include <chrono>
#include <format>
#include "security/PinHasher.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace securebank::core {

using domain::Account;
using domain::AccountType;
using domain::AccountStatus;
using domain::Transaction;
using domain::TransactionType;
using security::AuthOutcome;

BankSystem::BankSystem(std::unique_ptr<persistence::AccountLedgerStore> accountStore,
                        std::unique_ptr<persistence::TransactionJournal> journal)
    : accountStore_(std::move(accountStore)),
      journal_(std::move(journal)),
      authGuard_(3) {}

core::Expected<void, BankError> BankSystem::initialize() {
    auto loadedAccounts = accountStore_->loadAll();
    if (!loadedAccounts) return core::Unexpected(loadedAccounts.error());
    accounts_ = std::move(loadedAccounts.value());

    auto loadedTransactions = journal_->loadAll();
    if (!loadedTransactions) return core::Unexpected(loadedTransactions.error());

    nextTransactionId_ = 1;
    for (const auto& txn : loadedTransactions.value()) {
        if (txn.id() >= nextTransactionId_) {
            nextTransactionId_ = txn.id() + 1;
        }
    }
    if (auto counterLoaded = loadAccountNumberCounter(); !counterLoaded) {
    return core::Unexpected(counterLoaded.error());
}
    return {};
}

std::string BankSystem::currentTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string BankSystem::currentDate() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d");
    return oss.str();
}
Account* BankSystem::findAccountMutable(long accountNumber) noexcept {
    auto it = std::find_if(accounts_.begin(), accounts_.end(),
        [accountNumber](const Account& a) { return a.accountNumber() == accountNumber; });
    return it != accounts_.end() ? &(*it) : nullptr;
}

core::Expected<void, BankError> BankSystem::saveAccounts() const {
    return accountStore_->persistAll(accounts_);
}

core::Expected<void, BankError> BankSystem::loadAccountNumberCounter() {
    const std::filesystem::path counterPath = "data/account_counter.txt";
    if (!std::filesystem::exists(counterPath)) {
        nextAccountNumber_ = 1001; // First-ever run
        return {};
    }
    std::ifstream in(counterPath);
    if (!in.is_open()) {
        return core::Unexpected(BankError{BankErrorCode::FileReadFailure,
                                           "Could not open account counter file."});
    }
    in >> nextAccountNumber_;
    return {};
}

core::Expected<void, BankError> BankSystem::saveAccountNumberCounter() const {
    std::filesystem::create_directories("data");
    std::ofstream out("data/account_counter.txt", std::ios::trunc);
    if (!out.is_open()) {
        return core::Unexpected(BankError{BankErrorCode::FileWriteFailure,
                                           "Could not save account counter file."});
    }
    out << nextAccountNumber_;
    return {};
}

core::Expected<Account, BankError>
BankSystem::createAccount(std::string customerName, std::string cnic, std::string phone,
                           AccountType type, std::string hashedPin, double openingBalance) {
    const bool cnicTaken = std::any_of(accounts_.begin(), accounts_.end(),
        [&cnic](const Account& a) { return a.cnic() == cnic; });
    if (cnicTaken) {
        return core::Unexpected(BankError{BankErrorCode::AccountAlreadyExists,
                                          "An account with this CNIC already exists."});
    }

    const long newAccountNumber = nextAccountNumber_;

    accounts_.emplace_back(newAccountNumber, std::move(customerName), std::move(cnic),
                            std::move(phone), type, std::move(hashedPin), openingBalance);

    if (auto saved = saveAccounts(); !saved) {
        accounts_.pop_back();
        return core::Unexpected(saved.error());
    }
    ++nextAccountNumber_;
    if (auto counterSaved = saveAccountNumberCounter(); !counterSaved) {
        return core::Unexpected(counterSaved.error());
    }

    return accounts_.back();
}

core::Expected<void, BankError> BankSystem::deleteAccount(long accountNumber) {
    auto it = std::find_if(accounts_.begin(), accounts_.end(),
        [accountNumber](const Account& a) { return a.accountNumber() == accountNumber; });
    if (it == accounts_.end()) {
        return core::Unexpected(BankError{BankErrorCode::AccountNotFound, "Account not found."});
    }
    accounts_.erase(it);
    return saveAccounts();
}

core::Expected<void, BankError> BankSystem::unlockAccount(long accountNumber) {
    Account* account = findAccountMutable(accountNumber);
    if (!account) {
        return core::Unexpected(BankError{BankErrorCode::AccountNotFound, "Account not found."});
    }
    account->unlock();
    return saveAccounts();
}

core::Expected<Account, BankError> BankSystem::findAccountByNumber(long accountNumber) const {
    auto it = std::find_if(accounts_.begin(), accounts_.end(),
        [accountNumber](const Account& a) { return a.accountNumber() == accountNumber; });
    if (it == accounts_.end()) {
        return core::Unexpected(BankError{BankErrorCode::AccountNotFound, "Account not found."});
    }
    return *it;
}

const std::vector<Account>& BankSystem::allAccounts() const noexcept {
    return accounts_;
}

core::Expected<AuthOutcome, BankError>
BankSystem::authenticate(long accountNumber, std::string_view rawPin) {
    Account* account = findAccountMutable(accountNumber);
    if (!account) {
        return core::Unexpected(BankError{BankErrorCode::AccountNotFound, "Account not found."});
    }

    const std::string hashedAttempt = security::PinHasher::hash(rawPin);
    const AuthOutcome outcome = authGuard_.attempt(*account, hashedAttempt);

    if (auto saved = saveAccounts(); !saved) {
        return core::Unexpected(saved.error());
    }
    return outcome;
}

core::Expected<Transaction, BankError> BankSystem::deposit(long accountNumber, double amount) {
    Account* account = findAccountMutable(accountNumber);
    if (!account) return core::Unexpected(BankError{BankErrorCode::AccountNotFound, "Account not found."});
    if (account->status() != AccountStatus::Active) {
        return core::Unexpected(BankError{BankErrorCode::AccountLocked, "Account is not active."});
    }

    account->credit(amount);

    Transaction txn(nextTransactionId_++, accountNumber, TransactionType::Deposit,
                     amount, currentTimestamp(), account->balance());

    if (auto saved = saveAccounts(); !saved) return core::Unexpected(saved.error());
    if (auto logged = journal_->append(txn); !logged) return core::Unexpected(logged.error());
    return txn;
}

core::Expected<Transaction, BankError> BankSystem::withdraw(long accountNumber, double amount) {
    Account* account = findAccountMutable(accountNumber);
    if (!account) return core::Unexpected(BankError{BankErrorCode::AccountNotFound, "Account not found."});
    if (account->status() != AccountStatus::Active) {
        return core::Unexpected(BankError{BankErrorCode::AccountLocked, "Account is not active."});
    }

    account->resetDailyWithdrawalIfNewDay(currentDate());
    if (account->dailyWithdrawn() + amount > account->dailyLimit()) {
        return core::Unexpected(BankError{BankErrorCode::DailyLimitExceeded,
                                           "This withdrawal would exceed the daily limit."});
    }

    if (!account->debit(amount)) {
        return core::Unexpected(BankError{BankErrorCode::InsufficientFunds, "Insufficient balance."});
    }
    account->accumulateDailyWithdrawal(amount);

    Transaction txn(nextTransactionId_++, accountNumber, TransactionType::Withdrawal,
                     amount, currentTimestamp(), account->balance());

    if (auto saved = saveAccounts(); !saved) return core::Unexpected(saved.error());
    if (auto logged = journal_->append(txn); !logged) return core::Unexpected(logged.error());
    return txn;
}

core::Expected<Transaction, BankError>
BankSystem::transfer(long fromAccount, long toAccount, double amount) {
    Account* from = findAccountMutable(fromAccount);
    Account* to = findAccountMutable(toAccount);

    if (!from || !to) {
        return core::Unexpected(BankError{BankErrorCode::AccountNotFound,
                                          "Source or destination account not found."});
    }
    if (from->status() != AccountStatus::Active || to->status() != AccountStatus::Active) {
        return core::Unexpected(BankError{BankErrorCode::AccountLocked,
                                          "Both accounts must be active to transfer."});
    }
    if (!from->debit(amount)) {
        return core::Unexpected(BankError{BankErrorCode::InsufficientFunds, "Insufficient balance."});
    }
    to->credit(amount);

    Transaction outTxn(nextTransactionId_++, fromAccount, TransactionType::TransferOut,
                        amount, currentTimestamp(), from->balance(), toAccount);
    Transaction inTxn(nextTransactionId_++, toAccount, TransactionType::TransferIn,
                       amount, currentTimestamp(), to->balance(), fromAccount);

    if (auto saved = saveAccounts(); !saved) return core::Unexpected(saved.error());
    if (auto logged = journal_->append(outTxn); !logged) return core::Unexpected(logged.error());
    if (auto logged = journal_->append(inTxn); !logged) return core::Unexpected(logged.error());

    return outTxn;
}

} // namespace securebank::core