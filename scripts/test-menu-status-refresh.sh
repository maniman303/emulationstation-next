#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

CXX=${CXX:-c++}
SDL_CFLAGS=$(pkg-config --cflags sdl2)
BUILD_DIR=${TMPDIR:-/tmp}/menu-status-refresh-test-$$
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM
mkdir -p "$BUILD_DIR"

"$CXX" $SDL_CFLAGS -std=c++17 -pthread -w -ffunction-sections -fdata-sections \
    -Iexternal -Ies-core/src tests/menu-status-refresh.cpp \
    es-core/src/BackgroundStatusRefresh.cpp \
    es-core/src/CoveredSceneRenderScope.cpp \
    -Wl,--gc-sections -o "$BUILD_DIR/menu-status-refresh"
"$BUILD_DIR/menu-status-refresh"
