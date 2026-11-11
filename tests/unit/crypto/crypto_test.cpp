#include "bindings/c_api/handles.h"
#include "core/infrastructure/in_process_event_bus.h"
#include "core/infrastructure/null_logger.h"
#include "core/infrastructure/system_clock.h"
#include "modules/crypto/api/crypto_module.h"
#include "modules/crypto/infrastructure/library_crypto_provider.h"
#include "support/crypto/unavailable_crypto_provider.h"
#include <fiaisdk/crypto.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <future>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fiaisdk::crypto {
namespace {

std::string hex(const Sha256Digest &digest) {
    constexpr char kDigits[] = "0123456789abcdef";
    std::string output;
    for (auto byte : digest) {
        output.push_back(kDigits[byte >> 4]);
        output.push_back(kDigits[byte & 15]);
    }
    return output;
}

TEST(CryptoProvider, PublishedSha256Vectors) {
    LibraryCryptoProvider provider;
    const CryptoFacade crypto(provider);
    auto empty = crypto.sha256({});
    ASSERT_TRUE(empty);
    EXPECT_EQ(hex(empty.value()),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    auto digest = crypto.sha256(abc);
    ASSERT_TRUE(digest);
    EXPECT_EQ(hex(digest.value()),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const std::vector<std::uint8_t> million(1000000, 'a');
    auto large = crypto.sha256(million);
    ASSERT_TRUE(large);
    EXPECT_EQ(hex(large.value()),
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

class FailingProvider final : public test::UnavailableCryptoProvider {
  public:
    mutable unsigned calls = 0;
    core::Result<Sha256Digest> sha256(std::span<const std::uint8_t>) const override {
        ++calls;
        return core::Error::unavailable("provider unavailable");
    }
    core::Result<void> privateRandom(std::span<std::uint8_t> output) const override {
        ++calls;
        std::fill(output.begin(), output.end(), 0xAA);
        return core::Error::unavailable("provider unavailable");
    }
};

std::vector<std::uint8_t> fromHex(std::string_view text) {
    std::vector<std::uint8_t> output;
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        output.push_back(
            static_cast<std::uint8_t>(std::stoi(std::string(text.substr(i, 2)), nullptr, 16)));
    }
    return output;
}

TEST(CryptoAead, OpensPublishedAes256GcmVector) {
    // NIST GCM spec, test case 14: 256-bit zero key, zero IV, one zero block, no AAD.
    LibraryCryptoProvider provider;
    const CryptoFacade crypto(provider);
    const std::array<std::uint8_t, kAeadKeySize> key{};
    const auto sealed = fromHex("000000000000000000000000"
                                "cea7403d4d606b6e074ec5d3baf39d18"
                                "d0d1c8a799996bf0265b98b5d48ab919");
    auto opened = crypto.aeadOpen(key, {}, sealed);
    ASSERT_TRUE(opened);
    const auto plain = opened.value().view();
    ASSERT_EQ(plain.size(), 16U);
    EXPECT_TRUE(std::all_of(plain.begin(), plain.end(), [](auto byte) { return byte == 0; }));
}

TEST(CryptoAead, RoundTripsWithFreshNoncesAndRejectsTampering) {
    LibraryCryptoProvider provider;
    const CryptoFacade crypto(provider);
    std::array<std::uint8_t, kAeadKeySize> key{};
    key.fill(0x42);
    const std::array<std::uint8_t, 4> aad{1, 2, 3, 4};
    const std::array<std::uint8_t, 64> plain{7};
    auto first = crypto.aeadSeal(key, aad, plain);
    auto second = crypto.aeadSeal(key, aad, plain);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_EQ(first.value().size(), plain.size() + kAeadOverhead);
    EXPECT_FALSE(std::equal(first.value().begin(), first.value().begin() + kAeadNonceSize,
                            second.value().begin()));

    auto opened = crypto.aeadOpen(key, aad, first.value());
    ASSERT_TRUE(opened);
    EXPECT_TRUE(std::ranges::equal(opened.value().view(), plain));

    const auto rejected = [&](std::span<const std::uint8_t> withAad,
                              std::span<const std::uint8_t> sealed,
                              std::span<const std::uint8_t, kAeadKeySize> withKey) {
        auto result = crypto.aeadOpen(withKey, withAad, sealed);
        return !result && result.error().code == core::ErrorCode::AuthenticationFailed;
    };
    auto flipped = first.value();
    flipped[kAeadNonceSize] ^= 1U;
    EXPECT_TRUE(rejected(aad, flipped, key));
    flipped = first.value();
    flipped.back() ^= 1U;
    EXPECT_TRUE(rejected(aad, flipped, key));
    const std::array<std::uint8_t, 4> otherAad{1, 2, 3, 5};
    EXPECT_TRUE(rejected(otherAad, first.value(), key));
    auto otherKey = key;
    otherKey[0] ^= 1U;
    EXPECT_TRUE(rejected(aad, first.value(), otherKey));

    auto empty = crypto.aeadSeal(key, {}, {});
    ASSERT_TRUE(empty);
    ASSERT_EQ(empty.value().size(), kAeadOverhead);
    auto openedEmpty = crypto.aeadOpen(key, {}, empty.value());
    ASSERT_TRUE(openedEmpty);
    EXPECT_TRUE(openedEmpty.value().view().empty());

    const std::array<std::uint8_t, kAeadOverhead - 1> tooShort{};
    auto shortResult = crypto.aeadOpen(key, {}, tooShort);
    ASSERT_FALSE(shortResult);
    EXPECT_EQ(shortResult.error().code, core::ErrorCode::InvalidArgument);
}

TEST(CryptoUseCases, ValidateRandomSizeBeforeProviderAndPropagateFailure) {
    FailingProvider provider;
    CryptoFacade crypto(provider);
    EXPECT_FALSE(crypto.randomBytes(0));
    EXPECT_FALSE(crypto.randomBytes(GenerateRandomBytes::kMaxSize + 1));
    EXPECT_EQ(provider.calls, 0U);
    auto random = crypto.randomBytes(32);
    ASSERT_FALSE(random);
    EXPECT_EQ(random.error().code, core::ErrorCode::Unavailable);
    auto hash = crypto.sha256({});
    ASSERT_FALSE(hash);
    EXPECT_EQ(hash.error().code, core::ErrorCode::Unavailable);
    EXPECT_EQ(provider.calls, 2U);
}

TEST(CryptoProvider, ConcurrentHashAndRandomAndIndependentContexts) {
    LibraryCryptoProvider first;
    LibraryCryptoProvider second;
    std::vector<std::future<bool>> work;
    for (unsigned i = 0; i < 4; ++i) {
        work.push_back(std::async(std::launch::async, [&first, &second, i] {
            CryptoFacade crypto(i % 2 == 0 ? first : second);
            for (unsigned j = 0; j < 20; ++j) {
                auto digest = crypto.sha256({});
                auto bytes = crypto.randomBytes(32);
                if (!digest || !bytes || bytes.value().size() != 32 ||
                    hex(digest.value()) !=
                        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") {
                    return false;
                }
            }
            return true;
        }));
    }
    for (auto &result : work) {
        EXPECT_TRUE(result.get());
    }
    EXPECT_EQ(CryptoFacade(first).randomBytes(GenerateRandomBytes::kMaxSize).value().size(),
              GenerateRandomBytes::kMaxSize);
}

TEST(CryptoModule, LifecycleAndMissingProvider) {
    core::SystemClock clock;
    core::NullLogger logger;
    core::InProcessEventBus events(logger);
    const core::ModuleContext context(clock, logger, events);
    CryptoModule invalid(nullptr);
    EXPECT_FALSE(invalid.initialize(context));
    CryptoModule module(std::make_unique<LibraryCryptoProvider>());
    EXPECT_EQ(module.facade(), nullptr);
    ASSERT_TRUE(module.initialize(context));
    ASSERT_NE(module.facade(), nullptr);
    EXPECT_TRUE(module.facade()->sha256({}));
    module.shutdown();
    EXPECT_EQ(module.facade(), nullptr);
}

TEST(CryptoProvider, ProviderCanBeDestroyedBeforeCallingThreadExits) {
    auto provider = std::make_unique<LibraryCryptoProvider>();
    std::promise<bool> finished;
    std::promise<void> exit;
    auto canExit = exit.get_future();
    std::thread worker([&] {
        CryptoFacade crypto(*provider);
        auto hash = crypto.sha256({});
        auto random = crypto.randomBytes(32);
        finished.set_value(hash.hasValue() && random.hasValue());
        canExit.wait(); // remains alive while the SDK's context is freed
    });
    EXPECT_TRUE(finished.get_future().get());
    provider.reset();
    exit.set_value();
    worker.join();
}

TEST(CryptoBoundary, MissingModuleClearsOutputs) {
    fiaisdk_sdk sdk;
    std::array<std::uint8_t, 32> output;
    output.fill(0xAA);
    EXPECT_EQ(fiaisdk_crypto_sha256(&sdk, nullptr, 0, output.data(), output.size()),
              FIAISDK_STATUS_MODULE_NOT_FOUND);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0; }));
    output.fill(0xAA);
    EXPECT_EQ(fiaisdk_crypto_random_bytes(&sdk, output.data(), output.size()),
              FIAISDK_STATUS_MODULE_NOT_FOUND);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0; }));
}

