/**
 * @file AccountLedgerStore.hpp
 * @brief Sole owner of accounts.txt persistence.
 * @details No other class in the system opens accounts.txt directly.
 * BankSystem delegates all load/save operations here. Writes are atomic:
 * data is written to a temp file first, then renamed, so a crash mid-save
 * can never leave accounts.txt corrupted or half-written.
 */

#pragma once
#include <vector>
#include "core/Expected.hpp"
#include <filesystem>
#include "domain/Account.hpp"
#include "core/BankError.hpp"

namespace securebank::persistence {

class AccountLedgerStore {
public:
    explicit AccountLedgerStore(std::filesystem::path filePath);

    /// Loads all accounts from disk. Returns an empty vector if the file does not yet exist.
    [[nodiscard]] core::Expected<std::vector<domain::Account>, core::BankError> loadAll() const;

    /// Atomically overwrites accounts.txt with the given in-memory account set.
    [[nodiscard]] core::Expected<void, core::BankError>
        persistAll(const std::vector<domain::Account>& accounts) const;

private:
    std::filesystem::path filePath_;

    [[nodiscard]] core::Expected<domain::Account, core::BankError>
        parseLine(std::string_view line) const;
};

} // namespace securebank::persistence