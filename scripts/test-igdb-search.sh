#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/igdb-search-test.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

"${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
    -Ies-app/src -Ies-core/src -Iexternal tests/igdb-search.cpp \
    es-app/src/scrapers/GameSearchNames.cpp -Wl,--gc-sections -o "$BUILD_DIR/igdb-search"
"$BUILD_DIR/igdb-search"
