# MQuickJS Streaming Relocation Test Plan

## Overview

This document describes the plan for adding a test for the streaming bytecode relocation feature in mquickjs. The test verifies that the `bc_reloc_value` function correctly replaces bytecode atoms with ROM stdlib atoms when using a read callback (streaming mode).

## Background

### The Bug

When bytecode is relocated in streaming mode (e.g., streaming from a temp file to flash), `bc_reloc_value` was trying to dereference `val_relocated` (the TARGET flash address) to read string content. However, in streaming mode, the data is still in the source buffer, not yet written to the target address.

### The Fix

Added a `read_func` callback to `BCRelocState` that allows `bc_reloc_value` to read data from the correct location:
- In non-streaming mode: reads directly from the relocated pointer (existing behavior)
- In streaming mode: uses the callback to read from the source buffer

### Files Modified

1. `mquickjs.c`:
   - Added `BCRelocReadFunc` typedef and fields to `BCRelocState`
   - Modified `bc_reloc_value` to use the read callback when provided
   - Added `JS_RelocateBytecode2Streaming()` public function

2. `mquickjs.h`:
   - Added `JSRelocReadFunc` typedef
   - Added `JS_RelocateBytecode2Streaming()` declaration

## Test Implementation Plan

### Location

Create the test in `lib/mquickjs/tests/` directory using the existing mquickjs test infrastructure.

### Test File

Create `lib/mquickjs/tests/test_relocation.c`:

```c
/**
 * Test for streaming bytecode relocation with atom lookup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mquickjs.h"

// Use the standard mqjs stdlib (no platform dependencies)
extern const JSSTDLibraryDef js_stdlib;

// Test context for streaming read callback
typedef struct {
    uint8_t *data_buf;
    uint32_t data_len;
    uint32_t read_count;
} TestRelocContext;

// Read callback - returns data from buffer at given offset
static uint8_t* test_reloc_read(void *opaque, uintptr_t offset, size_t size) {
    TestRelocContext *ctx = (TestRelocContext *)opaque;
    ctx->read_count++;

    if (offset + size <= ctx->data_len) {
        return ctx->data_buf + offset;
    }
    return NULL;  // Not accessible
}

// Test: Streaming relocation with read callback
static int test_streaming_relocation(void) {
    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) return -1;

    // Create compilation context (populates unique_strings from stdlib)
    JSContext *ctx = JS_NewContext2(heap, heap_size, &js_stdlib, TRUE);
    if (!ctx) { free(heap); return -1; }

    // Compile a script that uses stdlib globals
    const char *source = "console.log('test');";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) {
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    // Prepare bytecode
    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    // Copy bytecode data
    uint8_t *data_copy = malloc(data_len);
    if (!data_copy) { JS_FreeContext(ctx); free(heap); return -1; }
    memcpy(data_copy, data_buf, data_len);

    JSBytecodeHeader hdr_copy;
    memcpy(&hdr_copy, &hdr, sizeof(hdr));

    // Create runtime context
    uint8_t *runtime_heap = malloc(heap_size);
    if (!runtime_heap) { free(data_copy); JS_FreeContext(ctx); free(heap); return -1; }
    JSContext *runtime_ctx = JS_NewContext(runtime_heap, heap_size, &js_stdlib);
    if (!runtime_ctx) { free(runtime_heap); free(data_copy); JS_FreeContext(ctx); free(heap); return -1; }

    // Setup streaming context
    TestRelocContext reloc_ctx = {
        .data_buf = data_copy,
        .data_len = data_len,
        .read_count = 0
    };

    // Target address (simulating flash)
    uintptr_t target_addr = 0x3C200000;

    // Perform streaming relocation
    int result = JS_RelocateBytecode2Streaming(
        runtime_ctx, &hdr_copy, data_copy, data_len,
        target_addr, TRUE,
        test_reloc_read, &reloc_ctx
    );

    int success = (result == 0) && (reloc_ctx.read_count > 0) &&
                  (hdr_copy.base_addr == target_addr);

    // Cleanup
    JS_FreeContext(runtime_ctx);
    JS_FreeContext(ctx);
    free(runtime_heap);
    free(data_copy);
    free(heap);

    return success ? 0 : -1;
}

// Test: Standard relocation (no callback)
static int test_standard_relocation(void) {
    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) return -1;

    JSContext *ctx = JS_NewContext2(heap, heap_size, &js_stdlib, TRUE);
    if (!ctx) { free(heap); return -1; }

    const char *source = "console.log('hello');";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) { JS_FreeContext(ctx); free(heap); return -1; }

    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    uint8_t *data_copy = malloc(data_len);
    if (!data_copy) { JS_FreeContext(ctx); free(heap); return -1; }
    memcpy(data_copy, data_buf, data_len);

    JSBytecodeHeader hdr_copy;
    memcpy(&hdr_copy, &hdr, sizeof(hdr));

    uint8_t *runtime_heap = malloc(heap_size);
    if (!runtime_heap) { free(data_copy); JS_FreeContext(ctx); free(heap); return -1; }
    JSContext *runtime_ctx = JS_NewContext(runtime_heap, heap_size, &js_stdlib);
    if (!runtime_ctx) { free(runtime_heap); free(data_copy); JS_FreeContext(ctx); free(heap); return -1; }

    uintptr_t target_addr = (uintptr_t)data_copy;
    int result = JS_RelocateBytecode2(runtime_ctx, &hdr_copy, data_copy, data_len, target_addr, TRUE);

    int success = (result == 0) && (hdr_copy.base_addr == target_addr);

    JS_FreeContext(runtime_ctx);
    JS_FreeContext(ctx);
    free(runtime_heap);
    free(data_copy);
    free(heap);

    return success ? 0 : -1;
}

// Test: Position-independent output (base_addr=0)
static int test_position_independent(void) {
    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) return -1;

    JSContext *ctx = JS_NewContext2(heap, heap_size, &js_stdlib, TRUE);
    if (!ctx) { free(heap); return -1; }

    const char *source = "var x = 1 + 2;";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) { JS_FreeContext(ctx); free(heap); return -1; }

    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    uint8_t *data_copy = malloc(data_len);
    if (!data_copy) { JS_FreeContext(ctx); free(heap); return -1; }
    memcpy(data_copy, data_buf, data_len);

    JSBytecodeHeader hdr_copy;
    memcpy(&hdr_copy, &hdr, sizeof(hdr));

    // Relocate to base_addr=0 (position-independent)
    int result = JS_RelocateBytecode2(ctx, &hdr_copy, data_copy, data_len, 0, FALSE);

    int success = (result == 0) && (hdr_copy.base_addr == 0);

    JS_FreeContext(ctx);
    free(data_copy);
    free(heap);

    return success ? 0 : -1;
}

int main(int argc, char **argv) {
    int failures = 0;

    printf("Running relocation tests...\n");

    printf("  test_streaming_relocation: ");
    if (test_streaming_relocation() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        failures++;
    }

    printf("  test_standard_relocation: ");
    if (test_standard_relocation() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        failures++;
    }

    printf("  test_position_independent: ");
    if (test_position_independent() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        failures++;
    }

    printf("\n%d tests failed\n", failures);
    return failures;
}
```

