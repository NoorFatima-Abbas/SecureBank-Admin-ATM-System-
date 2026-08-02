/**
 * @file TransactionJournal.hpp
 * @brief Sole owner of transactions.txt persistence (append-only ledger).
 * @details Owned by Ayesha Kamran. Declared here only so BankSystem has a
 * complete dependency graph and compiles independently on both members' machines.
 */

#pragma once
#include <vector>
#include "core/Expected.hpp"
#include <filesystem>
#include "domain/Transaction.hpp"
#include "core/BankError.hpp"

namespace securebank::persistence {

class TransactionJournal {
public:
    explicit TransactionJournal(std::filesystem::path filePath);

    /// Appends a single transaction record to transactions.txt.
    [[nodiscard]] core::Expected<void, core::BankError>
        append(const domain::Transaction& transaction) const;

    /// Loads the full transaction history (used for admin ledger view / mini-statements).
    [[nodiscard]] core::Expected<std::vector<domain::Transaction>, core::BankError>
        loadAll() const;

private:
    std::filesystem::path filePath_;
};

} // namespace securebank::persistence