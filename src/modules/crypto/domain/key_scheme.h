#pragma once

#include <cstdint>

namespace fiaisdk::crypto {

enum class KeyScheme : std::int32_t { Bls12381 = 1, Secp256k1 = 2 };

} // namespace fiaisdk::crypto
