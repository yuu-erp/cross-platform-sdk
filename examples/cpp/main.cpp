#include <fiaisdk/cpp/sdk.hpp>

#include <exception>
#include <iostream>

int main() {
    try {
        const fiaisdk::Sdk sdk({"cpp-example", "1.0.0"});
        std::cout << "FiaiSDK " << fiaisdk::runtimeVersion() << '\n';
        std::cout << "smc module: " << (sdk.hasModule("smc") ? "yes" : "no") << '\n';
    } catch (const fiaisdk::Error &e) {
        std::cerr << "SDK error " << e.status() << ": " << e.what() << '\n';
        return 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
