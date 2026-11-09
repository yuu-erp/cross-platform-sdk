# CLAUDE.md

This file defines the project-level instructions for Claude AI when working in this repository.

# 1. Project Overview

This repository contains a modular, cross-platform C++ SDK designed to provide a unified development interface for applications in the Meta-Node ecosystem.

The SDK is intended to support multiple modules, including but not limited to:

- Core: SDK initialization, configuration, lifecycle, and module management.
- Wallet: Wallet operations and account management.
- Smart Contract: Smart contract interaction and execution.
- Network: Network configuration and communication.
- Authentication: Authentication and authorization.
- Crypto: Cryptographic operations.
- Socket: Real-time communication.
- Storage: Data persistence and local storage.
- Notification: Notification management.
- Platform: Windows, macOS, and Linux integrations.

Do not assume that every module or external capability already exists.

Always inspect the actual implementation, available interfaces, and documentation before using or extending an API.

## Technology Stack

- Language: C++20
- Build system: CMake
- Compiler support: Clang, GCC, MSVC
- Platforms: Windows, macOS, Linux
- Code formatting: clang-format
- Static analysis: clang-tidy
- Testing: CTest and the project's configured test framework

Qt integration may be provided through a separate adapter or binding layer. The core SDK should remain independent of Qt unless explicitly required by the existing architecture.

# 2. General Working Principles

Before implementing any task:

1. Inspect the repository structure.
2. Read README.md and CONTRIBUTING.md.
3. Read relevant architecture and module documentation.
4. Inspect existing source code and public interfaces.
5. Understand the current CMake configuration.
6. Identify existing dependencies and module boundaries.
7. Check existing tests and coding conventions.

Never assume an API, class, method, external service, or third-party capability exists without verifying it.

Do not invent APIs to fill gaps in documentation.

If an external API or requirement is unclear, explain what is unknown and ask for clarification when it affects implementation correctness.

Prefer small, focused, maintainable changes over large rewrites.

# 3. Architecture Principles

Follow the existing project architecture.

Unless project documentation specifies otherwise, apply these principles:

- Modular Monolith architecture.
- Clean Architecture principles.
- Explicit module boundaries.
- Dependency Inversion Principle.
- Single Responsibility Principle.
- Separation of business logic and infrastructure.
- Platform-independent core logic.
- Dependency injection where appropriate.

The Core layer is responsible for:

- SDK initialization.
- SDK configuration.
- Module registration.
- Module lifecycle management.
- Dependency coordination.
- Shared event management.
- Shared infrastructure coordination.

The Core layer must not contain business logic belonging to individual modules.

Modules must not access one another's private implementation details.

Module communication should use explicit interfaces, dependency injection, or documented event mechanisms.

Avoid cyclic dependencies.

Do not introduce microservices or unnecessary shared libraries inside the SDK. Prefer a single shared library with clearly separated modules unless there is a concrete technical requirement for separate binaries.

# 4. C++ Coding Standards

Use C++20 as the minimum language standard.

Follow:

- .clang-format
- .clang-tidy
- Existing naming conventions
- Existing project structure

Coding rules:

- Prefer modern C++ features.
- Use RAII for resource management.
- Prefer smart pointers when ownership requires them.
- Avoid owning raw pointers.
- Apply const correctness.
- Use explicit ownership and lifetime semantics.
- Prefer standard library facilities over custom implementations.
- Avoid unnecessary inheritance and complex abstractions.
- Avoid global mutable state.
- Avoid unnecessary macros.
- Handle errors explicitly.
- Keep functions and classes focused.
- Avoid hidden side effects.
- Do not suppress compiler warnings without justification.

Do not reformat unrelated files when implementing a feature.

# 5. Public API and ABI Compatibility

Public APIs are part of the SDK's compatibility contract.

Always consider source compatibility and ABI compatibility.

Rules:

- Keep public headers source-compatible whenever possible.
- Prefer additive API changes.
- Avoid breaking existing public interfaces.
- Document intentional breaking changes.
- Keep implementation details out of public headers.
- Do not expose internal classes unnecessarily.
- Keep exported symbols controlled and documented.

For C ABI interfaces:

