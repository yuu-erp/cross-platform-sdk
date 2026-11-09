#include "core/domain/secure_bytes.h"

#include <atomic>
#include <utility>

namespace fiaisdk::core {

void secureZero(std::span<std::uint8_t> bytes) noexcept {
    for (auto &byte : bytes) {
        volatile std::uint8_t *target = &byte; // volatile store: cannot be elided
        *target = 0;
    }
    std::atomic_signal_fence(std::memory_order_seq_cst); // keep the stores before release
}

bool constantTimeEquals(std::span<const std::uint8_t> lhs,
                        std::span<const std::uint8_t> rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    unsigned difference = 0;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        difference |= static_cast<unsigned>(lhs[i] ^ rhs[i]);
    }
    return difference == 0;
}

SecureBytes::SecureBytes(std::size_t size) : bytes_(size, std::uint8_t{0}) {}

SecureBytes::SecureBytes(std::span<const std::uint8_t> bytes)
    : bytes_(bytes.begin(), bytes.end()) {}

SecureBytes::~SecureBytes() {
    wipe();
}

SecureBytes::SecureBytes(SecureBytes &&other) noexcept : bytes_(std::move(other.bytes_)) {
    other.bytes_.clear(); // moved-from vector is valid-but-unspecified; make it empty
}

SecureBytes &SecureBytes::operator=(SecureBytes &&other) noexcept {
    if (this != &other) {
        wipe();
        bytes_ = std::move(other.bytes_);
        other.bytes_.clear();
    }
    return *this;
}

SecureBytes SecureBytes::clone() const {
    return SecureBytes(view());
}

void SecureBytes::wipe() noexcept {
    secureZero(bytes_);
    bytes_.clear();
    bytes_.shrink_to_fit();
}

} // namespace fiaisdk::core
