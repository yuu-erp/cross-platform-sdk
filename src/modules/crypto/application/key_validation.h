#pragma once
#include "modules/crypto/domain/key_scheme.h"
#include <cstddef>
namespace fiaisdk::crypto {
inline bool validScheme(KeyScheme scheme) noexcept {
    return scheme == KeyScheme::Bls12381 || scheme == KeyScheme::Secp256k1;
}
inline std::size_t publicKeySize(KeyScheme scheme) noexcept {
    return scheme == KeyScheme::Bls12381 ? 48 : 65;
}
inline std::size_t signatureSize(KeyScheme scheme) noexcept {
    return scheme == KeyScheme::Bls12381 ? 96 : 65;
}
inline bool validSigningData(KeyScheme scheme, std::size_t size) noexcept {
    return scheme == KeyScheme::Bls12381 || size == 32;
}
} // namespace fiaisdk::crypto
