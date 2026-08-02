/**
 * @file Expected.hpp
 * @brief Minimal C++20-compatible substitute for std::expected (a C++23 feature).
 * @details Mirrors the subset of the std::expected API this project uses:
 * value(), error(), has_value(), and implicit bool conversion. This keeps
 * the entire codebase on strict C++20 as required, while preserving the
 * same error-handling style throughout.
 */

#pragma once
#include <variant>
#include <utility>
#include <stdexcept>
#include <optional>
#include <variant>
#include <utility>
#include <stdexcept>

namespace securebank::core {

/// Wraps an error value for construction of a failed Expected result.
template <typename E>
class Unexpected {
public:
    explicit Unexpected(E error) : error_(std::move(error)) {}
    [[nodiscard]] const E& error() const noexcept { return error_; }
    [[nodiscard]] E& error() noexcept { return error_; }
private:
    E error_;
};

/// Primary template: holds either a value of type T or an error of type E.
template <typename T, typename E>
class Expected {
public:
    Expected(T value) : data_(std::move(value)) {}
    Expected(Unexpected<E> unexpected) : data_(std::move(unexpected.error())) {}

    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(data_); }
    explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] const T& value() const {
        if (!has_value()) throw std::logic_error("Expected: no value present.");
        return std::get<T>(data_);
    }
    [[nodiscard]] T& value() {
        if (!has_value()) throw std::logic_error("Expected: no value present.");
        return std::get<T>(data_);
    }

    [[nodiscard]] const E& error() const {
        if (has_value()) throw std::logic_error("Expected: no error present.");
        return std::get<E>(data_);
    }

    [[nodiscard]] const T& operator*() const { return value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }

private:
    std::variant<T, E> data_;
};

/// Specialization for Expected<void, E> — success carries no value.
template <typename E>
class Expected<void, E> {
public:
    Expected() : error_(std::nullopt) {}
    Expected(Unexpected<E> unexpected) : error_(std::move(unexpected.error())) {}

    [[nodiscard]] bool has_value() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] const E& error() const {
        if (has_value()) throw std::logic_error("Expected: no error present.");
        return *error_;
    }

private:
    std::optional<E> error_;
};

} // namespace securebank::core