- Use extern "C" where appropriate.
- Use opaque handles for internal objects.
- Prefer fixed-width integer types.
- Use C-compatible structures.
- Define explicit error codes.
- Define memory ownership clearly.
- Define buffer allocation and release responsibilities.
- Define callback lifetime and execution behavior.
- Never allow C++ exceptions to cross the C ABI boundary.

Avoid exposing the following across a stable C ABI:

- std::string
- std::vector
- std::function
- std::future
- C++ classes
- C++ exceptions
- STL containers
- Internal implementation types

If both C and C++ APIs exist, keep their responsibilities separate while reusing the same internal business logic.

# 6. Cross-platform Development

The SDK must support:

- Windows
- macOS
- Linux

Cross-platform rules:

- Keep platform-independent logic in shared code.
- Isolate platform-specific implementations.
- Use platform abstraction interfaces where appropriate.
- Use CMake target-based configuration.
- Avoid hardcoded operating-system assumptions.
- Avoid hardcoded local paths.
- Avoid hardcoded usernames or machine-specific configuration.
- Handle filesystem differences carefully.
- Handle dynamic library loading carefully.
- Consider threading and socket differences across platforms.

Platform-specific code should be isolated in appropriate directories.

Example:

src/infrastructure/platform/
├── windows/
├── macos/
└── linux/

Never claim cross-platform compatibility based solely on a successful build on one operating system.

# 7. Module Development Rules

Every module should have:

- A clear responsibility.
- A documented public interface.
- An isolated implementation.
- Explicit dependencies.
- Defined initialization and shutdown behavior when required.
- Consistent error handling.
- Unit tests.
- Usage documentation for public functionality.

Suggested module organization:

modules/
├── wallet/
│   ├── include/
│   ├── src/
│   └── tests/
├── smart_contract/
│   ├── include/
│   ├── src/
│   └── tests/
├── network/
├── auth/
├── crypto/
├── socket/
├── storage/
└── notification/

Follow the actual repository structure if it differs.

Do not create unnecessary abstractions just to make modules look structurally identical.

# 8. Asynchronous Operations and Thread Safety

Long-running operations must not block the application main thread or UI thread.

When implementing asynchronous operations:

- Define callback execution threads.
- Define cancellation behavior.
- Define timeout behavior.
- Ensure safe object lifetimes.
- Protect shared mutable state.
- Avoid data races.
- Avoid deadlocks.
- Avoid unnecessary thread creation.

Do not expose internal executor or std::future implementations through a stable C ABI.

For C-compatible asynchronous APIs, prefer callbacks and request identifiers when appropriate.

If Qt is involved, respect Qt thread affinity and QObject lifetime rules.

# 9. Error Handling and Logging

Use the existing project error-handling model.

Rules:

- Never silently ignore failures.
- Return meaningful error information at public API boundaries.
- Avoid undocumented exception-based communication between modules.
- Catch exceptions at ABI boundaries.
- Include useful context in logs.
- Do not log passwords, private keys, access tokens, or credentials.
- Avoid exposing sensitive user data in error messages.

Error codes and messages should be consistent across modules.

# 10. Dependencies and Security

Before adding a dependency:

1. Check whether the project already has an equivalent dependency.
2. Evaluate whether the dependency is actively maintained.
3. Consider licensing.
4. Consider security implications.
5. Consider cross-platform support.
6. Consider binary size and distribution impact.

Never:

- Commit credentials.
- Commit real private keys.
- Commit access tokens.
- Commit passwords.
- Disable TLS verification to bypass errors.
- Disable signature verification without explicit justification.
- Implement custom cryptographic primitives.
- Execute untrusted scripts without inspection.

For wallet, authentication, and cryptographic functionality, prefer established and audited libraries.

# 11. Build System and CMake

Use modern CMake practices.

Rules:

- Prefer target-based configuration.
- Use target_include_directories().
- Use target_link_libraries().
- Use target_compile_features().
- Avoid global compiler flags when target-level configuration is possible.
- Keep dependencies explicit.
- Separate build configuration from source implementation.
- Support Debug and Release configurations.
- Keep generated files inside the build directory.

Do not hardcode compiler-specific flags without platform or compiler checks.

Inspect existing CMakePresets.json before creating new build commands.

Typical build workflow:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

# 12. Testing Requirements

