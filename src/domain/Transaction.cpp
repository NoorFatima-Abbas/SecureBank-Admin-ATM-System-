/**
 * @file Transaction.cpp
 * @brief Implementation of Transaction's accessors and serialization.
 */

#include "domain/Transaction.hpp"
#include <sstream>

namespace securebank::domain {

namespace {
    /// Converts a TransactionType to its stored string form.
    [[nodiscard]] std::string typeToString(TransactionType type) {
        switch (type) {
            case TransactionType::Deposit:     return "DEPOSIT";
            case TransactionType::Withdrawal:  return "WITHDRAWAL";
            case TransactionType::TransferOut: return "TRANSFER_OUT";
            case TransactionType::TransferIn:  return "TRANSFER_IN";
        }
        return "UNKNOWN";
    }
}

Transaction::Transaction(long transactionId,
                          long accountNumber,
                          TransactionType type,
                          double amount,
                          std::string timestamp,
                          double balanceAfter,
                          long counterpartyAccount)
    : id_(transactionId),
      accountNumber_(accountNumber),
      type_(type),
      amount_(amount),
      timestamp_(std::move(timestamp)),
      balanceAfter_(balanceAfter),
      counterpartyAccount_(counterpartyAccount) {}

long Transaction::id() const noexcept {
     return id_; 
    }
long Transaction::accountNumber() const noexcept { 
    return accountNumber_; 
}
TransactionType Transaction::type() const noexcept {
     return type_;
     }
double Transaction::amount() const noexcept {
     return amount_; 
    }
std::string_view Transaction::timestamp() const noexcept { 
    return timestamp_; 
}
double Transaction::balanceAfter() const noexcept { 
    return balanceAfter_;
 }
long Transaction::counterpartyAccount() const noexcept { 
    return counterpartyAccount_; 
}

std::string Transaction::toRecordLine() const {
    std::ostringstream oss;
    oss << id_ << '|'
        << accountNumber_ << '|'
        << typeToString(type_) << '|'
        << amount_ << '|'
        << timestamp_ << '|'
        << balanceAfter_ << '|'
        << counterpartyAccount_;
    return oss.str();
}

} // namespace securebank::domain