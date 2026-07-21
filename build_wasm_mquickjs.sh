#!/bin/bash
# Build the mquickjs compiler as a WebAssembly module (engine-only).
#
# This builds a generic mquickjs -> bytecode compiler against the example
# standard library (example_stdlib.c). It contains NO FreeButton-specific
# bindings: those live in the FreeButton firmware repo (src/scripting/js/),
# which builds its own WASM compiler with the FreeButton stdlib.

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "========================================"
echo "Building mquickjs compiler for WebAssembly (engine-only)"
echo "========================================"

# Ensure Emscripten is installed
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten not found. Install from: https://emscripten.org/"
    echo ""
    echo "Quick install:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo ""
echo "Emscripten version:"
emcc --version | head -1
echo ""

# Step 1: Generate the stdlib + atom headers (32-bit) from the example stdlib
echo "Step 1: Generating stdlib headers..."

if [ ! -f example_stdlib ]; then
    echo "Building example_stdlib generator..."
    gcc -O2 -Wall -D_GNU_SOURCE -c mquickjs_build.c -o mquickjs_build.host.o
    gcc -O2 -Wall -D_GNU_SOURCE -c example_stdlib.c -o example_stdlib.host.o
    gcc example_stdlib.host.o mquickjs_build.host.o -o example_stdlib
fi

echo "Generating 32-bit stdlib headers..."
./example_stdlib -m32 > example_stdlib.h
./example_stdlib -a -m32 > mquickjs_atom.h

echo "✓ Generated example_stdlib.h and mquickjs_atom.h"
echo ""

# Step 2: Compile to WebAssembly
echo "Step 2: Compiling to WebAssembly..."

# example.c defines the js_stdlib referenced by emscripten_wrapper.c
# (via example_stdlib.h) along with the example class implementations.
SOURCES="
    mquickjs.c
    cutils.c
    dtoa.c
    libm.c
    emscripten_wrapper.c
    example.c
"

# Export functions that will be called from JavaScript
EXPORTED_FUNCTIONS='["_compile_js_to_bytecode", "_get_bytecode_buffer", "_get_error_message", "_get_bytecode_size", "_malloc", "_free"]'

# Export runtime methods needed by JavaScript wrapper
EXPORTED_RUNTIME='["ccall", "cwrap", "HEAPU8", "UTF8ToString", "stringToUTF8"]'

echo "Compiling with Emscripten..."
echo ""

# Build to WebAssembly
emcc \
    $SOURCES \
    -o mquickjs.js \
    -I. \
    -s WASM=1 \
    -s EXPORTED_FUNCTIONS="$EXPORTED_FUNCTIONS" \
    -s EXPORTED_RUNTIME_METHODS="$EXPORTED_RUNTIME" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=16MB \
    -s MAXIMUM_MEMORY=64MB \
    -s STACK_SIZE=1MB \
    -s MODULARIZE=1 \
    -s EXPORT_NAME='createMQuickJSModule' \
    -s ENVIRONMENT='web,worker' \
    -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
    -O3 \
    -DEMSCRIPTEN \
    -D_GNU_SOURCE

echo ""
echo "✓ Build complete!"
echo ""
echo "Output files:"
ls -lh mquickjs.js mquickjs.wasm
echo ""
echo "Files ready for browser testing!"
