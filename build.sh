#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIGURATION="Release"
CPU_ONLY=0
SHARED=0
CLEAN=0
SKIP_TESTS=0
WARNINGS_AS_ERRORS=0
SANITIZE=0
EPOCHGUI_PATH=""
EPOCHPLATFORM_PATH=""

if [[ $# -gt 0 && "$1" != --* ]]; then
    CONFIGURATION="$1"
    shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cpu-only) CPU_ONLY=1 ;;
        --shared) SHARED=1 ;;
        --clean) CLEAN=1 ;;
        --skip-tests) SKIP_TESTS=1 ;;
        --warnings-as-errors) WARNINGS_AS_ERRORS=1 ;;
        --sanitize) SANITIZE=1 ;;
        --epochgui)
            shift
            EPOCHGUI_PATH="${1:?--epochgui requires a path}"
            ;;
        --epochplatform)
            shift
            EPOCHPLATFORM_PATH="${1:?--epochplatform requires a path}"
            ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

if [[ "$CONFIGURATION" != "Debug" && "$CONFIGURATION" != "Release" && "$CONFIGURATION" != "RelWithDebInfo" ]]; then
    echo "Configuration must be Debug, Release, or RelWithDebInfo" >&2
    exit 2
fi

BUILD_FLAVOR="vulkan"
VULKAN=ON
if [[ $CPU_ONLY -eq 1 ]]; then
    BUILD_FLAVOR="cpu"
    VULKAN=OFF
fi
if [[ $SHARED -eq 1 ]]; then
    BUILD_FLAVOR+="-shared"
fi
BUILD_DIR="$ROOT/out/build/ninja-${BUILD_FLAVOR}-${CONFIGURATION,,}"

if [[ $CLEAN -eq 1 ]]; then
    rm -rf "$BUILD_DIR"
fi

CMAKE_ARGS=(
    -S "$ROOT"
    -B "$BUILD_DIR"
    -G Ninja
    "-DCMAKE_BUILD_TYPE=$CONFIGURATION"
    "-DEPOCH_PARTICLE_BUILD_VULKAN=$VULKAN"
    "-DEPOCH_PARTICLE_BUILD_SHARED=$([[ $SHARED -eq 1 ]] && echo ON || echo OFF)"
    -DEPOCH_PARTICLE_BUILD_EXAMPLES=ON
    "-DEPOCH_PARTICLE_BUILD_TESTS=$([[ $SKIP_TESTS -eq 1 ]] && echo OFF || echo ON)"
    "-DEPOCH_PARTICLE_WARNINGS_AS_ERRORS=$([[ $WARNINGS_AS_ERRORS -eq 1 ]] && echo ON || echo OFF)"
    "-DEPOCH_PARTICLE_ENABLE_SANITIZERS=$([[ $SANITIZE -eq 1 ]] && echo ON || echo OFF)"
)

if [[ $CPU_ONLY -eq 0 && -n "${VCPKG_ROOT:-}" && -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
    CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake")
fi

if [[ -n "$EPOCHGUI_PATH" ]]; then
    CMAKE_ARGS+=(
        -DEPOCH_PARTICLE_WITH_EPOCHGUI=ON
        "-DEPOCHGUI_SOURCE_DIR=$(cd "$EPOCHGUI_PATH" && pwd)"
    )
fi

if [[ $CPU_ONLY -eq 0 ]]; then
    if [[ -z "$EPOCHPLATFORM_PATH" && -f "$ROOT/../EpochPlatformEngine/CMakeLists.txt" ]]; then
        EPOCHPLATFORM_PATH="$ROOT/../EpochPlatformEngine"
    fi
    if [[ -n "$EPOCHPLATFORM_PATH" ]]; then
        CMAKE_ARGS+=("-DEPOCH_PLATFORM_SOURCE_DIR=$(cd "$EPOCHPLATFORM_PATH" && pwd)")
    fi
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel
if [[ $SKIP_TESTS -eq 0 ]]; then
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "Build complete: $BUILD_DIR"
