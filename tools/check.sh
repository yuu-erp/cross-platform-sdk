#!/usr/bin/env bash
# Run the same checks as CI, locally.
#
#   tools/check.sh            quick: layering + format + debug build + all tests   (~1 min)
#   tools/check.sh full       quick + ASan/UBSan + TSan + clang-tidy + install/find_package
#   tools/check.sh --fresh    wipe the build directories first (after CMake/compiler changes)
#
# Extra CMake configure arguments: FIAISDK_CMAKE_ARGS="-DFOO=ON" tools/check.sh
# Works on macOS, Linux and Windows (Git Bash). Stops at the first failing step.
set -euo pipefail
cd "$(dirname "$0")/.."

MODE=quick
FRESH=""
for arg in "$@"; do
    case "$arg" in
        quick | full) MODE="$arg" ;;
        --fresh) FRESH="--fresh" ;;
        -h | --help) sed -n '2,9p' "$0"; exit 0 ;;
        *) echo "unknown argument: $arg (use quick, full, --fresh)"; exit 2 ;;
    esac
done

# shellcheck disable=SC2206
EXTRA_ARGS=(${FIAISDK_CMAKE_ARGS:-})
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
CURRENT=""
DONE=()

step() {
    CURRENT="$*"
    printf '\n\033[1;34m==> %s\033[0m\n' "$*"
}
finish() {
    local code=$?
    if [ "$code" -ne 0 ]; then
        printf '\n\033[1;31mFAILED at: %s\033[0m\n' "$CURRENT"
    else
        printf '\n\033[1;32mAll checks passed (%s):\033[0m\n' "$MODE"
        printf "  - %s\n" ${DONE[@]+"${DONE[@]}"}
    fi
}
trap finish EXIT

case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*) WINDOWS=1 ;;
    *) WINDOWS=0 ;;
esac

build_and_test() { # configure-preset build-preset test-preset
    step "configure $1"
    # ${a[@]+"${a[@]}"}: empty arrays are "unbound" under set -u in bash 3.2 (macOS default).
    cmake --preset "$1" $FRESH ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
    step "build $2"
    cmake --build --preset "$2" --parallel "$JOBS"
    step "test $3"
    ctest --preset "$3" -j "$JOBS"
    DONE+=("build + ctest: $3")
}

step "layering rules"
python3 tools/check_layering.py
DONE+=("layering")

step "clang-format"
if command -v clang-format >/dev/null 2>&1; then
    version="$(clang-format --version)"
    case "$version" in
        *" 18."*) ;;
        *) echo "WARNING: CI uses clang-format 18.1.8, you have: $version" ;;
    esac
    git ls-files '*.h' '*.hpp' '*.c' '*.cpp' | xargs clang-format --dry-run --Werror
    DONE+=("clang-format")
else
    echo "WARNING: clang-format not found, skipped (pipx install clang-format==18.1.8)"
fi

if [ "$WINDOWS" -eq 1 ]; then
    build_and_test msvc msvc-debug msvc-debug
else
    build_and_test debug debug debug
fi

if [ "$MODE" = full ]; then
    if [ "$WINDOWS" -eq 0 ]; then
        build_and_test asan asan asan
        build_and_test tsan tsan tsan
    fi

    step "clang-tidy"
    if command -v clang-tidy >/dev/null 2>&1 && [ -f build/debug/compile_commands.json ]; then
        output="$(git ls-files 'src/*.cpp' |
            xargs -P "$JOBS" -n 2 clang-tidy -p build/debug --quiet 2>&1 || true)"
        if printf '%s\n' "$output" | grep -E "warning:|error:"; then
            echo "clang-tidy reported problems (see above)"
            exit 1
        fi
        DONE+=("clang-tidy")
    else
        echo "WARNING: clang-tidy or build/debug/compile_commands.json missing, skipped"
    fi

    if [ "$WINDOWS" -eq 0 ]; then
        step "install + find_package consumer"
        rm -rf build/check-stage build/check-package
        cmake --install build/debug --prefix build/check-stage >/dev/null
        cmake -S tests/package -B build/check-package -G Ninja \
            -DCMAKE_PREFIX_PATH="$PWD/build/check-stage" >/dev/null
        cmake --build build/check-package >/dev/null
        LD_LIBRARY_PATH="$PWD/build/check-stage/lib" DYLD_LIBRARY_PATH="$PWD/build/check-stage/lib" \
            ./build/check-package/fiaisdk_package_test
        DONE+=("install + find_package")
    fi
fi
