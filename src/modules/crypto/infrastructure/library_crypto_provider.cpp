#include "modules/crypto/infrastructure/library_crypto_provider.h"

#include <blst.h>
#include <cstddef>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <secp256k1.h>
#include <secp256k1_preallocated.h>
#include <secp256k1_recovery.h>
#include <string_view>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace fiaisdk::crypto {

namespace {

/// Calls can finish on host threads that outlive this SDK's private library context.
class ThreadCleanup {
  public:
    explicit ThreadCleanup(OSSL_LIB_CTX *context) noexcept : context_(context) {}
    ~ThreadCleanup() {
        OPENSSL_thread_stop_ex(context_);
    }
    ThreadCleanup(const ThreadCleanup &) = delete;
    ThreadCleanup &operator=(const ThreadCleanup &) = delete;
    ThreadCleanup(ThreadCleanup &&) = delete;
    ThreadCleanup &operator=(ThreadCleanup &&) = delete;

  private:
    OSSL_LIB_CTX *context_;
};

struct SecpContextDestroy {
    void operator()(secp256k1_context *context) const noexcept {
        secp256k1_context_preallocated_destroy(context);
    }
};

template <class T> struct Wiped {
    Wiped() = default;
    Wiped(const Wiped &) = delete;
    Wiped &operator=(const Wiped &) = delete;
    Wiped(Wiped &&) = delete;
    Wiped &operator=(Wiped &&) = delete;
    T value{};
    ~Wiped() {
        OPENSSL_cleanse(&value, sizeof(value));
    }
};
constexpr std::string_view kBlsDst = "BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_POP_";
const std::uint8_t *messageData(std::span<const std::uint8_t> bytes) {
    static constexpr std::uint8_t kEmpty = 0;
    return bytes.empty() ? &kEmpty : bytes.data();
}

} // namespace

struct LibraryCryptoProvider::Impl {
    std::unique_ptr<OSSL_LIB_CTX, decltype(&OSSL_LIB_CTX_free)> context{OSSL_LIB_CTX_new(),
                                                                        OSSL_LIB_CTX_free};
    std::unique_ptr<OSSL_PROVIDER, decltype(&OSSL_PROVIDER_unload)> provider{nullptr,
                                                                             OSSL_PROVIDER_unload};
    std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)> sha256{nullptr, EVP_MD_free};

    std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)> keccak{nullptr, EVP_MD_free};
    std::unique_ptr<EVP_CIPHER, decltype(&EVP_CIPHER_free)> aesGcm{nullptr, EVP_CIPHER_free};
    // Avoid libsecp256k1's allocating constructor (whose OOM callback aborts).
    std::vector<std::max_align_t> secpStorage =
        std::vector<std::max_align_t>((secp256k1_context_preallocated_size(SECP256K1_CONTEXT_NONE) +
                                       sizeof(std::max_align_t) - 1) /
                                      sizeof(std::max_align_t));
    std::unique_ptr<secp256k1_context, SecpContextDestroy> secp;

    Impl() {
        if (!context) {
            throw std::runtime_error("could not initialize crypto context");
        }
        const ThreadCleanup cleanup(context.get());
        secp.reset(
            secp256k1_context_preallocated_create(secpStorage.data(), SECP256K1_CONTEXT_NONE));
        provider.reset(OSSL_PROVIDER_load(context.get(), "default"));
        if (provider) {
            sha256.reset(EVP_MD_fetch(context.get(), "SHA256", nullptr));
            keccak.reset(EVP_MD_fetch(context.get(), "KECCAK-256", nullptr));
            aesGcm.reset(EVP_CIPHER_fetch(context.get(), "AES-256-GCM", nullptr));
        }
        if (!sha256 || !keccak || !aesGcm || !secp || EVP_MD_get_size(sha256.get()) != 32 ||
            EVP_MD_get_size(keccak.get()) != 32) {
            throw std::runtime_error("required crypto primitive is unavailable");
        }
        core::SecureBytes seed(32);
        if (RAND_priv_bytes_ex(context.get(), seed.mutableView().data(), seed.size(), 256) != 1 ||
            secp256k1_context_randomize(secp.get(), seed.view().data()) != 1) {
            throw std::runtime_error("could not initialize signing context");
        }
    }
};

LibraryCryptoProvider::LibraryCryptoProvider() : impl_(std::make_unique<Impl>()) {}
LibraryCryptoProvider::~LibraryCryptoProvider() = default;

