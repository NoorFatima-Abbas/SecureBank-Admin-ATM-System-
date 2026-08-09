/**
 * @file PinHasher.hpp
 * @brief Lightweight PIN hashing so raw PINs are never stored or compared directly.
 * @details Not cryptographically secure (no external crypto library used, per
 * project scope) — but ensures plaintext PINs never touch disk or memory
 * comparisons, which is the intended teaching point for this bonus-adjacent design.
 */

#pragma once
#include <string>
#include <string_view>

namespace securebank::security {

class PinHasher {
public:
    /// Produces a deterministic hash string for a raw PIN.
    [[nodiscard]] static std::string hash(std::string_view rawPin);
};

} // namespace securebank::security