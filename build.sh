#!/bin/bash
set -e

emcc main.c -O3 -flto -o jpegxl.js \
    -I../jpeg-xl/build-wasm32/install/include \
    -L../jpeg-xl/build-wasm32/install/lib \
    -L../jpeg-xl/build-wasm32/third_party \
    -L../jpeg-xl/build-wasm32/third_party/brotli \
    -ljxl -ljxl_threads -lhwy -lskcms -lbrotlicommon-static -lbrotlidec-static -lbrotlienc-static \
    -sEXTRA_EXPORTED_RUNTIME_METHODS='["cwrap"]' -sALLOW_MEMORY_GROWTH=1 \
    --no-entry
wasm-opt -o jpegxl.wasm -O4 --disable-simd jpegxl.wasm

# clang main.c -g -o jpegxl \
#     -I../jpeg-xl/build/install/usr/local/include \
#     -Ivendor \
#     -L../jpeg-xl/build/install/usr/local/lib \
#     -ljxl -ljxl_threads -lhwy -lskcms
