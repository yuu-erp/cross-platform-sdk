#pragma once

#include "core/domain/error.h"

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace fiaisdk::core {

/// Value-or-error return type used between layers and modules (no exceptions across them).
/// Minimal stand-in for C++23 std::expected; replace when the toolchain baseline allows.
/// Calling value() on an error (or error() on a value) is a programming error and throws
/// std::bad_variant_access / std::bad_optional_access.
template <class T> class [[nodiscard]] Result {
    static_assert(!std::is_same_v<std::remove_cv_t<T>, Error>, "Result<Error> is ambiguous");

  public:
    Result(T value)
        : data_(std::in_place_index<0>, std::move(value)) {} // NOLINT(google-explicit-constructor)
    Result(Error error)
        : data_(std::in_place_index<1>, std::move(error)) {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool hasValue() const noexcept {
        return data_.index() == 0;
    }
    explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] T &value() & {
        return std::get<0>(data_);
    }
    [[nodiscard]] const T &value() const & {
        return std::get<0>(data_);
    }
    /// Returns by value so `for (auto& x : f().value())` cannot dangle.
    [[nodiscard]] T value() && {
        return std::get<0>(std::move(data_));
    }

    [[nodiscard]] const Error &error() const & {
        return std::get<1>(data_);
    }

  private:
    std::variant<T, Error> data_;
};

template <> class [[nodiscard]] Result<void> {
  public:
    Result() = default;
    Result(Error error) : error_(std::move(error)) {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool hasValue() const noexcept {
        return !error_.has_value();
    }
    explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] const Error &error() const & {
        if (!error_) {
            throw std::bad_optional_access{};
        }
        return *error_;
    }

  private:
    std::optional<Error> error_;
};

} // namespace fiaisdk::core
