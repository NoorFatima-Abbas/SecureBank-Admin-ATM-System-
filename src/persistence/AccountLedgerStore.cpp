/**
 * @file AccountLedgerStore.cpp
 * @brief Implementation of accounts.txt read/write with atomic save semantics.
 */

#include "persistence/AccountLedgerStore.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include<filesystem>

namespace securebank::persistence {

using domain::Account;
using domain::AccountType;
using domain::AccountStatus;
using core::BankError;
using core::BankErrorCode;

AccountLedgerStore::AccountLedgerStore(std::filesystem::path filePath)
    : filePath_(std::move(filePath)) {}

namespace {
    [[nodiscard]] std::vector<std::string> splitPipe(std::string_view line) {
        std::vector<std::string> tokens;
        std::string current;
        for (const char ch : line) {
            if (ch == '|') {
                tokens.push_back(current);
                current.clear();
            } else {
                current += ch;
            }
        }
        tokens.push_back(current);
        return tokens;
    }

    [[nodiscard]] AccountType parseType(const std::string& text) {
        return text == "SAVINGS" ? AccountType::Savings : AccountType::Current;
    }

    [[nodiscard]] AccountStatus parseStatus(const std::string& text) {
        if (text == "LOCKED") return AccountStatus::Locked;
        if (text == "CLOSED") return AccountStatus::Closed;
        return AccountStatus::Active;
    }
}

core::Expected<Account, BankError> AccountLedgerStore::parseLine(std::string_view line) const {
    const std::vector<std::string> f = splitPipe(line);
    if (f.size() != 12) {
        return core::Unexpected(BankError{BankErrorCode::FileReadFailure,
                                           "Malformed account record (field count mismatch)."});
    }
    try {
        return Account(
            std::stol(f[0]),           // accountNumber
            f[1],                      // customerName
            f[2],                      // cnic
            f[3],                      // phone
            parseType(f[4]),           // type
            parseStatus(f[5]),         // status
            f[6],                      // hashedPin
            std::stod(f[7]),           // balance
            std::stoi(f[8]),           // failedPinAttempts
            std::stod(f[9]),           // dailyWithdrawn
            std::stod(f[10]),          // dailyLimit
            f[11]                      // lastWithdrawalDate
        );
    } catch (const std::exception&) {
        return core::Unexpected(BankError{BankErrorCode::FileReadFailure,
                                           "Malformed numeric field in account record."});
    }
}

core::Expected<std::vector<Account>, BankError> AccountLedgerStore::loadAll() const {
    std::vector<Account> accounts;

    if (!std::filesystem::exists(filePath_)) {
        return accounts; // No file yet = empty ledger, not an error.
    }

    std::ifstream in(filePath_);
    if (!in.is_open()) {
        return core::Unexpected(BankError{BankErrorCode::FileReadFailure,
                                           "Could not open accounts.txt for reading."});
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto parsed = parseLine(line);
        if (!parsed) {
            return core::Unexpected(parsed.error());
        }
        accounts.push_back(std::move(parsed.value()));
    }
    return accounts;
}

core::Expected<void, BankError>
AccountLedgerStore::persistAll(const std::vector<Account>& accounts) const {
    const std::filesystem::path tempPath = filePath_.string() + ".tmp";
    std::filesystem::create_directories(filePath_.parent_path());

    {
        std::ofstream out(tempPath, std::ios::trunc);
        if (!out.is_open()) {
            return core::Unexpected(BankError{BankErrorCode::FileWriteFailure,
                                               "Could not open temp file for writing accounts."});
        }
        for (const auto& account : accounts) {
            out << account.toRecordLine() << '\n';
        }
    } // out closes here (RAII) before rename

    std::error_code ec;
    std::filesystem::rename(tempPath, filePath_, ec);
    if (ec) {
        return core::Unexpected(BankError{BankErrorCode::FileWriteFailure,
                                          "Atomic rename of accounts.txt failed: " + ec.message()});
    }
    return {};
}

} // namespace securebank::persistence