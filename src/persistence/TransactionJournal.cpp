/**
 * @file TransactionJournal.cpp
 * @brief Implementation of transactions.txt append/load persistence.
 */

#include "persistence/TransactionJournal.hpp"
#include <fstream>
#include <filesystem>

namespace securebank::persistence {

using domain::Transaction;
using domain::TransactionType;
using core::BankError;
using core::BankErrorCode;

TransactionJournal::TransactionJournal(std::filesystem::path filePath)
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

    [[nodiscard]] TransactionType parseType(const std::string& text) {
        if (text == "WITHDRAWAL")   return TransactionType::Withdrawal;
        if (text == "TRANSFER_OUT") return TransactionType::TransferOut;
        if (text == "TRANSFER_IN")  return TransactionType::TransferIn;
        return TransactionType::Deposit;
    }
}

core::Expected<void, BankError> TransactionJournal::append(const Transaction& transaction) const {
    std::filesystem::create_directories(filePath_.parent_path());

    std::ofstream out(filePath_, std::ios::app);
    if (!out.is_open()) {
        return core::Unexpected(BankError{BankErrorCode::FileWriteFailure,
                                           "Could not open transactions.txt for appending."});
    }
    out << transaction.toRecordLine() << '\n';
    return {};
}

core::Expected<std::vector<Transaction>, BankError> TransactionJournal::loadAll() const {
    std::vector<Transaction> transactions;

    if (!std::filesystem::exists(filePath_)) {
        return transactions; // No file yet = empty history, not an error.
    }

    std::ifstream in(filePath_);
    if (!in.is_open()) {
        return core::Unexpected(BankError{BankErrorCode::FileReadFailure,
                                           "Could not open transactions.txt for reading."});
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> f = splitPipe(line);
        if (f.size() != 7) continue; // skip malformed lines defensively

        try {
            transactions.emplace_back(
                std::stol(f[0]),           // id
                std::stol(f[1]),           // accountNumber
                parseType(f[2]),           // type
                std::stod(f[3]),           // amount
                f[4],                      // timestamp
                std::stod(f[5]),           // balanceAfter
                std::stol(f[6])            // counterpartyAccount
            );
        } catch (const std::exception&) {
            continue; // skip malformed lines defensively
        }
    }
    return transactions;
}

} // namespace securebank::persistence