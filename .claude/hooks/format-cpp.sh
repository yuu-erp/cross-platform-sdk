#!/usr/bin/env bash
# PostToolUse hook: clang-format the C/C++ file Claude just edited. Silent no-op if unavailable.
set -u
command -v clang-format >/dev/null 2>&1 || exit 0
input="$(cat)"
file="$(printf '%s' "$input" | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("tool_input",{}).get("file_path",""))' 2>/dev/null)"
case "$file" in
  *.h|*.hpp|*.c|*.cpp|*.cc) [ -f "$file" ] && clang-format -i --style=file "$file" ;;
esac
exit 0
