# CLAUDE.md

@AGENTS.md

## Claude-specific
- Talk to the project owner in **Vietnamese**; code, comments, docs, commit messages in English.
- Path-scoped rules in `.claude/rules/` load automatically for the files you touch — follow them.
- Use project skills in `.claude/skills/` for recurring work: `new-module`, `new-c-api`,
  `new-use-case`, `write-adr`, `verify`, `review-change`.
- For any change under `include/fiaisdk/` or `src/bindings/`, run the `abi-reviewer` subagent
  before reporting done. For new modules/layers, run `architecture-reviewer`.
- When architecture, ABI, security or public behavior is ambiguous: stop and ask, do not guess.
- Finish every task with: what changed, files, commands run + real results, limitations.

Previous long version of this file: `docs/archive/CLAUDE.v0.md` (content moved into rules).
