#!/usr/bin/env python3
"""Enforce FiaiSDK dependency rules (docs/architecture/target-architecture.md §3).

Usage: python3 tools/check_layering.py [--warn-only] [--root PATH]
Exit code 1 when violations are found (unless --warn-only).
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
SOURCE_EXT = {".h", ".hpp", ".cpp", ".c", ".cc", ".ipp"}
LAYERS = ("domain", "application", "infrastructure", "api")
CORE_LAYERS = ("domain", "application", "infrastructure")
OS_DIRS = ("windows", "macos", "linux")

# Headers that must never appear in domain/ or application/ (I/O, OS, 3rd-party, threads).
IMPURE_HEADERS = re.compile(
    r"^(windows\.h|winsock2?\.h|unistd\.h|sys/|pthread\.h|dlfcn\.h|fcntl\.h|"
    r"curl/|openssl/|sodium|secp256k1|nlohmann/|boost/|Q[A-Z]\w*|Qt\w*/|"
    r"thread|fstream|iostream|filesystem|random|future)"
)


@dataclass(frozen=True)
class Unit:
    kind: str  # bootstrap | core | platform | platform_os | bindings | module | other
    module: str = ""  # module name ("core" for src/core)
    layer: str = ""  # domain | application | infrastructure | api | root | flat


def classify(rel: Path) -> Unit:
    p = rel.parts
    if not p or p[0] != "src" or len(p) < 2:
        return Unit("other")
    if p[1] == "core":
        if len(p) > 3 and p[2] in CORE_LAYERS:
            return Unit("core", "core", p[2])
        return Unit("core", "core", "misplaced")
    if p[1] == "bootstrap":
        layer = "root" if len(p) == 3 and Path(p[2]).stem == "composition_root" else ""
        return Unit("bootstrap", layer=layer)
    if p[1] == "platform":
        return Unit("platform_os") if len(p) > 2 and p[2] in OS_DIRS else Unit("platform")
    if p[1] == "bindings":
        return Unit("bindings")
    if p[1] == "modules" and len(p) > 2:
        layer = p[3] if len(p) > 4 and p[3] in LAYERS else "flat"
        return Unit("module", p[2], layer)
    return Unit("other")


def resolve(src_file: Path, inc: str, root: Path) -> Path | None:
    for base in (src_file.parent, root / "src", root / "include"):
        cand = (base / inc).resolve()
        if cand.is_file():
            try:
                return cand.relative_to(root)
            except ValueError:
                return None
    return None


# Which core layers each layer may use (core is the shared kernel of every module).
CORE_ALLOWED = {
    "domain": {"domain"},
    "application": {"domain", "application"},
    "infrastructure": {"domain", "application", "infrastructure"},
    "api": {"domain", "application"},
    "flat": {"domain", "application"},
}
# Which layers of the SAME module each layer may use.
OWN_ALLOWED = {
    "domain": {"domain"},
    "application": {"domain", "application"},
    "infrastructure": {"domain", "application", "infrastructure"},
    "api": {"domain", "application", "api"},
    "flat": set(LAYERS) | {"flat"},
}


def check_edge(src: Unit, dst: Unit) -> str | None:
    composition_root = src.kind == "bootstrap" and src.layer == "root"
    if dst.kind == "bootstrap" and src.kind not in ("bootstrap", "bindings"):
        return "core, modules and platform must not depend on bootstrap"
    if dst.kind == "platform_os" and src.kind != "platform_os" and not composition_root:
        return "only platform/<os>/ or the composition root may include OS-specific implementations"
    if composition_root:
        return None  # the composition root wires everything together

    if src.kind == "core":
        if dst.kind == "module":
            return "core must not depend on modules (wiring belongs in bootstrap/composition_root.cpp)"
        if dst.kind == "bindings":
            return "core must not depend on bindings"
        if dst.kind == "core" and dst.layer not in CORE_ALLOWED[src.layer]:
            return f"core/{src.layer} must not depend on core/{dst.layer}"
        if src.layer in ("domain", "application") and dst.kind in ("platform", "platform_os"):
            return f"core/{src.layer} must not depend on platform (inject a port)"
        return None

    if src.kind == "module":
        if dst.kind == "core":
            if dst.layer not in CORE_ALLOWED[src.layer]:
                return f"{src.module}/{src.layer} must not depend on core/{dst.layer}"
            return None
        if dst.kind == "module":
            if dst.module != src.module:
                if src.layer == "domain":
                    return f"domain must not depend on other modules ('{dst.module}')"
                if dst.layer != "api":
                    return f"cross-module access to '{dst.module}/{dst.layer}' (use {dst.module}/api)"
                return None
            if dst.layer not in OWN_ALLOWED[src.layer]:
                return f"{src.layer} must not depend on {dst.layer}"
            return None
        if dst.kind == "bindings":
            return "modules must not depend on bindings"
        if src.layer in ("domain", "application") and dst.kind in ("platform", "platform_os"):
            return f"{src.layer} must not depend on platform (inject a port)"
        return None

    if src.kind == "bindings":
        if dst.kind == "module" and dst.layer != "api":
            return f"bindings may only use module facades ('{dst.module}/api'), not {dst.module}/{dst.layer}"
        if dst.kind in ("platform", "platform_os"):
            return "bindings must not depend on platform"
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--warn-only", action="store_true")
    ap.add_argument("--root", default=Path(__file__).resolve().parent.parent, type=Path)
    args = ap.parse_args()
    root: Path = args.root.resolve()

    violations: list[str] = []
    for f in sorted((root / "src").rglob("*")):
        if f.suffix not in SOURCE_EXT or not f.is_file():
            continue
        rel = f.relative_to(root)
        src = classify(rel)
        if src.layer == "misplaced":
            violations.append(
                f"{rel}: src/core files must live in domain/, application/ or infrastructure/ "
                "(SDK assembly belongs in src/bootstrap)")
            continue
        for lineno, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(2)
            if src.kind in ("module", "core") and src.layer in ("domain", "application") and IMPURE_HEADERS.match(inc):
                violations.append(f"{rel}:{lineno}: {src.layer} includes impure header <{inc}>")
                continue
            target = resolve(f, inc, root)
            if target is None:
                continue
            msg = check_edge(src, classify(target))
            if msg:
                violations.append(f"{rel}:{lineno}: {msg}  [#include {inc}]")

    for v in violations:
        print(v)
    print(f"check_layering: {len(violations)} violation(s)")
    return 1 if violations and not args.warn_only else 0


if __name__ == "__main__":
    sys.exit(main())