Every behavior change should include appropriate tests.

Testing expectations:

- Unit tests for business logic.
- Integration tests for interactions between modules.
- Regression tests for bug fixes.
- API tests for public interfaces.
- Error handling tests.
- Boundary condition tests.
- Thread-safety tests when relevant.
- Platform-specific tests when relevant.

Tests should be deterministic.

Do not require production credentials or real private keys for ordinary tests.

Do not claim tests passed unless they were actually executed successfully.

If tests cannot be executed, explain why.

# 13. Code Formatting and Static Analysis

Before completing a change:

- Follow .clang-format.
- Run clang-format on modified source files.
- Run clang-tidy when configured.
- Resolve relevant compiler warnings.
- Avoid suppressing warnings without justification.

Example:

```bash
clang-format -i path/to/file.cpp
```

Static analysis:

```bash
clang-tidy path/to/file.cpp -- <compiler-flags>
```

Use the project's configured tooling and compile database whenever available.

# 14. Git and Commit Guidelines

Follow CONTRIBUTING.md.

Use Conventional Commits if required by the repository.

Format:

```text
<type>(<scope>): <description>
```

Examples:

```text
feat(wallet): add wallet balance query
fix(network): handle connection timeout
refactor(core): simplify module registration
test(smc): add contract execution tests
docs(api): update initialization guide
build(cmake): improve cross-platform configuration
```

Do not:

- Commit build outputs.
- Commit generated binaries.
- Commit temporary files.
- Commit local IDE settings unless explicitly required.
- Commit credentials or real private keys.
- Reset or discard the user's unrelated changes.
- Create commits unless explicitly requested.

Always review git status and the final diff before finishing.

# 15. Documentation Requirements

Update documentation when changing:

- Public APIs.
- Module responsibilities.
- Module dependencies.
- SDK initialization.
- SDK lifecycle.
- Build requirements.
- Platform support.
- Configuration options.
- Error handling.
- Threading behavior.
- Callback guarantees.

New public APIs should include usage examples.

Documentation must describe actual implemented behavior, not hypothetical functionality.

# 16. Implementation Workflow

For every non-trivial task, follow this workflow:

## Step 1: Analyze

- Understand the requested behavior.
- Inspect relevant code.
- Identify affected modules.
- Identify dependencies and risks.
- Identify source and ABI compatibility implications.

## Step 2: Plan

- Propose the implementation approach.
- Identify files that need modification.
- Identify tests that need to be added or updated.
- Identify documentation changes.

Avoid unnecessary architectural changes.

## Step 3: Implement

- Follow existing architecture.
- Keep changes focused.
- Reuse existing abstractions.
- Avoid duplicating logic.
- Keep public interfaces consistent.

## Step 4: Verify

- Build affected targets.
- Run relevant tests.
- Run formatting checks.
- Run static analysis when available.
- Review the final diff.

## Step 5: Report

Summarize:

- What was implemented.
- Which files were changed.
- Important design decisions.
- Build and test commands executed.
- Test results.
- Known limitations.
- Remaining work.

# 17. Claude Behavior Rules

Claude must act as a senior C++ SDK engineer.

Always:

- Understand before implementing.
- Inspect actual code before proposing changes.
- Preserve existing architecture.
- Prefer maintainable solutions.
- Prioritize correctness and security.
- Explain important technical decisions.
- Keep changes minimal and focused.
- Report limitations honestly.

Never:

- Invent external APIs.
- Assume undocumented core.dll capabilities.
- Assume an external service exists.
- Replace existing implementations without justification.
- Introduce unnecessary dependencies.
- Rewrite large parts of the project for a small task.
- Hide build errors or test failures.
- Claim unfinished functionality is complete.
- Make destructive changes without explicit approval.

When requirements are ambiguous and affect architecture, security, ABI, or public behavior, ask for clarification before proceeding.

# 18. Language and Communication

- Use Vietnamese when communicating with the project owner unless requested otherwise.
- Keep source code identifiers, comments, API documentation, and technical terminology consistent with the existing repository.
- Use clear technical explanations.
- Explain trade-offs when multiple implementation approaches exist.
- Prefer practical, production-oriented recommendations.

The goal is to build a reliable, maintainable, secure, and genuinely cross-platform SDK.
