#include "modules/wallet/application/import_wallet.h"
#include <algorithm>
#include <utility>

namespace fiaisdk::wallet {

core::Result<std::unique_ptr<Wallet>>
ImportWallet::execute(std::span<const std::uint8_t> bls, std::span<const std::uint8_t> secp) const {
    if (bls.size() != 32 || secp.size() != 32) {
        return core::Error::invalidArgument("both wallet private keys must be 32 bytes");
    }
    core::SecureBytes blsCopy(bls);
    core::SecureBytes secpCopy(secp);
    auto blsPublic = crypto_->derivePublicKey(crypto::KeyScheme::Bls12381, blsCopy.view());
    if (!blsPublic) {
        return blsPublic.error();
    }
    auto secpPublic = crypto_->derivePublicKey(crypto::KeyScheme::Secp256k1, secpCopy.view());
    if (!secpPublic) {
        return secpPublic.error();
    }
    if (blsPublic.value().size() != 48 || secpPublic.value().size() != 65 ||
        secpPublic.value().front() != 4) {
        return core::Error::internal("provider returned invalid public key sizes");
    }
    auto blsHash = crypto_->keccak256(blsPublic.value());
    if (!blsHash) {
        return blsHash.error();
    }
    auto secpHash =
        crypto_->keccak256(std::span<const std::uint8_t>(secpPublic.value()).subspan(1));
    if (!secpHash) {
        return secpHash.error();
    }
    WalletInfo info;
    std::copy(blsPublic.value().begin(), blsPublic.value().end(), info.blsPublicKey.begin());
    std::copy(secpPublic.value().begin(), secpPublic.value().end(), info.secpPublicKey.begin());
    std::ranges::copy(std::span(blsHash.value()).last<20>(), info.blsAddress.begin());
    std::ranges::copy(std::span(secpHash.value()).last<20>(), info.address.begin());
    return std::make_unique<Wallet>(std::move(blsCopy), std::move(secpCopy), info);
}

} // namespace fiaisdk::wallet
