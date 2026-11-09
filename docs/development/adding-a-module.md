# Adding a module

AI agents: use the `new-module` skill (`.claude/skills/new-module/SKILL.md`). Humans: same steps.

1. Agree on the module's responsibility (one sentence) and its dependencies; add edges to
   `docs/architecture/module-graph.md` (no cycles).
2. Create `src/modules/<name>/` with `README.md`, `CMakeLists.txt` and `api/<name>_module.{h,cpp}`
   (class `<Name>Module final : public core::Module`, `kName = "<name>"`).
3. Add `domain/`, `application/` (+ `ports/`) and `infrastructure/` only when the first real use
   case needs them (ADR-0001).
4. `add_subdirectory(modules/<name>)` in `src/CMakeLists.txt` and link `fiaisdk_<name>` into `fiaisdk`.
5. Register it in `src/core/composition_root.cpp` after its dependencies.
6. Public API: `include/fiaisdk/<name>.h` + `src/bindings/c_api/<name>_c_api.cpp` + C++ wrapper,
   only when consumers need it (`new-c-api` skill). Add `tests/abi/` coverage.
7. Unit tests under `tests/unit/<name>/`.
8. Run: build, `ctest`, `python3 tools/check_layering.py`, clang-format, clang-tidy.
