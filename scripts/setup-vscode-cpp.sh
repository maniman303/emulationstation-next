#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/rgdsplus-aarch64"}

case "$BUILD_DIR" in
	/*) ;;
	*) BUILD_DIR="$ROOT/$BUILD_DIR" ;;
esac

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
	printf 'No configured CMake build found at %s\n' "$BUILD_DIR" >&2
	printf 'Configure the project first, or pass the path to an existing build directory.\n' >&2
	exit 2
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

COMPILE_COMMANDS="$BUILD_DIR/compile_commands.json"
if [ ! -f "$COMPILE_COMMANDS" ]; then
	printf 'CMake did not generate %s\n' "$COMPILE_COMMANDS" >&2
	exit 1
fi

mkdir -p "$ROOT/.vscode"
cat > "$ROOT/.vscode/c_cpp_properties.json" <<EOF
{
  "configurations": [
    {
      "name": "RG DS Plus (AArch64)",
      "compileCommands": "$COMPILE_COMMANDS",
      "compilerPath": "/usr/bin/aarch64-linux-gnu-g++",
      "intelliSenseMode": "linux-gcc-arm64",
      "cStandard": "c17",
      "cppStandard": "c++17"
    }
  ],
  "version": 4
}
EOF

printf 'Wrote %s\n' "$ROOT/.vscode/c_cpp_properties.json"
printf 'Compile database: %s\n' "$COMPILE_COMMANDS"
