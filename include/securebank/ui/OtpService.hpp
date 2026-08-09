/**
 * @file OtpService.hpp
 * @brief Bonus Feature: OTP Verification for Large Transfers.
 * @details Generates a random 6-digit one-time password whenever a
 * withdrawal or transfer exceeds Rs. 50,000, and verifies the code
 * re-entered by the customer before the transaction is allowed to proceed.
 *
 * Owned by: Ayesha Kamran
 */

#pragma once
#include <string>

namespace securebank::ui {

class OtpService {
public:
    /// Amount threshold above which OTP verification is required (Rs. 50,000).
    static constexpr double OtpThreshold = 50000.0;

    /// Generates a fresh 6-digit OTP, stores it internally, and returns it
    /// so the caller can display/simulate delivery to the customer.
    [[nodiscard]] std::string generate();

    /// Returns true if the provided code matches the most recently generated OTP.
    [[nodiscard]] bool verify(const std::string& enteredCode) const noexcept;

private:
    std::string currentOtp_;
};

} // namespace securebank::ui
