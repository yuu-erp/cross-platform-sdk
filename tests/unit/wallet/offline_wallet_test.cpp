#include "bindings/c_api/handles.h"
#include "core/infrastructure/in_process_event_bus.h"
#include "core/infrastructure/null_logger.h"
#include "core/infrastructure/system_clock.h"
#include "modules/crypto/infrastructure/library_crypto_provider.h"
#include "modules/wallet/api/wallet_facade.h"
#include "modules/wallet/api/wallet_module.h"
#include "support/crypto/unavailable_crypto_provider.h"
#include <algorithm>
#include <array>
#include <fiaisdk/wallet.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fiaisdk::wallet {
namespace {
std::vector<std::uint8_t> bytes(std::string_view text) {
    std::vector<std::uint8_t> output;
    auto digit = [](char c) -> unsigned {
        return c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'a' + 10);
    };
    for (std::size_t i = 0; i < text.size(); i += 2)
        output.push_back(static_cast<std::uint8_t>((digit(text[i]) << 4) | digit(text[i + 1])));
    return output;
}

class OfflineWallet : public ::testing::Test {
  protected:
    crypto::LibraryCryptoProvider provider_;
    crypto::CryptoFacade crypto_{provider_};
    WalletFacade wallet_{crypto_};
    std::array<std::uint8_t, 32> one_ = [] {
        std::array<std::uint8_t, 32> key{};
        key.back() = 1;
        return key;
    }();
};

TEST(WalletModuleTest, RequiresInitializedCryptoFacade) {
    core::SystemClock clock;
    core::NullLogger logger;
    core::InProcessEventBus events(logger);
    const core::ModuleContext context(clock, logger, events);
    crypto::CryptoModule crypto(std::make_unique<test::UnavailableCryptoProvider>());
    WalletModule wallet(crypto);

    const auto early = wallet.initialize(context);
    ASSERT_FALSE(early);
    EXPECT_EQ(early.error().code, core::ErrorCode::NotInitialized);
    EXPECT_EQ(wallet.facade(), nullptr);

    ASSERT_TRUE(crypto.initialize(context));
    ASSERT_TRUE(wallet.initialize(context));
    EXPECT_NE(wallet.facade(), nullptr);
    wallet.shutdown();
    EXPECT_EQ(wallet.facade(), nullptr);
    crypto.shutdown();
}

TEST_F(OfflineWallet, ScalarOnePublicKeysAndEvmAddress) {
    auto imported = wallet_.import(one_, one_);
    ASSERT_TRUE(imported);
    const auto &info = imported.value()->info();
    const auto generator = bytes("97f1d3a73197d7942695638c4fa9ac0fc3688c4f9774b905a14e3a3f171bac586"
                                 "c55e83ff97a1aeffb3af00adb22c6bb");
    EXPECT_TRUE(std::equal(generator.begin(), generator.end(), info.blsPublicKey.begin()));
    const auto evmAddress = bytes("7e5f4552091a69125d5dfcb7b8c2659029395bdf");
    EXPECT_TRUE(std::equal(evmAddress.begin(), evmAddress.end(), info.address.begin()));
    EXPECT_EQ(info.secpPublicKey[0], 4);
    auto hash = crypto_.keccak256(info.blsPublicKey);
    ASSERT_TRUE(hash);
    EXPECT_TRUE(std::equal(hash.value().begin() + 12, hash.value().end(), info.blsAddress.begin()));
}

