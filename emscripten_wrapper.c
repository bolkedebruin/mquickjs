/*
 * Emscripten Wrapper for MicroQuickJS Compiler
 *
 * Provides a C interface for compiling JavaScript to bytecode in the browser.
 * This is compiled to WebAssembly and called from JavaScript.
 */

#ifdef EMSCRIPTEN

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <emscripten.h>
#include "mquickjs.h"

// Forward declaration of js_stdlib (defined in stdlib_export.c)
extern const JSSTDLibraryDef js_stdlib;

// Maximum bytecode size (1MB should be plenty)
#define MAX_BYTECODE_SIZE (1024 * 1024)

// Global buffer for bytecode output
static uint8_t bytecode_buffer[MAX_BYTECODE_SIZE];
static size_t bytecode_size = 0;

// Error message buffer
static char error_message[1024];

/**
 * Compile JavaScript source to bytecode
 *
 * @param source_code JavaScript source code (null-terminated)
 * @param source_len Length of source code
 * @param target_addr Target flash address for pre-relocation (0 = no relocation)
 * @param use_32bit Generate 32-bit bytecode (for ESP32)
 * @return Bytecode size on success, -1 on error
 */
EMSCRIPTEN_KEEPALIVE
int compile_js_to_bytecode(const char* source_code, size_t source_len,
                           uintptr_t target_addr, int use_32bit) {
    JSContext *ctx = NULL;
    uint8_t mem_buf[128 * 1024]; // 128KB heap for compilation
    JSValue result;
    const uint8_t *data_buf;
    uint32_t data_len;

    bytecode_size = 0;
    error_message[0] = '\0';

    // Initialize context with FreeButton stdlib
    // This includes led, button, sensor, mqtt APIs in the atom table
    ctx = JS_NewContext(mem_buf, sizeof(mem_buf), &js_stdlib);
    if (!ctx) {
        snprintf(error_message, sizeof(error_message), "Failed to create JS context");
        return -1;
    }

    // Parse JavaScript source (compile without running)
    // Use 0 for parse_flags (no special flags)
    result = JS_Parse(ctx, source_code, source_len, "<input>", 0);

    if (JS_IsException(result)) {
        // Extract error message
        JSValue exception = JS_GetException(ctx);
        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, exception, &buf);
        if (str) {
            snprintf(error_message, sizeof(error_message), "%s", str);
        } else {
            snprintf(error_message, sizeof(error_message), "Compilation failed");
        }
        JS_FreeContext(ctx);
        return -1;
    }

    // Prepare bytecode for serialization
#if JSW == 8
    if (use_32bit) {
        // Generate 32-bit bytecode on 64-bit host
        JSBytecodeHeader32 hdr32;

        if (JS_PrepareBytecode64to32(ctx, &hdr32, &data_buf, &data_len, result)) {
            snprintf(error_message, sizeof(error_message), "Failed to convert bytecode to 32-bit");
            JS_FreeContext(ctx);
            return -1;
        }

        // For 32-bit, we need to handle relocation differently
        // For now, just copy header and data without relocation
        size_t hdr_size = sizeof(JSBytecodeHeader32);
        size_t total_size = hdr_size + data_len;

        if (total_size > MAX_BYTECODE_SIZE) {
            snprintf(error_message, sizeof(error_message),
                     "Bytecode too large: %zu bytes", total_size);
            JS_FreeContext(ctx);
            return -1;
        }

        memcpy(bytecode_buffer, &hdr32, hdr_size);
        memcpy(bytecode_buffer + hdr_size, data_buf, data_len);
        bytecode_size = total_size;
    } else
#endif
    {
        // Generate native-width bytecode
        JSBytecodeHeader hdr_buf;

        JS_PrepareBytecode(ctx, &hdr_buf, &data_buf, &data_len, result);

        // Pre-relocate if target address provided
        if (target_addr != 0) {
            // Relocate to target flash address
            // This enables zero-copy execution from flash
            int reloc_result = JS_RelocateBytecode2(
                ctx, &hdr_buf,
                (uint8_t*)data_buf, data_len,
                target_addr + sizeof(JSBytecodeHeader), // Data starts after header
                1 // Update atoms
            );

            if (reloc_result != 0) {
                snprintf(error_message, sizeof(error_message), "Relocation failed");
                JS_FreeContext(ctx);
                return -1;
            }
        } else {
            // Relocate to zero for deterministic output (standard approach)
            JS_RelocateBytecode2(ctx, &hdr_buf, (uint8_t*)data_buf, data_len, 0, 0);
        }

        // Copy to output buffer
        size_t hdr_size = sizeof(JSBytecodeHeader);
        size_t total_size = hdr_size + data_len;

        if (total_size > MAX_BYTECODE_SIZE) {
            snprintf(error_message, sizeof(error_message),
                     "Bytecode too large: %zu bytes", total_size);
            JS_FreeContext(ctx);
            return -1;
        }

        memcpy(bytecode_buffer, &hdr_buf, hdr_size);
        memcpy(bytecode_buffer + hdr_size, data_buf, data_len);
        bytecode_size = total_size;
    }

    JS_FreeContext(ctx);
    return (int)bytecode_size;
}

/**
 * Get pointer to compiled bytecode
 * Call after successful compile_js_to_bytecode()
 */
EMSCRIPTEN_KEEPALIVE
uint8_t* get_bytecode_buffer() {
    return bytecode_buffer;
}

/**
 * Get error message from last compilation
 */
EMSCRIPTEN_KEEPALIVE
const char* get_error_message() {
    return error_message;
}

/**
 * Get bytecode size from last compilation
 */
EMSCRIPTEN_KEEPALIVE
size_t get_bytecode_size() {
    return bytecode_size;
}

#endif // EMSCRIPTEN
