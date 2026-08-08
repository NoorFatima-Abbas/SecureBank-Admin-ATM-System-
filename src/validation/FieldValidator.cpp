/**
 * @file FieldValidator.cpp
 * @brief Implementation of the console input validation layer.
 */

#include "validation/FieldValidator.hpp"
#include <iostream>
#include <limits>
#include <cctype>

namespace securebank::validation {

using core::BankError;
using core::BankErrorCode;

void FieldValidator::clearInputBuffer() noexcept {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool FieldValidator::isAllDigits(std::string_view text) noexcept {
    if (text.empty()) return false;
    for (const char ch : text) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    }
    return true;
}

bool FieldValidator::isAlphaSpace(std::string_view text) noexcept {
    if (text.empty()) return false;
    for (const char ch : text) {
        if (!std::isalpha(static_cast<unsigned char>(ch)) && ch != ' ') return false;
    }
    return true;
}

core::Expected<double, BankError>
FieldValidator::readPositiveAmount(std::string_view prompt, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        std::cout << prompt;
        double value{};
        if (std::cin >> value) {
            clearInputBuffer();
            if (value > 0.0) return value;
            std::cout << "Amount must be greater than zero. Try again.\n";
        } else {
            clearInputBuffer();
            std::cout << "Invalid input — numbers only. Try again.\n";
        }
    }
    return core::Unexpected(BankError{BankErrorCode::ValidationFailure,
                                      "Too many invalid amount attempts."});
}

core::Expected<std::string, BankError>
FieldValidator::readNumericPin(std::string_view prompt, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        std::cout << prompt;
        std::string pin;
        std::cin >> pin;
        clearInputBuffer();
        if (pin.size() == 4 && isAllDigits(pin)) {
            return pin;
        }
        std::cout << "PIN must be exactly 4 digits. Try again.\n";
    }
    return core::Unexpected(BankError{BankErrorCode::ValidationFailure,
                                      "Too many invalid PIN format attempts."});
}

core::Expected<long, BankError>
FieldValidator::readAccountNumber(std::string_view prompt, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        std::cout << prompt;
        long value{};
        if (std::cin >> value) {
            clearInputBuffer();
            if (value > 0) return value;
            std::cout << "Account number must be positive. Try again.\n";
        } else {
            clearInputBuffer();
            std::cout << "Invalid input — numbers only. Try again.\n";
        }
    }
    return core::Unexpected(BankError{BankErrorCode::ValidationFailure,
                                      "Too many invalid account number attempts."});
}

core::Expected<std::string, BankError>
FieldValidator::readNonEmptyName(std::string_view prompt, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        std::cout << prompt;
        std::string name;
        std::getline(std::cin, name);
        if (isAlphaSpace(name)) {
            return name;
        }
        std::cout << "Name must contain letters/spaces only and cannot be empty. Try again.\n";
    }
    return core::Unexpected(BankError{BankErrorCode::ValidationFailure,
                                      "Too many invalid name attempts."});
}

core::Expected<std::string, BankError>
FieldValidator::readCnic(std::string_view prompt, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        std::cout << prompt;
        std::string cnic;
        std::getline(std::cin, cnic);
        if (cnic.size() == 13 && isAllDigits(cnic)) {
            return cnic;
        }
        std::cout << "CNIC must be exactly 13 digits (no dashes). Try again.\n";
    }
    return core::Unexpected(BankError{BankErrorCode::ValidationFailure,
                                       "Too many invalid CNIC attempts."});
}

} // namespace securebank::validation