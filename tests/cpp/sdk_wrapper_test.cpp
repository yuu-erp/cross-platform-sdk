// Tests the header-only C++ wrapper exactly as a consumer would use it.
#include <fiaisdk/cpp/sdk.hpp>

#include "test_check.h"

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void sdk_constructs_andReportsModules() {
    fiaisdk::Sdk sdk({"cpp-test", "1.0.0"});
    CHECK(sdk.native() != nullptr);
    CHECK(sdk.hasModule("wallet"));
    CHECK(!sdk.hasModule("nope"));
}

void sdk_move_transfersOwnership() {
    fiaisdk::Sdk a;
    fiaisdk_sdk *raw = a.native();
    fiaisdk::Sdk b(std::move(a));
    CHECK(b.native() == raw);
    CHECK(a.native() == nullptr); // NOLINT(bugprone-use-after-move): verifying moved-from state

    fiaisdk::Sdk c;
    c = std::move(b);
    CHECK(c.native() == raw);
}

void movedFrom_hasModule_throwsInvalidArgument() {
    fiaisdk::Sdk a;
    fiaisdk::Sdk b(std::move(a));
    bool threw = false;
    try {
        (void)a.hasModule("wallet"); // NOLINT(bugprone-use-after-move)
    } catch (const fiaisdk::Error &e) {
        threw = e.status() == FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    CHECK(threw);
}

void defaultConstruction_isCopyListInitializable() {
    fiaisdk::Sdk sdk = {};
    CHECK(sdk.native() != nullptr);
}

void hasModule_isSafeConcurrently() {
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
    CHECK(wrong.load() == 0);
}

void runtimeVersion_matchesHeader() {
    CHECK(fiaisdk::runtimeVersion() == FIAISDK_VERSION_STRING);
}

} // namespace

int main() {
    sdk_constructs_andReportsModules();
    sdk_move_transfersOwnership();
    movedFrom_hasModule_throwsInvalidArgument();
    defaultConstruction_isCopyListInitializable();
    hasModule_isSafeConcurrently();
    runtimeVersion_matchesHeader();
    return TEST_RESULT();
}
