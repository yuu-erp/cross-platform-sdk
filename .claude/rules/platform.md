---
paths:
  - "src/platform/**"
---
# Platform layer

- Shared code depends on ports in `src/platform/` (e.g. `IFileSystem`, `IKeyStore`, `IDynamicLibrary`);
  implementations live only in `windows/`, `macos/`, `linux/` and are selected in CMake, not with
  `#ifdef` scattered in shared code.
- Paths: `std::filesystem::path`; UTF-8 at API edges, convert to wide only inside `windows/`.
- No hard-coded paths, usernames, or env assumptions; use OS APIs for app-data dirs.
- A change here is not "cross-platform" until built and tested on all three OSes (CI matrix).
