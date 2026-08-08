/**
 * @file OtpService.cpp
 * @brief Implementation of the OTP generation and verification service.
 *
 * Owned by: Ayesha Kamran
 */

#include "ui/OtpService.hpp"
#include <random>
#include <string>

namespace securebank::ui {

std::string OtpService::generate() {
    std::random_device rd;
    std::mt19937 engine(rd());
    std::uniform_int_distribution<int> dist(100000, 999999);
    currentOtp_ = std::to_string(dist(engine));
    return currentOtp_;
}

bool OtpService::verify(const std::string& enteredCode) const noexcept {
    return !currentOtp_.empty() && enteredCode == currentOtp_;
}

} // namespace securebank::ui
