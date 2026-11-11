#include "bindings/c_api/status_mapping.h"

#include <exception>
#include <new>

namespace fiaisdk::bindings {

fiaisdk_status toStatus(const core::Error &error) noexcept {
    switch (error.code) {
    case core::ErrorCode::InvalidArgument:
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    case core::ErrorCode::NotInitialized:
        return FIAISDK_STATUS_NOT_INITIALIZED;
    case core::ErrorCode::ModuleNotFound:
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    case core::ErrorCode::Unavailable:
        return FIAISDK_STATUS_UNAVAILABLE;
    case core::ErrorCode::NotFound:
        return FIAISDK_STATUS_NOT_FOUND;
    case core::ErrorCode::AlreadyExists:
        return FIAISDK_STATUS_ALREADY_EXISTS;
    case core::ErrorCode::AuthenticationFailed:
        return FIAISDK_STATUS_AUTHENTICATION_FAILED;
    case core::ErrorCode::ModuleInitFailed:
    case core::ErrorCode::Internal:
        return FIAISDK_STATUS_INTERNAL_ERROR;
    }
    return FIAISDK_STATUS_INTERNAL_ERROR;
}

fiaisdk_status statusFromCurrentException() noexcept {
    try {
        throw;
    } catch (const std::bad_alloc &) {
        return FIAISDK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return FIAISDK_STATUS_INTERNAL_ERROR;
    }
}

} // namespace fiaisdk::bindings
