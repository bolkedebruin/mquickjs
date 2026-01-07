/**
 * Test for streaming bytecode relocation with atom lookup
 *
 * Tests the fix for the bug where bc_reloc_value was trying to dereference
 * val_relocated (the TARGET flash address) to read string content during
 * streaming relocation, but in streaming mode the data is still in the
 * source buffer, not yet written to the target address.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "../mquickjs.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// Track console.log calls to verify execution
static int g_print_call_count = 0;
static char g_last_print[256] = {0};

static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;
    g_print_call_count++;
    if (argc > 0) {
        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, argv[0], &buf);
        if (str) {
            strncpy(g_last_print, str, sizeof(g_last_print) - 1);
            g_last_print[sizeof(g_last_print) - 1] = '\0';
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_loadMapped(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_loadUserBytecode(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));
}

static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));
}

static JSValue js_led_init(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_led_rgb(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_led_on(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_led_off(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

// Now include the stdlib after stubs are defined
#include "../mqjs_stdlib.h"

// Test context for streaming read callback
typedef struct {
    uint8_t *data_buf;
    uint32_t data_len;
    uintptr_t original_base_addr;  // Base address from bytecode header
    uint32_t read_count;
} TestRelocContext;

// Context for restricted read callback (simulates firmware flash read)
typedef struct {
    uint8_t *data_buf;
    uint32_t data_len;
    uintptr_t original_base_addr;
    uint32_t read_count;
    uint8_t read_buffer[256];  // Fixed buffer like firmware uses
} RestrictedRelocContext;

// Read callback - converts absolute address to buffer offset and returns data
static int g_debug_callback = 0;

// Structure from mquickjs.c to check mtag
typedef struct {
    uint64_t mtag_and_flags;  // Contains mtag in bits
    uint8_t buf[];
} DebugJSString;

static uint8_t* test_reloc_read(void *opaque, uintptr_t addr, size_t size) {
    TestRelocContext *ctx = (TestRelocContext *)opaque;
    ctx->read_count++;

    // Convert absolute address to offset relative to original base
    if (addr >= ctx->original_base_addr) {
        uintptr_t offset = addr - ctx->original_base_addr;
        if (offset + size <= ctx->data_len) {
            uint8_t *result = ctx->data_buf + offset;
            if (g_debug_callback) {
                // Try to print what we found - first byte should have mtag
                DebugJSString *s = (DebugJSString *)result;
                int mtag = s->mtag_and_flags & 0x1F;  // Assuming 5-bit mtag
                printf("  read: offset=%lu mtag=%d first_bytes=%02x%02x%02x%02x\n",
                       (unsigned long)offset, mtag,
                       result[0], result[1], result[2], result[3]);
            }
            return result;
        }
        if (g_debug_callback) {
            printf("  read: addr=0x%lx -> offset=%lu OUT OF BOUNDS (len=%u)\n",
                   (unsigned long)addr, (unsigned long)offset, ctx->data_len);
        }
    } else {
        if (g_debug_callback) {
            printf("  read: addr=0x%lx BELOW BASE (base=0x%lx)\n",
                   (unsigned long)addr, (unsigned long)ctx->original_base_addr);
        }
    }
    return NULL;  // Not accessible
}

// Restricted read callback - copies ONLY the requested bytes into a separate buffer
// This simulates firmware behavior where flash reads go into a fixed buffer
static uint8_t* restricted_reloc_read(void *opaque, uintptr_t addr, size_t size) {
    RestrictedRelocContext *ctx = (RestrictedRelocContext *)opaque;
    ctx->read_count++;

    if (size > sizeof(ctx->read_buffer)) {
        return NULL;  // Too large
    }

    // Convert absolute address to offset relative to original base
    if (addr >= ctx->original_base_addr) {
        uintptr_t offset = addr - ctx->original_base_addr;
        if (offset + size <= ctx->data_len) {
            // Fill buffer with garbage first to catch out-of-bounds reads
            memset(ctx->read_buffer, 0xDE, sizeof(ctx->read_buffer));
            // Copy ONLY the requested bytes
            memcpy(ctx->read_buffer, ctx->data_buf + offset, size);
            return ctx->read_buffer;
        }
    }
    return NULL;
}

// Test: Streaming relocation with read callback and execution
static int test_streaming_relocation(void) {
    printf("  test_streaming_relocation: ");

    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) {
        printf("FAIL (malloc heap)\n");
        return -1;
    }

    // Compilation context
    JSContext *ctx = JS_NewContext2(heap, heap_size, &js_stdlib, TRUE);
    if (!ctx) {
        printf("FAIL (JS_NewContext2)\n");
        free(heap);
        return -1;
    }

    // Compile a script that uses stdlib globals (console, log)
    const char *source = "console.log('streaming_test');";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) {
        printf("FAIL (JS_Parse)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    // Prepare bytecode
    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    // Allocate bytecode buffer (header + data)
    uint32_t total_len = sizeof(JSBytecodeHeader) + data_len;
    uint8_t *bc_buf = malloc(total_len);
    if (!bc_buf) {
        printf("FAIL (malloc bc_buf)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    memcpy(bc_buf, &hdr, sizeof(JSBytecodeHeader));
    memcpy(bc_buf + sizeof(JSBytecodeHeader), data_buf, data_len);

    JS_FreeContext(ctx);
    free(heap);

    // Runtime context for relocation and execution
    uint8_t *runtime_heap = malloc(heap_size);
    if (!runtime_heap) {
        printf("FAIL (malloc runtime_heap)\n");
        free(bc_buf);
        return -1;
    }
    JSContext *runtime_ctx = JS_NewContext(runtime_heap, heap_size, &js_stdlib);
    if (!runtime_ctx) {
        printf("FAIL (runtime JS_NewContext)\n");
        free(runtime_heap);
        free(bc_buf);
        return -1;
    }

    // Get the header
    JSBytecodeHeader *hdr_ptr = (JSBytecodeHeader *)bc_buf;
    uint8_t *data_section = bc_buf + sizeof(JSBytecodeHeader);

    // Setup streaming context - points to data section after header
    TestRelocContext reloc_ctx = {
        .data_buf = data_section,
        .data_len = data_len,
        .original_base_addr = hdr_ptr->base_addr,  // Original base from compilation
        .read_count = 0
    };

    g_debug_callback = 0;  // Disable debug output for normal testing

    // Target address - use actual data section address for execution
    uintptr_t target_addr = (uintptr_t)data_section;

    // Test streaming relocation with read callback
    int result = JS_RelocateBytecode2Indirect(
        runtime_ctx, hdr_ptr, data_section, data_len,
        target_addr, TRUE,
        test_reloc_read, &reloc_ctx
    );

    g_debug_callback = 0;

    if (result != 0) {
        printf("FAIL (JS_RelocateBytecode2Indirect=%d)\n", result);
        JS_FreeContext(runtime_ctx);
        free(runtime_heap);
        free(bc_buf);
        return -1;
    }

    // Verify callback was used
    if (reloc_ctx.read_count == 0) {
        printf("FAIL (read_count=0, callback not used)\n");
        JS_FreeContext(runtime_ctx);
        free(runtime_heap);
        free(bc_buf);
        return -1;
    }

    // Reset print tracking
    g_print_call_count = 0;
    g_last_print[0] = '\0';

    // Load the bytecode
    JSValue loaded = JS_LoadBytecode(runtime_ctx, bc_buf);
    if (JS_IsException(loaded)) {
        JSValue exc = JS_GetException(runtime_ctx);
        JSCStringBuf exc_buf;
        const char *exc_str = JS_ToCString(runtime_ctx, exc, &exc_buf);
        printf("FAIL (JS_LoadBytecode exception: %s)\n", exc_str ? exc_str : "(null)");
        JS_FreeContext(runtime_ctx);
        free(runtime_heap);
        free(bc_buf);
        return -1;
    }

    // Execute the loaded bytecode
    JSValue exec_result = JS_Run(runtime_ctx, loaded);
    int exec_exception = JS_IsException(exec_result);

    // Print exception details if any
    if (exec_exception) {
        JSValue exc = JS_GetException(runtime_ctx);
        JSCStringBuf exc_buf;
        const char *exc_str = JS_ToCString(runtime_ctx, exc, &exc_buf);
        printf("Exception: %s\n", exc_str ? exc_str : "(null)");
    }

    // Check if console.log was called with correct argument
    int success = !exec_exception && (g_print_call_count == 1) &&
                  (strcmp(g_last_print, "streaming_test") == 0);

    JS_FreeContext(runtime_ctx);
    free(runtime_heap);
    free(bc_buf);

    if (success) {
        printf("PASS (read_count=%u, output='%s')\n", reloc_ctx.read_count, g_last_print);
        return 0;
    } else {
        printf("FAIL (exec_exception=%d, read_count=%u, output='%s')\n",
               exec_exception, reloc_ctx.read_count, g_last_print);
        return -1;
    }
}

// Test: Standard relocation with bytecode execution
static int test_standard_relocation(void) {
    printf("  test_standard_relocation: ");

    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) {
        printf("FAIL (malloc)\n");
        return -1;
    }

    // Compilation context
    JSContext *ctx = JS_NewContext2(heap, heap_size, &js_stdlib, TRUE);
    if (!ctx) {
        printf("FAIL (JS_NewContext2)\n");
        free(heap);
        return -1;
    }

    const char *source = "console.log('hello_reloc');";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) {
        printf("FAIL (JS_Parse)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    // Allocate bytecode buffer (header + data)
    uint32_t total_len = sizeof(JSBytecodeHeader) + data_len;
    uint8_t *bc_buf = malloc(total_len);
    if (!bc_buf) {
        printf("FAIL (malloc bc_buf)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    memcpy(bc_buf, &hdr, sizeof(JSBytecodeHeader));
    memcpy(bc_buf + sizeof(JSBytecodeHeader), data_buf, data_len);

    JS_FreeContext(ctx);
    free(heap);

    // Runtime context for execution
    uint8_t *runtime_heap = malloc(heap_size);
    if (!runtime_heap) {
        printf("FAIL (malloc runtime_heap)\n");
        free(bc_buf);
        return -1;
    }
    JSContext *runtime_ctx = JS_NewContext(runtime_heap, heap_size, &js_stdlib);
    if (!runtime_ctx) {
        printf("FAIL (runtime JS_NewContext)\n");
        free(runtime_heap);
        free(bc_buf);
        return -1;
    }

    // Relocate bytecode to its actual address
    int result = JS_RelocateBytecode(runtime_ctx, bc_buf, total_len);
    if (result != 0) {
        printf("FAIL (JS_RelocateBytecode=%d)\n", result);
        JS_FreeContext(runtime_ctx);
        free(runtime_heap);
        free(bc_buf);
        return -1;
    }

    // Reset print tracking
    g_print_call_count = 0;
    g_last_print[0] = '\0';

    // Load the bytecode
    JSValue loaded = JS_LoadBytecode(runtime_ctx, bc_buf);
    if (JS_IsException(loaded)) {
        printf("FAIL (JS_LoadBytecode exception)\n");
        JS_FreeContext(runtime_ctx);
        free(runtime_heap);
        free(bc_buf);
        return -1;
    }

    // Execute the loaded bytecode
    JSValue exec_result = JS_Run(runtime_ctx, loaded);
    int exec_exception = JS_IsException(exec_result);

    // Check if console.log was called with correct argument
    int success = !exec_exception && (g_print_call_count == 1) &&
                  (strcmp(g_last_print, "hello_reloc") == 0);

    JS_FreeContext(runtime_ctx);
    free(runtime_heap);
    free(bc_buf);

    if (success) {
        printf("PASS (executed, print_count=%d, output='%s')\n", g_print_call_count, g_last_print);
        return 0;
    } else {
        printf("FAIL (exec_exception=%d, print_count=%d, output='%s')\n",
               exec_exception, g_print_call_count, g_last_print);
        return -1;
    }
}

// Test: Position-independent output (base_addr=0)
static int test_position_independent(void) {
    printf("  test_position_independent: ");

    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) {
        printf("FAIL (malloc)\n");
        return -1;
    }

    JSContext *ctx = JS_NewContext2(heap, heap_size, &js_stdlib, TRUE);
    if (!ctx) {
        printf("FAIL (JS_NewContext2)\n");
        free(heap);
        return -1;
    }

    const char *source = "var x = 1 + 2;";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) {
        printf("FAIL (JS_Parse)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    uint8_t *data_copy = malloc(data_len);
    if (!data_copy) {
        printf("FAIL (malloc data)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    memcpy(data_copy, data_buf, data_len);

    JSBytecodeHeader hdr_copy;
    memcpy(&hdr_copy, &hdr, sizeof(hdr));

    // Relocate to base_addr=0 (position-independent)
    int result = JS_RelocateBytecode2(ctx, &hdr_copy, data_copy, data_len, 0, FALSE);

    int success = (result == 0) && (hdr_copy.base_addr == 0);

    JS_FreeContext(ctx);
    free(data_copy);
    free(heap);

    if (success) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL (result=%d, base_addr=%lu)\n", result, (unsigned long)hdr_copy.base_addr);
        return -1;
    }
}

// Test: Verify atom replacement in relocated bytecode
static int test_atom_replacement(void) {
    printf("  test_atom_replacement: ");

    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) {
        printf("FAIL (malloc)\n");
        return -1;
    }

    JSContext *ctx = JS_NewContext2(heap, heap_size, &js_stdlib, TRUE);
    if (!ctx) {
        printf("FAIL (JS_NewContext2)\n");
        free(heap);
        return -1;
    }

    // Script that uses common stdlib atoms
    const char *source = "console.log(JSON.stringify({a:1}));";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) {
        printf("FAIL (JS_Parse)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    // Make copy for streaming relocation
    uint8_t *data_copy = malloc(data_len);
    if (!data_copy) {
        printf("FAIL (malloc data)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    memcpy(data_copy, data_buf, data_len);
    JSBytecodeHeader hdr_copy;
    memcpy(&hdr_copy, &hdr, sizeof(hdr));

    // Setup streaming context
    TestRelocContext reloc_ctx = {
        .data_buf = data_copy,
        .data_len = data_len,
        .original_base_addr = hdr.base_addr,  // Original base from compilation
        .read_count = 0
    };

    // Perform streaming relocation with atom update
    uintptr_t target_addr = 0x3C200000;
    int result = JS_RelocateBytecode2Indirect(
        ctx, &hdr_copy, data_copy, data_len,
        target_addr, TRUE,
        test_reloc_read, &reloc_ctx
    );

    // Verify:
    // 1. Relocation succeeded
    // 2. Read callback was used (atoms were looked up)
    int success = (result == 0) && (reloc_ctx.read_count > 0);

    JS_FreeContext(ctx);
    free(data_copy);
    free(heap);

    if (success) {
        printf("PASS (atoms accessed via callback: %u times)\n", reloc_ctx.read_count);
        return 0;
    } else {
        printf("FAIL (result=%d, read_count=%u)\n", result, reloc_ctx.read_count);
        return -1;
    }
}

// Test: Restricted buffer read (simulates firmware flash behavior)
// This test would FAIL if bc_reloc_value only reads sizeof(JSString)
// without the two-step read for full string content
static int test_restricted_buffer_read(void) {
    printf("  test_restricted_buffer_read: ");

    const size_t heap_size = 32 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) {
        printf("FAIL (malloc heap)\n");
        return -1;
    }

    JSContext *ctx = JS_NewContext(heap, heap_size, &js_stdlib);
    if (!ctx) {
        printf("FAIL (JS_NewContext)\n");
        free(heap);
        return -1;
    }

    // Script that uses atoms that need to be looked up
    const char *source = "console.log('test_restricted');";
    JSValue val = JS_Parse(ctx, source, strlen(source), "<test>", 0);
    if (JS_IsException(val)) {
        printf("FAIL (JS_Parse)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    JSBytecodeHeader hdr;
    const uint8_t *data_buf;
    uint32_t data_len;
    JS_PrepareBytecode(ctx, &hdr, &data_buf, &data_len, val);

    // Make copy for relocation
    uint8_t *data_copy = malloc(data_len);
    if (!data_copy) {
        printf("FAIL (malloc data)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    memcpy(data_copy, data_buf, data_len);
    JSBytecodeHeader hdr_copy;
    memcpy(&hdr_copy, &hdr, sizeof(hdr));

    // Setup restricted context - uses a separate read buffer like firmware
    RestrictedRelocContext reloc_ctx;
    reloc_ctx.data_buf = data_copy;
    reloc_ctx.data_len = data_len;
    reloc_ctx.original_base_addr = hdr.base_addr;
    reloc_ctx.read_count = 0;
    memset(reloc_ctx.read_buffer, 0, sizeof(reloc_ctx.read_buffer));

    // Perform streaming relocation with restricted read callback
    uintptr_t target_addr = 0x3C200000;
    int result = JS_RelocateBytecode2Indirect(
        ctx, &hdr_copy, data_copy, data_len,
        target_addr, TRUE,
        restricted_reloc_read, &reloc_ctx
    );

    int success = (result == 0) && (reloc_ctx.read_count > 0);

    JS_FreeContext(ctx);
    free(data_copy);
    free(heap);

    if (success) {
        printf("PASS (restricted reads: %u)\n", reloc_ctx.read_count);
        return 0;
    } else {
        printf("FAIL (result=%d, read_count=%u)\n", result, reloc_ctx.read_count);
        return -1;
    }
}

static int test_const_keyword(void) {
    printf("  test_const_keyword: ");

    const size_t heap_size = 128 * 1024;
    uint8_t *heap = malloc(heap_size);
    if (!heap) {
        printf("FAIL (malloc)\n");
        return -1;
    }

    JSContext *ctx = JS_NewContext(heap, heap_size, &js_stdlib);
    if (!ctx) {
        printf("FAIL (JS_NewContext)\n");
        free(heap);
        return -1;
    }

    // Test -1: sanity check - simple var works
    g_print_call_count = 0;
    const char *source_sanity = "var y = 42; console.log(y);";
    JSValue val_sanity = JS_Parse(ctx, source_sanity, strlen(source_sanity), "<sanity>", 0);
    if (JS_IsException(val_sanity)) {
        printf("FAIL (JS_Parse var sanity)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    JSValue result_sanity = JS_Run(ctx, val_sanity);
    if (JS_IsException(result_sanity)) {
        printf("FAIL (JS_Run var sanity)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    if (g_print_call_count != 1 || strcmp(g_last_print, "42") != 0) {
        printf("FAIL (var sanity: expected '42', got '%s', count=%d)\n", g_last_print, g_print_call_count);
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    // Test 0: just parse const (no execution)
    const char *source0 = "const a = 1;";
    JSValue val0 = JS_Parse(ctx, source0, strlen(source0), "<const0>", 0);
    if (JS_IsException(val0)) {
        printf("FAIL (JS_Parse simple const)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    // Test 1: const declaration and access (using small int 1)
    g_print_call_count = 0;
    const char *source1 = "const x = 1; console.log(x);";
    JSValue val1 = JS_Parse(ctx, source1, strlen(source1), "<const1>", 0);
    if (JS_IsException(val1)) {
        printf("FAIL (JS_Parse const declaration)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    JSValue result1 = JS_Run(ctx, val1);
    if (JS_IsException(result1)) {
        printf("FAIL (JS_Run const declaration)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    if (g_print_call_count != 1 || strcmp(g_last_print, "1") != 0) {
        printf("FAIL (expected '1', got '%s', count=%d)\n", g_last_print, g_print_call_count);
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    // Test 2: const with string
    g_print_call_count = 0;
    const char *source2 = "const s = 'hello'; console.log(s);";
    JSValue val2 = JS_Parse(ctx, source2, strlen(source2), "<const2>", 0);
    if (JS_IsException(val2)) {
        printf("FAIL (JS_Parse const string)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    JSValue result2 = JS_Run(ctx, val2);
    if (JS_IsException(result2)) {
        printf("FAIL (JS_Run const string)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    if (g_print_call_count != 1 || strcmp(g_last_print, "hello") != 0) {
        printf("FAIL (expected 'hello', got '%s')\n", g_last_print);
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    // Test 3: const in function scope
    g_print_call_count = 0;
    const char *source3 = "function f() { const y = 100; return y; } console.log(f());";
    JSValue val3 = JS_Parse(ctx, source3, strlen(source3), "<const3>", 0);
    if (JS_IsException(val3)) {
        printf("FAIL (JS_Parse const in function)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    JSValue result3 = JS_Run(ctx, val3);
    if (JS_IsException(result3)) {
        printf("FAIL (JS_Run const in function)\n");
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }
    if (g_print_call_count != 1 || strcmp(g_last_print, "100") != 0) {
        printf("FAIL (expected '100', got '%s')\n", g_last_print);
        JS_FreeContext(ctx);
        free(heap);
        return -1;
    }

    JS_FreeContext(ctx);
    free(heap);
    printf("PASS\n");
    return 0;
}

int main(int argc, char **argv) {
    int failures = 0;

    printf("Running relocation tests...\n");

    if (test_streaming_relocation() != 0) failures++;
    if (test_standard_relocation() != 0) failures++;
    if (test_position_independent() != 0) failures++;
    if (test_atom_replacement() != 0) failures++;
    if (test_restricted_buffer_read() != 0) failures++;
    if (test_const_keyword() != 0) failures++;

    printf("\n%d test(s) failed\n", failures);
    return failures;
}
