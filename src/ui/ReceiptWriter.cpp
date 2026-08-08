/**
 * @file ReceiptWriter.cpp
 * @brief Implementation of post-transaction receipt file generation.
 *
 * Owned by: Ayesha Kamran
 */

#include "ui/ReceiptWriter.hpp"
#include "domain/Enums.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace securebank::ui {

namespace {

[[nodiscard]] std::string typeLabel(domain::TransactionType type) {
    switch (type) {
        case domain::TransactionType::Deposit:     return "Deposit";
        case domain::TransactionType::Withdrawal:  return "Withdrawal";
        case domain::TransactionType::TransferOut: return "Transfer (Sent)";
        case domain::TransactionType::TransferIn:  return "Transfer (Received)";
    }
    return "Transaction";
}

[[nodiscard]] std::string formatAmount(double amount) {
    std::ostringstream oss;
    oss << "Rs. " << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

[[nodiscard]] std::string accountTypeLabel(domain::AccountType type) {
    return (type == domain::AccountType::Savings) ? "Savings" : "Current";
}

} // anonymous namespace

void ReceiptWriter::write(const domain::Account& account,
                          const domain::Transaction& transaction) {
    std::filesystem::create_directories("receipts");

    std::ostringstream fileName;
    fileName << "receipts/"
             << account.accountNumber()
             << "_txn"
             << transaction.id()
             << ".txt";

    std::ofstream out(fileName.str());
    if (!out.is_open()) {
        return; // Non-fatal — ATM flow continues even if receipt cannot be written.
    }

    const std::string separator(50, '-');

    out << separator << "\n";
    out << "         SECUREBANK ATM RECEIPT\n";
    out << separator << "\n";
    out << "\n";
    out << "Transaction ID   : " << transaction.id() << "\n";
    out << "Date & Time      : " << transaction.timestamp() << "\n";
    out << "\n";
    out << separator << "\n";
    out << "  ACCOUNT DETAILS\n";
    out << separator << "\n";
    out << "Account Number   : " << account.accountNumber() << "\n";
    out << "Account Holder   : " << account.customerName() << "\n";
    out << "Account Type     : " << accountTypeLabel(account.type()) << "\n";
    out << "\n";
    out << separator << "\n";
    out << "  TRANSACTION DETAILS\n";
    out << separator << "\n";
    out << "Type             : " << typeLabel(transaction.type()) << "\n";
    out << "Amount           : " << formatAmount(transaction.amount()) << "\n";

    if (transaction.counterpartyAccount() != 0) {
        out << "Counterparty A/C : " << transaction.counterpartyAccount() << "\n";
    }

    out << "Balance After    : " << formatAmount(transaction.balanceAfter()) << "\n";
    out << "\n";
    out << separator << "\n";
    out << "   Thank you for banking with SecureBank.\n";
    out << separator << "\n";
}

} // namespace securebank::ui
