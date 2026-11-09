# ADR-0002: Hourglass API — stable C ABI, header-only C++ wrapper

- Status: Accepted
- Date: 2026-11-09

## Context
Consumers: C, C#, Swift, Kotlin/JNI, Qt/C++ on Windows (MSVC), macOS (Clang), Linux (GCC/Clang).
C++ ABI (STL layout, exceptions, allocator/CRT) is not stable across compilers, versions or
MSVC Debug/Release runtimes. The scaffold exports `class SDK` with `std::string`/`unique_ptr`.

## Decision
- Only `extern "C"` functions with C types are exported (`FIAISDK_API`).
- The C++ API in `include/fiaisdk/cpp/` is header-only, inline, built on the C ABI
  (RAII handles, `std::string_view` in, `std::string` out by copying).
- Internally C ABI → core/module facades. The C layer never wraps the public C++ wrapper.
- Qt adapter, if any, is a separate target on top of the C++ wrapper.

## Consequences
+ One binary works for every consumer/compiler. + FFI generation (C#, Swift, JNI) is trivial.
- Some boilerplate per function; small copy overhead at boundary (acceptable).

## ABI / compatibility impact
Breaking vs scaffold (pre-1.0, acceptable).
