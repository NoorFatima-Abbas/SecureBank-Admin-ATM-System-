/**
 * @file FieldValidator.hpp
 * @brief Foolproof console input validation — never crashes, never infinite-loops
 * on bad input, always clears the buffer.
 * @details Shared by both AdminConsole and AtmConsole. All methods are static
 * and stateless; each returns std::expected so calling code decides how to
 * react (retry prompt, abort transaction, etc.) rather than validator deciding.
 */

#pragma once
#include <string>
#include <string_view>
#include "core/Expected.hpp"
#include "core/BankError.hpp"

namespace securebank::validation {

class FieldValidator {
public:
    /// Prompts repeatedly (up to maxRetries) until a valid positive amount is entered.
    [[nodiscard]] static core::Expected<double, core::BankError>
        readPositiveAmount(std::string_view prompt, int maxRetries = 3);

    /// Prompts repeatedly until a valid 4-digit numeric PIN is entered (visible input).
    [[nodiscard]] static core::Expected<std::string, core::BankError>
        readNumericPin(std::string_view prompt, int maxRetries = 3);

    /// Prompts repeatedly until a valid positive account number is entered.
    [[nodiscard]] static core::Expected<long, core::BankError>
        readAccountNumber(std::string_view prompt, int maxRetries = 3);

    /// Prompts for a non-empty name (letters/spaces only).
    [[nodiscard]] static core::Expected<std::string, core::BankError>
        readNonEmptyName(std::string_view prompt, int maxRetries = 3);

    /// Clears cin's fail state and discards the rest of the current input line.
    static void clearInputBuffer() noexcept;
    /// Prompts repeatedly until a valid 13-digit numeric CNIC is entered.
[[nodiscard]] static core::Expected<std::string, core::BankError>
    readCnic(std::string_view prompt, int maxRetries = 3);

private:
    [[nodiscard]] static bool isAllDigits(std::string_view text) noexcept;
    [[nodiscard]] static bool isAlphaSpace(std::string_view text) noexcept;
};

} // namespace securebank::validation