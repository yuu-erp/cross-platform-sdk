#include "core/domain/strong_type.h"

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <unordered_set>

namespace fiaisdk::core {
namespace {

struct WalletIdTag;
struct AccountIdTag;
using WalletId = StrongType<std::string, WalletIdTag>;
using AccountId = StrongType<std::string, AccountIdTag>;

static_assert(!std::is_convertible_v<std::string, WalletId>, "construction must be explicit");
static_assert(!std::is_convertible_v<WalletId, AccountId>, "different tags must not mix");
static_assert(!std::is_constructible_v<AccountId, WalletId>, "different tags must not mix");

TEST(StrongType, ComparesByValue) {
    EXPECT_EQ(WalletId("a"), WalletId("a"));
    EXPECT_NE(WalletId("a"), WalletId("b"));
    EXPECT_LT(WalletId("a"), WalletId("b"));
}

TEST(StrongType, ExposesValue) {
    const WalletId id("w-1");
    EXPECT_EQ(id.value(), "w-1");
}

TEST(StrongType, IsHashable) {
    const std::unordered_set<WalletId> ids{WalletId("a"), WalletId("b"), WalletId("a")};
    EXPECT_EQ(ids.size(), 2U);
    EXPECT_EQ(ids.count(WalletId("b")), 1U);
}

} // namespace
} // namespace fiaisdk::core
