# Contributing

Thank you for your interest in contributing to this project.

This document describes the development standards, contribution workflow, and quality requirements for the SDK.

## 1. Development Requirements

- Use **C++20** as the minimum supported C++ standard.
- Use **CMake** as the build system.
- Follow the project's `.clang-format` configuration.
- Follow the project's `.clang-tidy` configuration where applicable.
- Use consistent naming conventions across the codebase.
- Prefer modern C++ practices, RAII, smart pointers, and strong type safety.
- Avoid unnecessary dependencies and platform-specific code in shared modules.

## 2. Code Guidelines

- Keep public APIs simple, consistent, and well-documented.
- Keep public headers source-compatible whenever possible.
- Avoid exposing implementation details through public headers.
- Keep module dependencies explicit and avoid cyclic dependencies.
- Follow the existing architecture and module boundaries.
- Separate business logic from platform-specific implementations.
- Use interfaces or abstractions when integrating external systems.
- Avoid global mutable state unless explicitly justified.
- Handle errors explicitly; do not silently ignore failures.
- Ensure thread safety for shared resources and asynchronous operations.

## 3. Cross-platform Compatibility

- Support all officially targeted platforms: Windows, macOS, and Linux.
- Avoid platform-specific APIs in shared code.
- Isolate platform-specific implementations behind clear interfaces.
- Verify compiler and platform compatibility before introducing new dependencies.
- Do not assume filesystem paths, environment variables, or system behavior are identical across platforms.
- Test platform-specific changes on the affected operating systems whenever possible.

## 4. Build and Development Workflow

Before submitting changes, ensure that the project can be configured and built successfully.

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Run formatting checks (CI pins **clang-format 18.1.8**; other major versions format differently):

```bash
pipx install clang-format==18.1.8
clang-format --dry-run --Werror <source-files>
```

If static analysis is configured, run the project's `clang-tidy` checks as well.

## 5. Testing Requirements

- Tests use GoogleTest + GoogleMock (ADR-0004); C ABI tests stay plain C99.
- Add or update tests for every behavior change.
- Add unit tests for new business logic.
- Add integration tests when changing interactions between modules.
- Add regression tests when fixing bugs.
- Test error handling and edge cases.
- Verify public API behavior when modifying exported interfaces.
- Do not merge changes that introduce known test failures.

## 6. Public API and ABI Compatibility

- Avoid breaking existing public APIs without a documented reason.
- Prefer additive changes over modifying or removing existing interfaces.
- Document breaking changes explicitly.
- Consider ABI compatibility when modifying exported symbols, public structures, or binary interfaces.
- Keep memory ownership, object lifetime, and error-handling contracts explicit.
- Do not expose internal implementation types across the C ABI boundary.

## 7. Commit Guidelines

Use Conventional Commits for commit messages.

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

Recommended commit types:

- `feat`: Add a feature.
- `fix`: Fix a bug.
- `refactor`: Refactor code without changing behavior.
- `test`: Add or update tests.
- `docs`: Documentation changes.
- `build`: Build system or dependency changes.
- `ci`: CI/CD configuration changes.
- `perf`: Performance improvements.
- `chore`: Maintenance tasks.

## 8. Pull Request Guidelines

Before submitting a pull request:

- Ensure the changes follow the project's coding standards.
- Ensure the project builds successfully.
- Run relevant tests and static analysis.
- Update documentation when public behavior changes.
- Keep pull requests focused on a single concern.
- Provide a clear description of the problem and solution.
- Include relevant test results.
- Link related issues when applicable.

Pull requests should include:

1. Summary of changes.
2. Motivation and context.
3. Implementation details.
4. Testing performed.
5. Breaking changes, if any.

## 9. Security Guidelines

- Never commit credentials, API tokens, passwords, real private keys, or sensitive configuration.
- Never commit generated binaries, build outputs, or temporary files.
- Use environment variables or secure configuration mechanisms for sensitive values.
- Report security vulnerabilities privately instead of publishing exploit details in public issues.
- Avoid logging secrets, private keys, or sensitive user information.
- Validate external inputs and handle untrusted data safely.

## 10. Documentation

- Document all new public APIs.
- Include usage examples for new modules and significant features.
- Update architecture documentation when changing module boundaries or dependencies.
- Keep README, API references, and examples consistent with the implementation.

## 11. Scope and Contribution Principles

- Keep changes focused and maintainable.
- Avoid introducing abstractions without a clear use case.
- Do not introduce unnecessary third-party dependencies.
- Discuss major architectural changes before implementation.
- Preserve backward compatibility whenever reasonably possible.

Thank you for helping improve this project.
