/**
 * @file Transaction.hpp
 * @brief Immutable record of a single compleetd financial event
 * @details Once constructed, a transaction cannot be modified-every field 
 * is const and set only at construction, modeling the real-eorld rule that ledger entries are 
 * historical facts, never edited after the fact
 */

 #pragma once
 #include <string>
 #include <string_view>
 #include "domain/Enums.hpp"

 namespace securebank::domain{
    class Transaction{
        public:
        /**
         * @param transactionId  Unique, monotonically increasing ID
         * @param accountNumber  Account this entry belongs to
         * @param type           Deposit/Wuthfrawal/TransferIn/TransferOut
         * @param amount         Transaction amount(always positive)
         * @param timestamp      Formatted date-time string (e.g, "2026-08-01 14:30:00")
         * @param balanceAfter   Account balance immediately afetr the transaction
         * @param counterpartyAccount  For transfers: the other account involved, 0 otherwise
         */

         Transaction(long transactionId, 
        long accountNumber,
        TransactionType type,
        double amount,
        std::string timestamp,
        double balanceAfter,
        long counterpartyAccount=0
    );

    [[nodiscard]] long id() const noexcept;
    [[nodiscard]] long accountNumber() const noexcept;
    [[nodiscard]] TransactionType type() const noexcept;
    [[nodiscard]] double amount() const noexcept;
    [[nodiscard]] std::string_view timestamp() const noexcept;
    [[nodiscard]] double balanceAfter() const noexcept;
    [[nodiscard]] long counterpartyAccount() const noexcept;

    ///serializes this entry into the pipe-delimited transaction.txt line format
    [[nodiscard]] std::string toRecordLine() const;

private:
    long id_;
    long accountNumber_;
    TransactionType type_;
    double amount_;
    std::string timestamp_;
    double balanceAfter_;
    long counterpartyAccount_;
    };
 }//namespace securebank::domain