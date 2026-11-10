#include <fiaisdk/cpp/crypto.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string_view>
#include <utility>

TEST(CryptoWrapper, BinaryInputAndRandomOutput) {
    const fiaisdk::Sdk sdk;
    const fiaisdk::Crypto crypto(sdk);
    const std::array<std::uint8_t, 3> bytes{'a', 0, 'b'};
    EXPECT_EQ(crypto.sha256(bytes.data(), bytes.size()),
              crypto.sha256(std::string_view("a\0b", 3)));
    EXPECT_NE(crypto.sha256("a"), crypto.sha256(std::string_view("a\0b", 3)));
    std::array<std::uint8_t, 32> output{};
    EXPECT_NO_THROW(crypto.randomBytes(output.data(), output.size()));
    EXPECT_THROW(crypto.randomBytes(output.data(), 0), fiaisdk::Error);
}

TEST(CryptoWrapper, MovedFromSdkIsRejected) {
    fiaisdk::Sdk source;
    const fiaisdk::Sdk target(std::move(source));
    const fiaisdk::Crypto invalid(source);
    EXPECT_THROW((void)invalid.sha256("abc"), fiaisdk::Error);
}
