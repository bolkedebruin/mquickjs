#!/bin/bash
# Build mquickjs compiler as WebAssembly module with FreeButton stdlib
# This ensures the compiler knows about led, button, sensor, mqtt APIs

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "========================================"
echo "Building mquickjs compiler for WebAssembly"
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

# Step 1: Generate FreeButton stdlib headers (32-bit for ESP32)
echo "Step 1: Generating FreeButton stdlib headers..."

# Check if freebutton_stdlib executable exists
if [ ! -f freebutton_stdlib ]; then
    echo "Building freebutton_stdlib generator..."

    # Compile stdlib builder as host executable
    gcc -O2 -Wall -D_GNU_SOURCE \
        -c mquickjs_build.c -o mquickjs_build.host.o

    gcc -O2 -Wall -D_GNU_SOURCE \
        -c freebutton_stdlib.c -o freebutton_stdlib.host.o

    gcc freebutton_stdlib.host.o mquickjs_build.host.o \
        -o freebutton_stdlib
fi

# Generate 32-bit headers (for ESP32)
echo "Generating 32-bit stdlib headers..."
./freebutton_stdlib -m32 > freebutton_stdlib.h
./freebutton_stdlib -a -m32 > mquickjs_atom.h

echo "✓ Generated freebutton_stdlib.h and mquickjs_atom.h"
echo ""

# Step 2: Compile to WebAssembly
echo "Step 2: Compiling to WebAssembly..."

# Source files needed for WASM compilation
SOURCES="
    mquickjs.c
    cutils.c
    dtoa.c
    libm.c
    emscripten_wrapper.c
    freebutton_stubs_wasm.c
    freebutton_stdlib.c
    stdlib_export.c
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
    -s MODULARIZE=1 \
    -s EXPORT_NAME='createMQuickJSModule' \
    -s ENVIRONMENT='web,worker' \
    -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
    -O3 \
    -DEMSCRIPTEN \
    -D_GNU_SOURCE \
    -DCONFIG_FREEBUTTON_LED \
    -DCONFIG_FREEBUTTON_BUTTON \
    -DCONFIG_FREEBUTTON_SENSOR \
    -DCONFIG_FREEBUTTON_MQTT

echo ""
echo "✓ Build complete!"
echo ""
echo "Output files:"
ls -lh mquickjs.js mquickjs.wasm
echo ""
echo "Files ready for browser testing!"
echo "Open the test HTML file in a browser to try it out."
