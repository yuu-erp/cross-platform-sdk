#pragma once

#include "core/domain/secure_bytes.h"
#include "modules/crypto/application/ports/i_crypto_provider.h"

#include <cstddef>

namespace fiaisdk::crypto {

class GenerateRandomBytes {
  public:
    static constexpr std::size_t kMaxSize = 65536;
    explicit GenerateRandomBytes(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<core::SecureBytes> execute(std::size_t size) const {
        if (size == 0 || size > kMaxSize) {
            return core::Error::invalidArgument("random byte count must be between 1 and 65536");
        }
        core::SecureBytes output(size);
        const auto result = provider_->privateRandom(output.mutableView());
        if (!result) {
            return result.error();
        }
        return output;
    }

  private:
    const ICryptoProvider *provider_;
};

} // namespace fiaisdk::crypto
