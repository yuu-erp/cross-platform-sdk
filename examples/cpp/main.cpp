// FiaiSDK C++ example: RAII wrapper + network module.
//
//   fiaisdk_example_cpp [endpoint-url]
//
// NOTE: the network transport is simulated in this version (no real I/O).
#include <fiaisdk/cpp/crypto.hpp>
#include <fiaisdk/cpp/network.hpp>
#include <fiaisdk/cpp/wallet.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    const std::string url = argc > 1 ? argv[1] : "https://node.example.com/rpc";
    try {
        const fiaisdk::Sdk sdk({"cpp-example", "1.0.0"});
        std::cout << "FiaiSDK " << fiaisdk::runtimeVersion() << '\n';

        const fiaisdk::Crypto crypto(sdk);
        const auto digest = crypto.sha256("abc");
        std::cout << "SHA-256 of demo message calculated (" << digest.size() << " bytes)\n";

        const fiaisdk::Wallet wallet(sdk);
        const auto walletInfo = wallet.info();
        const auto messageDigest = crypto.keccak256("public demo message");
        const auto signature = wallet.signDigest(messageDigest);
        if (!crypto.verify(FIAISDK_CRYPTO_SCHEME_SECP256K1, walletInfo.secp256k1_public_key,
                           sizeof(walletInfo.secp256k1_public_key), messageDigest.data(),
                           messageDigest.size(), signature.data(), signature.size())) {
            std::cerr << "Offline signature verification failed\n";
            return 1;
        }
        std::cout << "Offline secp256k1 signature verified\n";

        const fiaisdk::Network network(sdk);
        network.connect(url, std::chrono::seconds(5));
        const auto status = network.status();
        // Demo only: a real app must not print/log the full URL (may contain an API key).
        std::cout << "connected: " << network.endpoint() << '\n'
                  << "latency: " << status.latency.count() << " ms\n";
        network.disconnect();
        std::cout << "disconnected\n";
    } catch (const fiaisdk::Error &e) {
        std::cerr << "SDK error " << e.status() << ": " << e.what() << '\n';
        return 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
