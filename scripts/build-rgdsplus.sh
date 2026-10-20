#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

for tool in cmake ninja aarch64-linux-gnu-gcc aarch64-linux-gnu-g++ aarch64-linux-gnu-readelf; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'Missing build tool: %s\n' "$tool" >&2
        exit 2
    }
done

PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
cmake -S . -B build/rgdsplus-aarch64 -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-multiarch-linux-gnu.cmake \
    -DFREETYPE_INCLUDE_DIR_ft2build=/usr/include/freetype2 \
    -DFREETYPE_INCLUDE_DIR_freetype2=/usr/include/freetype2 \
    -DFREETYPE_LIBRARY_RELEASE=/usr/lib/aarch64-linux-gnu/libfreetype.so \
    -DFREETYPE_LIBRARY_DEBUG=/usr/lib/aarch64-linux-gnu/libfreetype.so \
    -DFreeImage_INCLUDE_DIR=/usr/include \
    -DFreeImage_LIBRARY_REL=/usr/lib/aarch64-linux-gnu/libfreeimage.so \
    -DFreeImage_LIBRARY_DBG=/usr/lib/aarch64-linux-gnu/libfreeimage.so \
    -DSDLMIXER_INCLUDE_DIR=/usr/include/SDL2 \
    -DSDLMIXER_LIBRARY=/usr/lib/aarch64-linux-gnu/libSDL2_mixer.so \
    -DSDL2_INCLUDE_DIR=/usr/include/SDL2 \
    -DSDL2_LIBRARY=/usr/lib/aarch64-linux-gnu/libSDL2.so \
    -DSDL2MAIN_LIBRARY=SDL2main \
    -DVLC_INCLUDE_DIR=/usr/include/vlc \
    -DVLC_LIBRARIES=/usr/lib/aarch64-linux-gnu/libvlc.so \
    -DCURL_LIBRARY=/usr/lib/aarch64-linux-gnu/libcurl.so \
    -DCURL_INCLUDE_DIR=/usr/include/aarch64-linux-gnu \
    -DRAPIDJSON_INCLUDE_DIRS=/usr/include \
    -DALSA_INCLUDE_DIR=/usr/include \
    -DALSA_LIBRARY=/usr/lib/aarch64-linux-gnu/libasound.so

cmake --build build/rgdsplus-aarch64 --target emulationstation -j"${BUILD_JOBS:-2}"
file emulationstation
aarch64-linux-gnu-readelf -h emulationstation | grep -E 'Class:|Machine:'
