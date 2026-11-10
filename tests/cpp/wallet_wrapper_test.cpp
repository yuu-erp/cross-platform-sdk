#include <array>
#include <fiaisdk/cpp/crypto.hpp>
#include <fiaisdk/cpp/wallet.hpp>
#include <gtest/gtest.h>
#include <utility>

TEST(WalletWrapper, CreateImportSignVerifyAndMove) {
    const fiaisdk::Sdk sdk;
    const fiaisdk::Crypto crypto(sdk);
    std::array<std::uint8_t, 32> one{};
    one.back() = 1;
    auto wallet = fiaisdk::Wallet::import(sdk, one.data(), one.size(), one.data(), one.size());
    EXPECT_EQ(wallet.address(), "7e5f4552091a69125d5dfcb7b8c2659029395bdf");
    EXPECT_EQ(wallet.blsAddress().size(), 40U);
    auto info = wallet.info();
    auto signature = wallet.signBls(std::string_view("a\0b", 3));
    const std::array<std::uint8_t, 3> data{'a', 0, 'b'};
    EXPECT_TRUE(crypto.verify(FIAISDK_CRYPTO_SCHEME_BLS12381, info.bls_public_key, 48, data.data(),
                              data.size(), signature.data(), signature.size()));
    const auto digest = crypto.keccak256("public demo");
    auto secpSignature = wallet.signDigest(digest);
    EXPECT_TRUE(crypto.verify(FIAISDK_CRYPTO_SCHEME_SECP256K1, info.secp256k1_public_key, 65,
                              digest.data(), digest.size(), secpSignature.data(),
                              secpSignature.size()));
    fiaisdk::Wallet moved(std::move(wallet));
    EXPECT_THROW((void)wallet.info(), fiaisdk::Error);
    EXPECT_THROW((void)wallet.signDigest(digest), fiaisdk::Error);
    EXPECT_EQ(moved.address(), "7e5f4552091a69125d5dfcb7b8c2659029395bdf");
    fiaisdk::Wallet created(sdk);
    created = std::move(moved);
    EXPECT_EQ(created.address(), "7e5f4552091a69125d5dfcb7b8c2659029395bdf");
    EXPECT_THROW((void)fiaisdk::Wallet::import(sdk, nullptr, 0, one.data(), 32), fiaisdk::Error);
}