TEST_F(OfflineWallet, PublicKeccakVectorsAreNotSha3) {
    auto empty = crypto_.keccak256({});
    ASSERT_TRUE(empty);
    const auto expected = bytes("c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), empty.value().begin()));
    const std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    auto digest = crypto_.keccak256(abc);
    ASSERT_TRUE(digest);
    const auto abcExpected =
        bytes("4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45");
    EXPECT_TRUE(std::equal(abcExpected.begin(), abcExpected.end(), digest.value().begin()));
}

TEST_F(OfflineWallet, EthereumBlsReferenceSignature) {
    // Public CC0 fixture sign_case_7055381f640f2c1d, ethereum/bls12-381-tests v0.1.2.
    const auto key = bytes("328388aff0d4a5b7dc9205abd374e7e98f3cd9f3418edb4eafda5fb16473d216");
    const std::array<std::uint8_t, 32> message{};
    const auto expected = bytes("948a7cb99f76d616c2c564ce9bf4a519f1bea6b0a624a02276443c245854219fab"
                                "b8d4ce061d255af5330b078d5380681751aa7053da2c98bae898edc218c75f07e2"
                                "4d8802a17cd1f6833b71e58f5eb5b94208b4d0bb3848cecb075ea21be115");
    auto signedData = crypto_.sign(crypto::KeyScheme::Bls12381, key, message);
    ASSERT_TRUE(signedData);
    EXPECT_EQ(signedData.value(), expected);
    auto pk = crypto_.derivePublicKey(crypto::KeyScheme::Bls12381, key);
    ASSERT_TRUE(pk);
    auto verified = crypto_.verify(crypto::KeyScheme::Bls12381, pk.value(), message, expected);
    ASSERT_TRUE(verified);
    EXPECT_TRUE(verified.value());
}

TEST_F(OfflineWallet, BothSigningSchemesAndRecoveryIdAreBound) {
    auto imported = wallet_.import(one_, one_);
    ASSERT_TRUE(imported);
    const std::array<std::uint8_t, 32> digest{};
    for (auto scheme : {crypto::KeyScheme::Bls12381, crypto::KeyScheme::Secp256k1}) {
        auto signature = wallet_.sign(*imported.value(), scheme, digest);
        ASSERT_TRUE(signature);
        const auto &info = imported.value()->info();
        const std::span<const std::uint8_t> pk =
            scheme == crypto::KeyScheme::Bls12381
                ? std::span<const std::uint8_t>(info.blsPublicKey)
                : std::span<const std::uint8_t>(info.secpPublicKey);
        auto valid = crypto_.verify(scheme, pk, digest, signature.value());
        ASSERT_TRUE(valid);
        EXPECT_TRUE(valid.value());
        auto wrongMessage = digest;
        wrongMessage[0] = 1;
        auto wrong = crypto_.verify(scheme, pk, wrongMessage, signature.value());
        ASSERT_TRUE(wrong);
        EXPECT_FALSE(wrong.value());
        auto repeated = wallet_.sign(*imported.value(), scheme, digest);
        ASSERT_TRUE(repeated);
        EXPECT_EQ(repeated.value(), signature.value());
        if (scheme == crypto::KeyScheme::Secp256k1)
            signature.value()[64] ^= 1;
        else
            signature.value()[0] ^= 1;
        auto corrupted = crypto_.verify(scheme, pk, digest, signature.value());
        ASSERT_TRUE(corrupted);
        EXPECT_FALSE(corrupted.value());
    }
}

TEST_F(OfflineWallet, RejectsZeroOrdersAndWrongSizesWithoutClamping) {
    const std::array<std::uint8_t, 32> zero{};
    EXPECT_FALSE(wallet_.import(zero, one_));
    EXPECT_FALSE(wallet_.import(one_, zero));
    const auto blsOrder = bytes("73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001");
    const auto secpOrder =
        bytes("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");
    EXPECT_FALSE(wallet_.import(blsOrder, one_));
    EXPECT_FALSE(wallet_.import(one_, secpOrder));
    EXPECT_FALSE(wallet_.import(std::span<const std::uint8_t>(one_).first(31), one_));
    auto imported = wallet_.import(one_, one_);
    ASSERT_TRUE(imported);
    EXPECT_FALSE(wallet_.sign(*imported.value(), crypto::KeyScheme::Secp256k1, {}));
    EXPECT_FALSE(wallet_.sign(*imported.value(), static_cast<crypto::KeyScheme>(999), one_));
}

TEST_F(OfflineWallet, BlsInfinityAndMalformedPointsAreRejected) {
    std::array<std::uint8_t, 48> publicKey{};
    std::array<std::uint8_t, 96> signature{};
    publicKey[0] = 0xC0;
    signature[0] = 0xC0;
    auto result = crypto_.verify(crypto::KeyScheme::Bls12381, publicKey, {}, signature);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value());
    publicKey.fill(0xFF);
    signature.fill(0xFF);
    result = crypto_.verify(crypto::KeyScheme::Bls12381, publicKey, {}, signature);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value());
}

TEST_F(OfflineWallet, NewWalletCanSignWithBothIndependentlyGeneratedKeys) {
    auto created = wallet_.create();
    ASSERT_TRUE(created);
    for (auto scheme : {crypto::KeyScheme::Bls12381, crypto::KeyScheme::Secp256k1}) {
        auto signature = wallet_.sign(*created.value(), scheme, one_);
        ASSERT_TRUE(signature);
        const auto &info = created.value()->info();
        const auto pk = scheme == crypto::KeyScheme::Bls12381
                            ? std::span<const std::uint8_t>(info.blsPublicKey)
                            : std::span<const std::uint8_t>(info.secpPublicKey);
        auto verified = crypto_.verify(scheme, pk, one_, signature.value());
        ASSERT_TRUE(verified);
        EXPECT_TRUE(verified.value());
    }
}

TEST(OfflineWalletFailures, ProviderFailuresAndExceptionsReachAbiSafely) {
    test::UnavailableCryptoProvider provider;
    crypto::CryptoFacade crypto(provider);
    WalletFacade wallet(crypto);
    EXPECT_FALSE(wallet.create());
    const std::array<std::uint8_t, 32> key{};
    EXPECT_FALSE(wallet.import(key, key));
    fiaisdk_sdk sdk;
    sdk.apis.wallet = &wallet;
    fiaisdk_wallet *handle = nullptr;
    EXPECT_EQ(fiaisdk_wallet_create(&sdk, &handle), FIAISDK_STATUS_UNAVAILABLE);
    EXPECT_EQ(handle, nullptr);
}

} // namespace
} // namespace fiaisdk::wallet
