/**
 * @file AdminConsole.hpp
 * @brief Admin-facing console UI. Presentation only — all logic delegates
 * to BankSystem. This class never touches files or vectors directly.
 */

#pragma once
#include "core/BankSystem.hpp"

namespace securebank::ui {

class AdminConsole {
public:
    explicit AdminConsole(core::BankSystem& bankSystem) noexcept;

    /// Runs the admin menu loop until the admin chooses to exit.
    void run();

private:
    core::BankSystem& bankSystem_;

    void showMenu() const;
    void handleCreateAccount();
    void handleViewAllAccounts() const;
    void handleDeleteAccount();
    void handleUnlockAccount();
    void handleSearchAccount() const;
};

} // namespace securebank::ui