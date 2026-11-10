#include <fiaisdk/cpp/crypto.hpp>
#include <fiaisdk/cpp/wallet.hpp>
#include <fiaisdk/fiaisdk.h>

int main() {
    const fiaisdk::Sdk sdk({"package-test", "1.0.0"});
    const fiaisdk::Crypto crypto(sdk);
    const auto digest = crypto.sha256("abc");
    const fiaisdk::Wallet wallet(sdk);
    const auto info = wallet.info();
    const auto signature = wallet.signDigest(digest);
    if (!crypto.verify(FIAISDK_CRYPTO_SCHEME_SECP256K1, info.secp256k1_public_key, 65,
                       digest.data(), digest.size(), signature.data(), signature.size()))
        return 1;
    return sdk.hasModule("crypto") && sdk.hasModule("wallet") && digest[0] == 0xBA &&
                   fiaisdk::runtimeVersion() == FIAISDK_VERSION_STRING
               ? 0
               : 1;
}
