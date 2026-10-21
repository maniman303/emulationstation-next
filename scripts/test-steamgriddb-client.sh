#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/steamgriddb-client-test.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Ies-app/src tests/steamgriddb-client.cpp \
    es-app/src/scrapers/SteamGridDBClient.cpp es-app/src/scrapers/GameSearchNames.cpp \
    -o "$BUILD_DIR/steamgriddb-client"
"$BUILD_DIR/steamgriddb-client"