core::Result<Sha256Digest>
LibraryCryptoProvider::sha256(std::span<const std::uint8_t> input) const {
    const ThreadCleanup cleanup(impl_->context.get());
    // EVP documents EVP_MAX_MD_SIZE as the maximum digest output buffer size.
    std::array<std::uint8_t, EVP_MAX_MD_SIZE> digest{};
    unsigned length = 0;
    const std::uint8_t empty = 0;
    const auto *data = input.empty() ? &empty : input.data();
    if (EVP_Digest(data, input.size(), digest.data(), &length, impl_->sha256.get(), nullptr) != 1 ||
        length != 32) {
        return core::Error::unavailable("SHA-256 operation failed");
    }
    Sha256Digest output{};
    std::copy_n(digest.begin(), output.size(), output.begin());
    return output;
}

core::Result<void> LibraryCryptoProvider::privateRandom(std::span<std::uint8_t> output) const {
    const ThreadCleanup cleanup(impl_->context.get());
    if (RAND_priv_bytes_ex(impl_->context.get(), output.data(), output.size(), 256) != 1) {
        return core::Error::unavailable("private random source is unavailable");
    }
    return {};
}

core::Result<Sha256Digest>
LibraryCryptoProvider::keccak256(std::span<const std::uint8_t> input) const {
    const ThreadCleanup cleanup(impl_->context.get());
    std::array<std::uint8_t, EVP_MAX_MD_SIZE> bytes{};
    unsigned length = 0;
    if (EVP_Digest(messageData(input), input.size(), bytes.data(), &length, impl_->keccak.get(),
                   nullptr) != 1 ||
        length != 32) {
        return core::Error::unavailable("Keccak-256 operation failed");
    }
    Sha256Digest output{};
    std::copy_n(bytes.begin(), output.size(), output.begin());
    return output;
}

core::Result<core::SecureBytes> LibraryCryptoProvider::generatePrivateKey(KeyScheme scheme) const {
    core::SecureBytes secret(32);
    if (scheme == KeyScheme::Bls12381) {
        core::SecureBytes ikm(32);
        const auto random = privateRandom(ikm.mutableView());
        if (!random) {
            return random.error();
        }
        Wiped<blst_scalar> scalar;
        blst_keygen(&scalar.value, ikm.view().data(), ikm.size(), nullptr, 0);
        if (!blst_sk_check(&scalar.value)) {
            return core::Error::unavailable("BLS key generation failed");
        }
        blst_bendian_from_scalar(secret.mutableView().data(), &scalar.value);
        return secret;
    }
    if (scheme == KeyScheme::Secp256k1) {
        for (unsigned attempt = 0; attempt < 128; ++attempt) {
            const auto random = privateRandom(secret.mutableView());
            if (!random) {
                return random.error();
            }
            if (secp256k1_ec_seckey_verify(impl_->secp.get(), secret.view().data()) == 1) {
                return secret;
            }
        }
        return core::Error::unavailable("secp256k1 key generation failed");
    }
    return core::Error::invalidArgument("unsupported key scheme");
}

core::Result<std::vector<std::uint8_t>>
LibraryCryptoProvider::derivePublicKey(KeyScheme scheme,
                                       std::span<const std::uint8_t> secret) const {
    if (secret.size() != 32) {
        return core::Error::invalidArgument("private key must be 32 bytes");
    }
    if (scheme == KeyScheme::Bls12381) {
        Wiped<blst_scalar> scalar;
        blst_scalar_from_bendian(&scalar.value, secret.data());
        if (!blst_sk_check(&scalar.value)) {
            return core::Error::invalidArgument("invalid BLS private key");
        }
        blst_p1 point{};
        blst_sk_to_pk_in_g1(&point, &scalar.value);
        std::vector<std::uint8_t> output(48);
        blst_p1_compress(output.data(), &point);
        return output;
    }
    if (scheme == KeyScheme::Secp256k1) {
        if (secp256k1_ec_seckey_verify(impl_->secp.get(), secret.data()) != 1) {
            return core::Error::invalidArgument("invalid secp256k1 private key");
        }
        secp256k1_pubkey point{};
        if (secp256k1_ec_pubkey_create(impl_->secp.get(), &point, secret.data()) != 1) {
            return core::Error::unavailable("public key derivation failed");
        }
        std::vector<std::uint8_t> output(65);
        std::size_t size = output.size();
        if (secp256k1_ec_pubkey_serialize(impl_->secp.get(), output.data(), &size, &point,
                                          SECP256K1_EC_UNCOMPRESSED) != 1 ||
            size != output.size()) {
            return core::Error::internal("public key serialization failed");
        }
        return output;
    }
    return core::Error::invalidArgument("unsupported key scheme");
}

