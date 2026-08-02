/**@file Enums.hpp
 * @brief strongly-typed domain enumerations for secure bank
 * @details Scoped enums (enum class) prevent implicit iint conversion
 * ans accidental comparison between unrelated states - e.g, an AccountStatus 
 * can never be silentl cmpared againsta TransactionType.
 */
#pragma once

namespace securebank::domain{
    /// type of bank account held by customer
    enum class AccountType{Savings, Current};

    ///lifecycle status of an account
    enum class AccountStatus{Active, Locked, Closed};

    ///Category of recorded financial transaction
    enum class TransactionType{Deposit, Withdrawal, TransferOut, TransferIn};
}//namepsace securebank::domain