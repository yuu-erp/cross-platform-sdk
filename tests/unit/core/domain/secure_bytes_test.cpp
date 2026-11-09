#include "core/domain/secure_bytes.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace fiaisdk::core {
namespace {

static_assert(!std::is_copy_constructible_v<SecureBytes>, "secrets must not be copied implicitly");
static_assert(!std::is_copy_assignable_v<SecureBytes>, "secrets must not be copied implicitly");
static_assert(std::is_nothrow_move_constructible_v<SecureBytes>);

TEST(SecureBytes, SizedConstructorZeroFills) {
    const SecureBytes bytes(4);
    ASSERT_EQ(bytes.size(), 4U);
    for (const auto byte : bytes.view()) {
        EXPECT_EQ(byte, 0U);
    }
}

TEST(SecureBytes, CopiesFromSpan) {
    const std::array<std::uint8_t, 3> source{1, 2, 3};
    const SecureBytes bytes{std::span<const std::uint8_t>(source)};
    EXPECT_TRUE(constantTimeEquals(bytes.view(), source));
}

TEST(SecureBytes, MoveLeavesSourceEmpty) {
    const std::array<std::uint8_t, 2> source{9, 9};
    SecureBytes original{std::span<const std::uint8_t>(source)};
    const SecureBytes moved(std::move(original));
    EXPECT_EQ(moved.size(), 2U);
    EXPECT_TRUE(original.empty()); // NOLINT(bugprone-use-after-move): verifying moved-from state
}

TEST(SecureBytes, CloneIsIndependentCopy) {
    const std::array<std::uint8_t, 2> source{4, 5};
    SecureBytes original{std::span<const std::uint8_t>(source)};
    const SecureBytes copy = original.clone();
    original.wipe();
    EXPECT_TRUE(original.empty());
    EXPECT_TRUE(constantTimeEquals(copy.view(), source));
}

TEST(SecureBytes, WipeIsIdempotent) {
    SecureBytes bytes(8);
    bytes.wipe();
    bytes.wipe();
    EXPECT_TRUE(bytes.empty());
}

TEST(SecureZero, OverwritesEveryByte) {
    std::array<std::uint8_t, 5> buffer{1, 2, 3, 4, 5};
    secureZero(buffer);
    for (const auto byte : buffer) {
        EXPECT_EQ(byte, 0U);
    }
}

TEST(ConstantTimeEquals, ComparesContentAndLength) {
    const std::array<std::uint8_t, 3> a{1, 2, 3};
    const std::array<std::uint8_t, 3> b{1, 2, 3};
    const std::array<std::uint8_t, 3> c{1, 2, 4};
    const std::array<std::uint8_t, 2> shorter{1, 2};
    EXPECT_TRUE(constantTimeEquals(a, b));
    EXPECT_FALSE(constantTimeEquals(a, c));
    EXPECT_FALSE(constantTimeEquals(a, shorter));
    EXPECT_TRUE(constantTimeEquals({}, {}));
}

} // namespace
} // namespace fiaisdk::core
