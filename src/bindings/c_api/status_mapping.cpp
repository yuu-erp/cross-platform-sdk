#include "bindings/c_api/status_mapping.h"

#include <exception>
#include <new>

namespace fiaisdk::bindings {

fiaisdk_status toStatus(const kernel::Error &error) noexcept {
    switch (error.code) {
    case kernel::ErrorCode::InvalidArgument:
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    case kernel::ErrorCode::NotInitialized:
        return FIAISDK_STATUS_NOT_INITIALIZED;
    case kernel::ErrorCode::ModuleNotFound:
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    case kernel::ErrorCode::ModuleInitFailed:
    case kernel::ErrorCode::Internal:
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