### Build Instructions

Add to mquickjs Makefile or create a test build script:

```bash
#!/bin/bash
# build_test_relocation.sh

# Generate stdlib first
./generate_stdlib.sh

# Compile test with standard mqjs stdlib
gcc -O2 -Wall -D_GNU_SOURCE \
    -I. \
    -o test_relocation \
    tests/test_relocation.c \
    mquickjs.c \
    cutils.c \
    dtoa.c \
    libm.c \
    mqjs_stdlib.c

# Run test
./test_relocation
```

### Dependencies

The test uses `mqjs_stdlib.c` which generates `mqjs_stdlib.h` containing:
- Standard JavaScript atoms (console, log, etc.)
- No platform-specific C function dependencies
- Pure data tables that work on any platform

### Test Cases

1. **test_streaming_relocation**: Verifies that when bytecode is relocated with a read callback:
   - The relocation succeeds (returns 0)
   - The read callback is invoked (read_count > 0)
   - The header base_addr is updated correctly

2. **test_standard_relocation**: Verifies standard (non-streaming) relocation still works:
   - The relocation succeeds
   - Atoms are replaced with ROM atoms (update_atoms=TRUE)

3. **test_position_independent**: Verifies position-independent output:
   - Relocating to base_addr=0 succeeds
   - Header reflects base_addr=0

### Integration with mqjs Test Suite

The mquickjs project already has JavaScript tests in `tests/`:
- `test_builtin.js`
- `test_language.js`
- `test_closure.js`

The relocation test should be added as a C-level unit test since it tests internal relocation mechanisms not exposed to JavaScript.

### Expected Output

```
Running relocation tests...
  test_streaming_relocation: PASS
  test_standard_relocation: PASS
  test_position_independent: PASS

0 tests failed
```

## Future Enhancements

1. Add test for atom replacement verification - confirm specific atoms (like "console") are replaced with ROM pointers
2. Add stress test with large bytecode
3. Add test for partial chunk scenarios (data split across chunks)
4. Integration test with actual flash simulation
