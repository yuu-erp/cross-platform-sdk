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
OS_DIRS = ("windows", "macos", "linux")

# Headers that must never appear in domain/ or application/ (I/O, OS, 3rd-party, threads).
IMPURE_HEADERS = re.compile(
    r"^(windows\.h|winsock2?\.h|unistd\.h|sys/|pthread\.h|dlfcn\.h|fcntl\.h|"
    r"curl/|openssl/|sodium|secp256k1|nlohmann/|boost/|Q[A-Z]\w*|Qt\w*/|"
    r"thread|fstream|iostream|filesystem|random|future)"
)


@dataclass(frozen=True)
class Unit:
    kind: str  # kernel | platform | platform_os | core | bindings | module | other
    module: str = ""
    layer: str = ""  # for modules: domain/application/infrastructure/api/flat


def classify(rel: Path) -> Unit:
    p = rel.parts
    if not p or p[0] != "src":
        return Unit("other")
    if len(p) > 1 and p[1] == "kernel":
        return Unit("kernel")
    if len(p) > 1 and p[1] == "platform":
        return Unit("platform_os") if len(p) > 2 and p[2] in OS_DIRS else Unit("platform")
    if len(p) > 1 and p[1] == "core":
        return Unit("core")
    if len(p) > 1 and p[1] == "bindings":
        return Unit("bindings")
    if len(p) > 2 and p[1] == "modules":
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


def check_edge(f: Path, src: Unit, dst: Unit) -> str | None:
    name = f.name
    if dst.kind == "platform_os" and src.kind != "platform_os" and name != "composition_root.cpp":
        return "only platform/<os>/ or composition_root may include OS-specific implementations"
    if src.kind == "kernel" and dst.kind not in ("kernel", "other"):
        return "kernel must not depend on other src/ code"
    if src.kind == "core" and dst.kind == "module":
        if name != "composition_root.cpp":
            return "core may reference modules only in composition_root.cpp"
        return None
    if src.kind == "module" and dst.kind == "module":
        if dst.module != src.module:
            if dst.layer != "api":
                return f"cross-module access to '{dst.module}/{dst.layer}' (use {dst.module}/api)"
            return None
        allowed = {
            "domain": {"domain"},
            "application": {"domain", "application"},
            "infrastructure": {"domain", "application", "infrastructure"},
            "api": {"domain", "application", "api"},
            "flat": set(LAYERS) | {"flat"},
        }[src.layer]
        if dst.layer not in allowed:
            return f"{src.layer} must not depend on {dst.layer}"
        return None
    if src.kind == "module" and src.layer in ("domain", "application"):
        if dst.kind in ("core", "bindings", "platform", "platform_os"):
            return f"{src.layer} must not depend on {dst.kind} (inject a port)"
    if src.kind == "module" and dst.kind == "bindings":
        return "modules must not depend on bindings"
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
        for lineno, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(2)
            if src.kind == "module" and src.layer in ("domain", "application") and IMPURE_HEADERS.match(inc):
                violations.append(f"{rel}:{lineno}: {src.layer} includes impure header <{inc}>")
                continue
            target = resolve(f, inc, root)
            if target is None:
                continue
            msg = check_edge(f, src, classify(target))
            if msg:
                violations.append(f"{rel}:{lineno}: {msg}  [#include {inc}]")

    for v in violations:
        print(v)
    print(f"check_layering: {len(violations)} violation(s)")
    return 1 if violations and not args.warn_only else 0


if __name__ == "__main__":
    sys.exit(main())