core::Result<std::vector<std::uint8_t>>
LibraryCryptoProvider::sign(KeyScheme scheme, std::span<const std::uint8_t> secret,
                            std::span<const std::uint8_t> data) const {
    if (secret.size() != 32) {
        return core::Error::invalidArgument("private key must be 32 bytes");
    }
    if (scheme == KeyScheme::Bls12381) {
        Wiped<blst_scalar> scalar;
        blst_scalar_from_bendian(&scalar.value, secret.data());
        if (!blst_sk_check(&scalar.value)) {
            return core::Error::invalidArgument("invalid BLS private key");
        }
        blst_p2 hashed{};
        blst_p2 signedPoint{};
        blst_hash_to_g2(&hashed, messageData(data), data.size(),
                        // blst expects the bytes of the ASCII domain separation tag.
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                        reinterpret_cast<const std::uint8_t *>(kBlsDst.data()), kBlsDst.size(),
                        nullptr, 0);
        blst_sign_pk_in_g1(&signedPoint, &hashed, &scalar.value);
        std::vector<std::uint8_t> output(96);
        blst_p2_compress(output.data(), &signedPoint);
        return output;
    }
    if (scheme == KeyScheme::Secp256k1) {
        if (data.size() != 32 ||
            secp256k1_ec_seckey_verify(impl_->secp.get(), secret.data()) != 1) {
            return core::Error::invalidArgument("invalid secp256k1 key or digest length");
        }
        secp256k1_ecdsa_recoverable_signature signature{};
        if (secp256k1_ecdsa_sign_recoverable(impl_->secp.get(), &signature, data.data(),
                                             secret.data(), nullptr, nullptr) != 1) {
            return core::Error::unavailable("secp256k1 signing failed");
        }
        std::vector<std::uint8_t> output(65);
        int recovery = 0;
        if (secp256k1_ecdsa_recoverable_signature_serialize_compact(
                impl_->secp.get(), output.data(), &recovery, &signature) != 1) {
            return core::Error::internal("signature serialization failed");
        }
        output.back() = static_cast<std::uint8_t>(recovery);
        return output;
    }
    return core::Error::invalidArgument("unsupported key scheme");
}

core::Result<bool> LibraryCryptoProvider::verify(KeyScheme scheme,
                                                 std::span<const std::uint8_t> publicKey,
                                                 std::span<const std::uint8_t> data,
                                                 std::span<const std::uint8_t> signature) const {
    if (scheme == KeyScheme::Bls12381) {
        if (publicKey.size() != 48 || signature.size() != 96) {
            return core::Error::invalidArgument("invalid BLS public key or signature length");
        }
        blst_p1_affine pk{};
        blst_p2_affine sig{};
        if (blst_p1_uncompress(&pk, publicKey.data()) != BLST_SUCCESS ||
            blst_p2_uncompress(&sig, signature.data()) != BLST_SUCCESS ||
            !blst_p1_affine_in_g1(&pk) || blst_p1_affine_is_inf(&pk) ||
            !blst_p2_affine_in_g2(&sig) || blst_p2_affine_is_inf(&sig)) {
            return false;
        }
        return blst_core_verify_pk_in_g1(
                   &pk, &sig, true, messageData(data), data.size(),
                   // blst expects the bytes of the ASCII domain separation tag.
                   // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                   reinterpret_cast<const std::uint8_t *>(kBlsDst.data()), kBlsDst.size(), nullptr,
                   0) == BLST_SUCCESS;
    }
    if (scheme == KeyScheme::Secp256k1) {
        if (publicKey.size() != 65 || signature.size() != 65 || data.size() != 32) {
            return core::Error::invalidArgument("invalid secp256k1 input length");
        }
        if (signature.back() > 3 || publicKey.front() != 4) {
            return false;
        }
        secp256k1_pubkey pk{};
        secp256k1_pubkey recovered{};
        secp256k1_ecdsa_recoverable_signature sig{};
        secp256k1_ecdsa_signature normal{};
        if (secp256k1_ec_pubkey_parse(impl_->secp.get(), &pk, publicKey.data(), publicKey.size()) !=
                1 ||
            secp256k1_ecdsa_recoverable_signature_parse_compact(
                impl_->secp.get(), &sig, signature.data(), signature.back()) != 1 ||
            secp256k1_ecdsa_recoverable_signature_convert(impl_->secp.get(), &normal, &sig) != 1 ||
            secp256k1_ecdsa_verify(impl_->secp.get(), &normal, data.data(), &pk) != 1 ||
            secp256k1_ecdsa_recover(impl_->secp.get(), &recovered, &sig, data.data()) != 1) {
            return false;
        }
        std::array<std::uint8_t, 65> bytes{};
        std::size_t size = bytes.size();
        if (secp256k1_ec_pubkey_serialize(impl_->secp.get(), bytes.data(), &size, &recovered,
                                          SECP256K1_EC_UNCOMPRESSED) != 1) {
            return false;
        }
        return std::equal(bytes.begin(), bytes.end(), publicKey.begin());
    }
    return core::Error::invalidArgument("unsupported key scheme");
}

