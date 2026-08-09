/**
 * @file ReceiptWriter.hpp
 * @brief Bonus Feature: Receipt File Generation.
 * @details After every successful ATM transaction, writes a formatted text
 * receipt to the receipts/ directory. The file name encodes the account
 * number and transaction ID so receipts never overwrite each other.
 *
 * Owned by: Ayesha Kamran
 */

#pragma once
#include "domain/Account.hpp"
#include "domain/Transaction.hpp"

namespace securebank::ui {

class ReceiptWriter {
public:
    /// Writes a formatted receipt file for the given account and transaction.
    /// Output path: receipts/<accountNumber>_txn<transactionId>.txt
    /// Silently no-ops if the file cannot be created (non-fatal for the ATM flow).
    static void write(const domain::Account& account,
                      const domain::Transaction& transaction);
};

} // namespace securebank::ui
