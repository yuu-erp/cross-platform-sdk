#include "kernel/result.h"

#include "test_check.h"

#include <memory>
#include <string>
#include <utility>

using fiaisdk::kernel::Error;
using fiaisdk::kernel::ErrorCode;
using fiaisdk::kernel::Result;

namespace {

void resultWithValue_isTruthy_andHoldsValue() {
    Result<int> r = 42;
    CHECK(r.hasValue());
    CHECK(static_cast<bool>(r));
    CHECK(r.value() == 42);
}

void resultWithError_isFalsy_andHoldsError() {
    Result<int> r = Error{ErrorCode::InvalidArgument, "bad"};
    CHECK(!r);
    CHECK(r.error().code == ErrorCode::InvalidArgument);
    CHECK(r.error().message == "bad");
}

void resultMoveOnlyValue_canBeMovedOut() {
    Result<std::unique_ptr<int>> r = std::make_unique<int>(7);
    std::unique_ptr<int> p = std::move(r).value();
    CHECK(p != nullptr);
    CHECK(*p == 7);
}

void resultVoid_defaultIsSuccess_errorIsFailure() {
    Result<void> ok;
    CHECK(ok.hasValue());
    Result<void> err = Error{ErrorCode::Internal, "x"};
    CHECK(!err);
    CHECK(err.error().code == ErrorCode::Internal);
}

} // namespace

int main() {
    resultWithValue_isTruthy_andHoldsValue();
    resultWithError_isFalsy_andHoldsError();
    resultMoveOnlyValue_canBeMovedOut();
    resultVoid_defaultIsSuccess_errorIsFailure();
    return TEST_RESULT();
}
