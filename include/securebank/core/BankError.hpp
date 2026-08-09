/**
 * @file BankError.hpp
 * @brief Centralized error taxonomy used with std::expected across secureBank
 * @details Every fallible opeartion in the system returns
 * std::expected<T, BankError> instead of throwing for expected business 
 * failures(e.g, insufficent funds).
 * Exceptions are reserved for truly exceptional/programmer-error conditions only.
 */
 #pragma once
 #include <string>

 namespace securebank::core{

 ///Enumerates every recognizzed failure category in the system
 enum class BankErrorCode{
    AccountNotFound,
    AccountAlreadyExists,
    AccountLocked,
    AccountClosed,
    InvalidPin, 
    InsufficientFunds,
    InvalidAmount,
    DailyLimitExceeded,
    OtpRequired,
    OtpMismatch,
    FileReadFailure,
    FileWriteFailure,
    ValidationFailure
 };

///Pairs an error code with a human-readable message for console display
struct BankError{
    BankErrorCode code;
    std::string message;
};

 }///namespace securebank::core