#include <fiaisdk/cpp/sdk.hpp>
#include <fiaisdk/fiaisdk.h>

int main() {
    const fiaisdk::Sdk sdk({"package-test", "1.0.0"});
    return sdk.hasModule("wallet") && fiaisdk::runtimeVersion() == FIAISDK_VERSION_STRING ? 0 : 1;
}
