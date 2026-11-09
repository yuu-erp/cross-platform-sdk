#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace fiaisdk::core {

/// Distinct type over a primitive so ids, amounts and addresses cannot be mixed up.
///
///     struct WalletIdTag;
///     using WalletId = core::StrongType<std::string, WalletIdTag>;
///
/// Validation belongs in a factory of the owning module (e.g. `Address::parse`), not here.
template <class T, class Tag> class StrongType {
  public:
    using ValueType = T;

    constexpr explicit StrongType(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : value_(std::move(value)) {}

    [[nodiscard]] constexpr const T &value() const & noexcept {
        return value_;
    }
    [[nodiscard]] constexpr T &&value() && noexcept {
        return std::move(value_);
    }

    friend constexpr bool operator==(const StrongType &, const StrongType &) = default;
    friend constexpr auto operator<=>(const StrongType &, const StrongType &) = default;

  private:
    T value_;
};

} // namespace fiaisdk::core

template <class T, class Tag>
    requires requires(const T &v) { std::hash<T>{}(v); }
struct std::hash<fiaisdk::core::StrongType<T, Tag>> {
    std::size_t operator()(const fiaisdk::core::StrongType<T, Tag> &v) const noexcept {
        return std::hash<T>{}(v.value());
    }
};