TEST(CryptoBoundary, FailedAndThrowingProvidersDoNotExposePartialOutput) {
    FailingProvider provider;
    CryptoFacade crypto(provider);
    fiaisdk_sdk sdk;
    sdk.apis.crypto = &crypto;
    std::array<std::uint8_t, 32> output;
    output.fill(0xAA);
    EXPECT_EQ(fiaisdk_crypto_random_bytes(&sdk, output.data(), output.size()),
              FIAISDK_STATUS_UNAVAILABLE);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0; }));
    output.fill(0xAA);
    EXPECT_EQ(fiaisdk_crypto_sha256(&sdk, nullptr, 0, output.data(), output.size()),
              FIAISDK_STATUS_UNAVAILABLE);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0; }));

    class ThrowingProvider final : public test::UnavailableCryptoProvider {
      public:
        core::Result<Sha256Digest> sha256(std::span<const std::uint8_t>) const override {
            throw std::runtime_error("test provider");
        }
        core::Result<void> privateRandom(std::span<std::uint8_t> bytes) const override {
            std::fill(bytes.begin(), bytes.end(), 0xBB);
            throw std::runtime_error("test provider");
        }
    } throwing;
    CryptoFacade throwingCrypto(throwing);
    sdk.apis.crypto = &throwingCrypto;
    output.fill(0xAA);
    EXPECT_EQ(fiaisdk_crypto_random_bytes(&sdk, output.data(), output.size()),
              FIAISDK_STATUS_INTERNAL_ERROR);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0; }));
    output.fill(0xAA);
    EXPECT_EQ(fiaisdk_crypto_sha256(&sdk, nullptr, 0, output.data(), output.size()),
              FIAISDK_STATUS_INTERNAL_ERROR);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0; }));
}

} // namespace
} // namespace fiaisdk::crypto
