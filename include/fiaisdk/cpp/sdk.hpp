// SPDX-License-Identifier: UNLICENSED
#pragma once

// Header-only C++17 wrapper over the FiaiSDK C ABI (ADR-0002).
// Nothing here is exported from the library; it compiles inside the consumer's binary, so it is
// safe across compilers and runtimes. Errors are reported as fiaisdk::Error exceptions.
//
// Note: fiaisdk::Error is an inline class. If your application is split into several shared
// libraries built with -fvisibility=hidden, catch it in the same library that called the SDK
// (or re-export it), otherwise typeinfo matching may fail across your own library boundaries.

#include <fiaisdk/sdk.h>
#include <fiaisdk/version.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fiaisdk {

class Error : public std::runtime_error {
  public:
    explicit Error(fiaisdk_status status)
        : std::runtime_error(fiaisdk_status_message(status)), status_(status) {}

    [[nodiscard]] fiaisdk_status status() const noexcept {
        return status_;
    }

  private:
    fiaisdk_status status_;
};

inline void throwIfError(fiaisdk_status status) {
    if (status != FIAISDK_STATUS_OK) {
        throw Error(status);
    }
}

struct Config {
    std::string applicationName;
    std::string applicationVersion;
};

/// Version of the loaded library (may differ from FIAISDK_VERSION_STRING the code was built with).
[[nodiscard]] inline std::string_view runtimeVersion() noexcept {
    return fiaisdk_version();
}

/// RAII owner of a fiaisdk_sdk handle. Move-only.
class Sdk {
  public:
    /// Creates an instance with an empty Config. Throws fiaisdk::Error.
    Sdk() : Sdk(Config{}) {}

    /// Throws fiaisdk::Error on failure.
    explicit Sdk(const Config &config) {
        fiaisdk_sdk_config raw = FIAISDK_SDK_CONFIG_INIT;
        raw.application_name = config.applicationName.c_str();
        raw.application_version = config.applicationVersion.c_str();
        throwIfError(fiaisdk_sdk_create(&raw, &handle_));
    }

    ~Sdk() {
        fiaisdk_sdk_destroy(handle_);
    }

    Sdk(const Sdk &) = delete;
    Sdk &operator=(const Sdk &) = delete;

    Sdk(Sdk &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Sdk &operator=(Sdk &&other) noexcept {
        if (this != &other) {
            fiaisdk_sdk_destroy(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    /// Throws fiaisdk::Error(INVALID_ARGUMENT) if the instance was moved from, or
    /// std::bad_alloc if copying `name` fails.
    [[nodiscard]] bool hasModule(std::string_view name) const {
        const std::string terminated(name);
        std::uint8_t available = 0;
        throwIfError(fiaisdk_sdk_has_module(handle_, terminated.c_str(), &available));
        return available != 0;
    }

    /// Underlying C handle for calling C functions not yet wrapped. Not owned by the caller.
    [[nodiscard]] fiaisdk_sdk *native() const noexcept {
        return handle_;
    }

  private:
    fiaisdk_sdk *handle_ = nullptr;
};

} // namespace fiaisdk
