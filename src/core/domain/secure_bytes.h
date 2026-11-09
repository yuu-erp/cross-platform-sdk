#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace fiaisdk::core {

/// Owning buffer for secrets (private keys, seeds, tokens).
///
/// - Zeroed when destroyed, moved-from or wipe()d.
/// - Fixed size after construction: the storage never reallocates, so no stray copies are left
///   behind in freed memory.
/// - Not copyable; use clone() when a second copy is really needed.
/// - No conversion to std::string and no stream operator, so it cannot be logged by accident.
///
/// This does not lock memory against swapping; that is an OS concern for the platform layer.
class SecureBytes {
  public:
    SecureBytes() = default;
    /// size zero-initialised bytes.
    explicit SecureBytes(std::size_t size);
    /// Copies `bytes`. The caller remains responsible for wiping its own source buffer.
    explicit SecureBytes(std::span<const std::uint8_t> bytes);

    ~SecureBytes();

    SecureBytes(const SecureBytes &) = delete;
    SecureBytes &operator=(const SecureBytes &) = delete;
    SecureBytes(SecureBytes &&other) noexcept;
    SecureBytes &operator=(SecureBytes &&other) noexcept;

    [[nodiscard]] SecureBytes clone() const;

    [[nodiscard]] std::span<const std::uint8_t> view() const noexcept {
        return bytes_;
    }
    [[nodiscard]] std::span<std::uint8_t> mutableView() noexcept {
        return bytes_;
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return bytes_.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return bytes_.empty();
    }

    /// Zero the contents and release the storage. Idempotent.
    void wipe() noexcept;

  private:
    std::vector<std::uint8_t> bytes_;
};

/// Overwrite `bytes` with zeros in a way the optimiser may not remove.
void secureZero(std::span<std::uint8_t> bytes) noexcept;

/// Compare two byte sequences in time that depends only on their length (not their contents).
/// Use for MACs, signatures, tokens. Different lengths return false immediately.
/// Portable best effort: once the crypto library ADR lands, delegate to its audited routine
/// (e.g. sodium_memcmp / CRYPTO_memcmp).
[[nodiscard]] bool constantTimeEquals(std::span<const std::uint8_t> lhs,
                                      std::span<const std::uint8_t> rhs) noexcept;

} // namespace fiaisdk::core
