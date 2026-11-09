// Tests the header-only C++ wrapper exactly as a consumer would use it (built as C++17).
#include <fiaisdk/cpp/sdk.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

TEST(SdkWrapper, ConstructsAndReportsModules) {
    const fiaisdk::Sdk sdk({"cpp-test", "1.0.0"});
    EXPECT_NE(sdk.native(), nullptr);
    EXPECT_TRUE(sdk.hasModule("wallet"));
    EXPECT_TRUE(sdk.hasModule("smc"));
    EXPECT_TRUE(sdk.hasModule("network"));
    EXPECT_FALSE(sdk.hasModule("nope"));
}

TEST(SdkWrapper, DefaultConstructionIsCopyListInitializable) {
    const fiaisdk::Sdk sdk = {};
    EXPECT_NE(sdk.native(), nullptr);
}

TEST(SdkWrapper, MoveTransfersOwnership) {
    fiaisdk::Sdk a;
    fiaisdk_sdk *raw = a.native();
    fiaisdk::Sdk b(std::move(a));
    EXPECT_EQ(b.native(), raw);
    EXPECT_EQ(a.native(), nullptr); // NOLINT(bugprone-use-after-move): verifying moved-from state

    fiaisdk::Sdk c;
    c = std::move(b);
    EXPECT_EQ(c.native(), raw);
}

TEST(SdkWrapper, MovedFromHasModuleThrowsInvalidArgument) {
    fiaisdk::Sdk a;
    const fiaisdk::Sdk b(std::move(a));
    try {
        (void)a.hasModule("wallet"); // NOLINT(bugprone-use-after-move)
        FAIL() << "expected fiaisdk::Error";
    } catch (const fiaisdk::Error &e) {
        EXPECT_EQ(e.status(), FIAISDK_STATUS_INVALID_ARGUMENT);
    }
}

TEST(SdkWrapper, HasModuleIsSafeConcurrently) {
    const fiaisdk::Sdk sdk;
    std::atomic<int> wrong{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 1000; ++i) {
                if (!sdk.hasModule("wallet") || sdk.hasModule("nope")) {
                    ++wrong;
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(wrong.load(), 0);
}

TEST(SdkWrapper, RuntimeVersionMatchesHeader) {
    EXPECT_EQ(fiaisdk::runtimeVersion(), FIAISDK_VERSION_STRING);
}

} // namespace
