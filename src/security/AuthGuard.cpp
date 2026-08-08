/**
 * @file AuthGuard.cpp
 * @brief Implementation of the automatic account-locking policy.
 */

#include "security/AuthGuard.hpp"

namespace securebank::security {

using domain::Account;
using domain::AccountStatus;

AuthGuard::AuthGuard(int maxAttempts) noexcept : maxAttempts_(maxAttempts) {}

AuthOutcome AuthGuard::attempt(Account& account, std::string_view hashedPinAttempt) const {
    if (account.status() == AccountStatus::Locked) {
        return AuthOutcome::DeniedAccountLocked;
    }

    if (account.verifyPin(hashedPinAttempt)) {
        account.resetPinAttempts();
        return AuthOutcome::Granted;
    }

    account.registerFailedPinAttempt();

    if (account.failedPinAttempts() >= maxAttempts_) {
        account.lock();
        return AuthOutcome::NowLocked;
    }

    return AuthOutcome::DeniedWrongPin;
}

} // namespace securebank::security