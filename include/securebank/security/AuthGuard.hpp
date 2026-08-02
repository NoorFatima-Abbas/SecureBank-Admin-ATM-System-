/**
 * @file AuthGuard.hpp
 * @brief Bonus Feature: Automatic Account Locking.
 * @details Encapsulates the PIN-attempt/lockout policy separately from
 * Account itself. Account only stores counters; AuthGuard owns the *rule*
 * (3 wrong attempts → lock). Changing the threshold means editing one
 * constructor default here — nothing else in the system changes.
 */

#pragma once
#include <string_view>
#include "domain/Account.hpp"

namespace securebank::security {

enum class AuthOutcome { Granted, DeniedWrongPin, DeniedAccountLocked, NowLocked };

class AuthGuard {
public:
    explicit AuthGuard(int maxAttempts = 3) noexcept;

    /// Evaluates a login attempt against an account and mutates its lock state accordingly.
    [[nodiscard]] AuthOutcome attempt(domain::Account& account, std::string_view hashedPinAttempt) const;

private:
    int maxAttempts_;
};

} // namespace securebank::security