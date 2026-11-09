#include "core/domain/result.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace fiaisdk::core {
namespace {

TEST(Result, HoldsValue) {
    const Result<int> result = 42;
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.value(), 42);
}

TEST(Result, HoldsError) {
    const Result<int> result = Error::invalidArgument("bad");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(result.error().message, "bad");
}

TEST(Result, MoveOnlyValueCanBeMovedOut) {
    Result<std::unique_ptr<int>> result = std::make_unique<int>(7);
    const std::unique_ptr<int> value = std::move(result).value();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 7);
}

TEST(Result, VoidDefaultIsSuccessAndErrorIsFailure) {
    const Result<void> ok;
    EXPECT_TRUE(ok.hasValue());
    const Result<void> failed = Error::internal("x");
    EXPECT_FALSE(failed);
    EXPECT_EQ(failed.error().code, ErrorCode::Internal);
}

TEST(Result, WrongAccessorThrows) {
    const Result<int> value = 1;
    EXPECT_THROW((void)value.error(), std::bad_variant_access);
    const Result<int> error = Error::internal("x");
    EXPECT_THROW((void)error.value(), std::bad_variant_access);
    const Result<void> ok;
    EXPECT_THROW((void)ok.error(), std::bad_optional_access);
}

} // namespace
} // namespace fiaisdk::core
