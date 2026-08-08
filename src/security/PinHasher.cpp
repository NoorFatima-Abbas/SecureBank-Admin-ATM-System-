/**
 * @file PinHasher.cpp
 * @brief Implementation of the lightweight PIN hashing utility.
 */

#include "security/PinHasher.hpp"
#include <functional>
#include <sstream>

namespace securebank::security {

std::string PinHasher::hash(std::string_view rawPin) {
    static constexpr std::string_view salt = "SecureBank_NUST_2026";
    const std::string salted = std::string(rawPin) + std::string(salt);
    const std::size_t hashed = std::hash<std::string>{}(salted);

    std::ostringstream oss;
    oss << std::hex << hashed;
    return oss.str();
}

} // namespace securebank::security