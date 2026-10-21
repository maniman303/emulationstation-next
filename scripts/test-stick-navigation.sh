#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/stick-navigation-test.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM
SDL_CFLAGS=$(pkg-config --cflags sdl2)

"${CXX:-c++}" $SDL_CFLAGS -std=c++17 -ffunction-sections -fdata-sections \
    -Iexternal -Ies-core/src tests/stick-navigation.cpp es-core/src/InputConfig.cpp \
    -Wl,--gc-sections -o "$BUILD_DIR/stick-navigation"
"$BUILD_DIR/stick-navigation"
