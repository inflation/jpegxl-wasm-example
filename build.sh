#!/bin/bash
emcc main.c -O3 -g -o jpegxl.js \
    -I../jpeg-xl/build-wasm32/install/include \
    -L../jpeg-xl/build-wasm32/install/lib \
    -L../jpeg-xl/build-wasm32/third_party \
    -L../jpeg-xl/build-wasm32/third_party/brotli \
    -L../jpeg-xl/build-wasm32/artifacts \
    -ljxl -ljxl_threads -lhwy -lskcms \
    -sEXTRA_EXPORTED_RUNTIME_METHODS='["cwrap"]' -sALLOW_MEMORY_GROWTH=1 \
    -mnontrapping-fptoint \
    --no-entry

# clang main.c -O2 -g -o jpegxl \
#     -I../jpeg-xl/build/install/include \
#     -L../jpeg-xl/build/install/lib \
#     -L../jpeg-xl/build/third_party \
#     -L../jpeg-xl/build/third_party/brotli \
#     -ljxl -ljxl_threads -lhwy -lskcms \
#     -mnontrapping-fptoint
