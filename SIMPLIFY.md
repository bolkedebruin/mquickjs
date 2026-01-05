# Simplify mquickjs changes - sync with upstream

Goal: Minimize diff with upstream (bellard/mquickjs) while keeping only necessary firmware changes.

## Changes to KEEP (necessary for chunked relocation)

### mquickjs.c
1. **`JS_RelocateHdr`** - NEW function, split out for chunked header relocation
2. **`JS_RelocateMtag`** - NEW function, split out for chunked block relocation
3. **`bc_reloc_value`** - Rewritten to relocate FIRST before dereferencing (needed for streaming from position-independent bytecode where val is offset, not pointer)
4. **`JS_RelocateBytecode2`** - Refactored to use JS_RelocateHdr/JS_RelocateMtag
5. **`JS_GetMemBlockSize`** - Public API wrapper for get_mblock_size (used by BytecodeWriter)
6. **`JS_GetMemBlockMTag`** - Public API for getting memory block tag (used by BytecodeWriter)
7. **PRIu32/PRId32 format specifiers** - Better cross-platform compatibility than upstream

### mquickjs.h
1. **`#include <stddef.h>`** - Needed for size_t
2. **`JS_SetConsoleWriteFunc`, `JS_ProcessTimers`** - For REPL/console support
3. **`JS_GetMemBlockSize`, `JS_GetMemBlockMTag`** declarations
4. **`JSFunctionBytecodePublic`, `JSValueArrayPublic`** - Public structs for external relocation

## Changes IMPLEMENTED (all complete)

### emscripten_wrapper.c
1. ✅ **Handle `use_32bit` parameter** - Same as mqjs.c: when `use_32bit` is true and JSW == 8, use `JS_PrepareBytecode64to32()` instead of `JS_PrepareBytecode()` to generate 32-bit bytecode for ESP32 target

### mquickjs.h
1. ✅ **Removed `JS_BYTECODE_VERSION_32_V1`** - Using upstream's version check
2. ✅ **Removed `reserved1`/`reserved2` fields from JSBytecodeHeader** - Reverted to upstream
3. ✅ **Removed `reserved1`/`reserved2` fields from JSBytecodeHeader32** - Reverted to upstream

### mquickjs.c
1. ✅ **Updated JS_RelocateHdr** - Simplified version check to match upstream
2. ✅ **Updated JS_LoadBytecode** - Simplified version check to match upstream
3. ✅ **Removed JS_GetContextRomAtomTableCount** - Not used
4. ✅ **Removed reserved field initialization in JS_PrepareBytecode**
5. ✅ **Removed reserved field initialization in JS_PrepareBytecode64to32**

## Build Status

- ✅ ESP32 build: SUCCESS
- ✅ WASM build: SUCCESS
