// FiaiSDK C++ example: RAII wrapper + network module.
//
//   fiaisdk_example_cpp [endpoint-url]
//
// Pass a listening TCP endpoint such as tcp://127.0.0.1:9545 to exercise the network.
#include <fiaisdk/cpp/crypto.hpp>
#include <fiaisdk/cpp/network.hpp>
#include <fiaisdk/cpp/wallet.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
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

        if (argc < 2) {
            return 0;
        }
        const fiaisdk::Network network(sdk);
        network.connect(argv[1], std::chrono::seconds(5));
        const auto status = network.status();
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