namespace {

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

enum class GcmResult : std::uint8_t { Ok, Failed, TagMismatch };

// One GCM pass. Sizes are bounded by SealData/OpenSealedData (<= 1 MiB), so int casts are safe.
GcmResult runGcm(const EVP_CIPHER *cipher, bool encrypt, std::span<const std::uint8_t> key,
                 std::span<const std::uint8_t> nonce, std::span<const std::uint8_t> aad,
                 std::span<const std::uint8_t> input, std::span<std::uint8_t> output,
                 std::span<std::uint8_t> tag) {
    const CipherContext ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int written = 0;
    if (!ctx ||
        EVP_CipherInit_ex(ctx.get(), cipher, nullptr, key.data(), nonce.data(), encrypt ? 1 : 0) !=
            1 ||
        (!aad.empty() && EVP_CipherUpdate(ctx.get(), nullptr, &written, aad.data(),
                                          static_cast<int>(aad.size())) != 1)) {
        return GcmResult::Failed;
    }
    if (!input.empty() && (EVP_CipherUpdate(ctx.get(), output.data(), &written, input.data(),
                                            static_cast<int>(input.size())) != 1 ||
                           static_cast<std::size_t>(written) != input.size())) {
        return GcmResult::Failed;
    }
    if (!encrypt && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG,
                                        static_cast<int>(tag.size()), tag.data()) != 1) {
        return GcmResult::Failed;
    }
    std::array<std::uint8_t, EVP_MAX_BLOCK_LENGTH> final{};
    const bool finished = EVP_CipherFinal_ex(ctx.get(), final.data(), &written) == 1;
    core::secureZero(final);
    if (!finished) {
        // For decryption this is the authentication tag check.
        return encrypt ? GcmResult::Failed : GcmResult::TagMismatch;
    }
    if (written != 0 ||
        (encrypt && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG,
                                        static_cast<int>(tag.size()), tag.data()) != 1)) {
        return GcmResult::Failed;
    }
    return GcmResult::Ok;
}

} // namespace

core::Result<std::vector<std::uint8_t>>
LibraryCryptoProvider::aeadSeal(std::span<const std::uint8_t, kAeadKeySize> key,
                                std::span<const std::uint8_t> aad,
                                std::span<const std::uint8_t> plaintext) const {
    const ThreadCleanup cleanup(impl_->context.get());
    std::vector<std::uint8_t> sealed(plaintext.size() + kAeadOverhead);
    const auto bytes = std::span(sealed);
    const auto nonce = bytes.first<kAeadNonceSize>();
    if (RAND_bytes_ex(impl_->context.get(), nonce.data(), nonce.size(), 256) != 1) {
        return core::Error::unavailable("random nonce source is unavailable");
    }
    if (runGcm(impl_->aesGcm.get(), true, key, nonce, aad, plaintext,
               bytes.subspan(kAeadNonceSize, plaintext.size()),
               bytes.last<kAeadTagSize>()) != GcmResult::Ok) {
        return core::Error::unavailable("AES-256-GCM encryption failed");
    }
    return sealed;
}

core::Result<core::SecureBytes>
LibraryCryptoProvider::aeadOpen(std::span<const std::uint8_t, kAeadKeySize> key,
                                std::span<const std::uint8_t> aad,
                                std::span<const std::uint8_t> sealed) const {
    if (sealed.size() < kAeadOverhead) {
        return core::Error::invalidArgument("invalid AEAD sealed size");
    }
    const ThreadCleanup cleanup(impl_->context.get());
    // OpenSSL takes a non-const tag pointer for EVP_CTRL_AEAD_SET_TAG; copy it.
    std::array<std::uint8_t, kAeadTagSize> tag{};
    std::ranges::copy(sealed.last<kAeadTagSize>(), tag.begin());
    const auto ciphertext = sealed.subspan(kAeadNonceSize, sealed.size() - kAeadOverhead);
    core::SecureBytes plaintext(ciphertext.size());
    const auto result = runGcm(impl_->aesGcm.get(), false, key, sealed.first<kAeadNonceSize>(), aad,
                               ciphertext, plaintext.mutableView(), tag);
    if (result == GcmResult::TagMismatch) {
        return core::Error{core::ErrorCode::AuthenticationFailed, "AEAD authentication failed"};
    }
    if (result != GcmResult::Ok) {
        return core::Error::unavailable("AES-256-GCM decryption failed");
    }
    return plaintext;
}

} // namespace fiaisdk::crypto
