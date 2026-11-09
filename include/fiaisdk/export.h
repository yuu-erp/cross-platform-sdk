/* SPDX-License-Identifier: UNLICENSED */
#ifndef FIAISDK_EXPORT_H
#define FIAISDK_EXPORT_H

/* FIAISDK_API marks a symbol exported from the shared library. Only C functions use it. */
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(FIAISDK_BUILDING_LIBRARY)
#define FIAISDK_API __declspec(dllexport)
#else
#define FIAISDK_API __declspec(dllimport)
#endif
#define FIAISDK_CALL __cdecl
#else
#if defined(__GNUC__) || defined(__clang__)
#define FIAISDK_API __attribute__((visibility("default")))
#else
#define FIAISDK_API
#endif
#define FIAISDK_CALL
#endif

/* FIAISDK_DEPRECATED("use fiaisdk_x_v2") marks a function scheduled for removal. */
#if defined(_MSC_VER)
#define FIAISDK_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define FIAISDK_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define FIAISDK_DEPRECATED(msg)
#endif

#endif /* FIAISDK_EXPORT_H */
