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
#include "mquickjs_priv.h"  /* For JS_VALUE_TO_PTR and JS_IsPtr macros */

// Forward declaration of js_stdlib (defined in stdlib_export.c)
extern const JSSTDLibraryDef js_stdlib;

// Maximum bytecode size (1MB should be plenty)
#define MAX_BYTECODE_SIZE (1024 * 1024)

// Global buffer for bytecode output
static uint8_t bytecode_buffer[MAX_BYTECODE_SIZE];
static size_t bytecode_size = 0;

// Error message buffer
static char error_message[1024];

/* ROM Atom Translation Table (v0x0002 bytecode) */
typedef struct {
    uint32_t offset;
    uint16_t rom_index;
} ROMAtomTableBuilder;

/* Comparison function for qsort - sort by offset */
static int compare_offset(const void* a, const void* b) {
    uint32_t off_a = ((ROMAtomTableBuilder*)a)->offset;
    uint32_t off_b = ((ROMAtomTableBuilder*)b)->offset;
    if (off_a < off_b) return -1;
    if (off_a > off_b) return 1;
    return 0;
}

/* Helper functions to get ROM table info - defined in mquickjs.c */
extern void* JS_GetStdlibAtomTable(JSContext* ctx);
extern uint32_t JS_GetRomAtomTableSize(const void* table);
extern JSValue JS_GetRomAtomEntry(const void* table, int index);

/* External functions for ROM collection */
extern void JS_SetRomCollectionBuffer(JSContext *ctx, void* buf, int* count);
extern void JS_ClearRomCollectionBuffer(JSContext *ctx);
extern int JS_RelocateBytecode2(JSContext *ctx, JSBytecodeHeader *hdr,
                                 uint8_t *buf, uint32_t buf_len,
                                 uintptr_t new_base_addr, int update_atoms);

/**
 * Build ROM atom table using proper memory tag traversal (like ESP32 does)
 * This ensures we only record ROM atoms at valid JSValue field offsets
 *
 * @param ctx QuickJS context (with stdlib loaded)
 * @param hdr Bytecode header
 * @param bytecode Bytecode data buffer
 * @param size Size of bytecode data
 * @param table Output table buffer
 * @param max_entries Maximum table entries
 * @return Number of ROM atoms found
 */
static int build_rom_atom_table(JSContext* ctx, JSBytecodeHeader* hdr, uint8_t* bytecode, uint32_t size,
                                 ROMAtomTableBuilder* table, int max_entries) {
    void* rom_table = JS_GetStdlibAtomTable(ctx);

    if (!rom_table) {
        printf("[WASM] No ROM table loaded, skipping ROM atom detection\n");
        return 0;
    }

    printf("[WASM] Using proper memory tag traversal to collect ROM atoms\n");

    // Set up ROM collection state
    int rom_count = 0;

    // Set the ROM collection buffer in the context
    JS_SetRomCollectionBuffer(ctx, table, &rom_count);

    // Traverse bytecode using proper memory tag structure
    // This calls bc_reloc_value() for each valid JSValue field
    printf("[WASM] Starting ROM collection pass...\n");
    int result = JS_RelocateBytecode2(ctx, hdr, bytecode, size, 0, 0);

    // Clear the ROM collection buffer
    JS_ClearRomCollectionBuffer(ctx);

    if (result != 0) {
        printf("[WASM] WARNING: ROM collection pass failed: %d\n", result);
        return 0;
    }

    printf("[WASM] ROM collection found %d ROM atoms\n", rom_count);

    // Sort by offset for binary search on ESP32
    if (rom_count > 0) {
        qsort(table, rom_count, sizeof(ROMAtomTableBuilder), compare_offset);
        printf("[WASM] Sorted %d ROM atoms by offset\n", rom_count);
    }

    return rom_count;
}

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

#ifdef EMSCRIPTEN
    printf("[WASM] compile_js_to_bytecode called: JSW=%d, use_32bit=%d, target_addr=0x%lx\n",
           JSW, use_32bit, (unsigned long)target_addr);
#endif

    // Create NORMAL context with stdlib (so APIs like 'led' are available)
    // ROM atoms WILL be created during compilation, but we'll use
    // update_atoms=1 in JS_RelocateBytecode2 to replace them with embedded strings
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

        size_t hdr_size = sizeof(JSBytecodeHeader32);
        size_t total_size = hdr_size + data_len;

        if (total_size > MAX_BYTECODE_SIZE) {
            snprintf(error_message, sizeof(error_message),
                     "Bytecode too large: %zu bytes", total_size);
            JS_FreeContext(ctx);
            return -1;
        }

        // Copy header and data to output buffer first
        memcpy(bytecode_buffer, &hdr32, hdr_size);
        memcpy(bytecode_buffer + hdr_size, data_buf, data_len);

        // Relocate the complete buffer (header + data) to embed atoms
        // This works the same way as the ESP32 firmware does at load time
        if (JS_RelocateBytecode(ctx, bytecode_buffer, total_size) != 0) {
            snprintf(error_message, sizeof(error_message), "32-bit bytecode relocation failed");
            JS_FreeContext(ctx);
            return -1;
        }

        bytecode_size = total_size;
#ifdef EMSCRIPTEN
        printf("[WASM] Used 64-to-32 conversion path, size=%zu\n", bytecode_size);
#endif
    } else
#endif
    {
        // Generate native-width bytecode
#ifdef EMSCRIPTEN
        printf("[WASM] Using native bytecode path (JSW=%d)\n", JSW);
#endif
        JSBytecodeHeader hdr_buf;

        JS_PrepareBytecode(ctx, &hdr_buf, &data_buf, &data_len, result);

#ifdef EMSCRIPTEN
        printf("[WASM] JS_PrepareBytecode returned: data_len=%u, data_buf=%p\n",
               data_len, (void*)data_buf);
        printf("[WASM] hdr_buf AFTER JS_PrepareBytecode:\n");
        printf("       base_addr=0x%lx\n", (unsigned long)hdr_buf.base_addr);
        printf("       unique_strings=0x%llx\n", (unsigned long long)hdr_buf.unique_strings);
        printf("       main_func=0x%llx\n", (unsigned long long)hdr_buf.main_func);
#endif

        // Copy to output buffer FIRST (before relocation)
        // JS_RelocateBytecode2 modifies the buffer in place, so we need writable memory
        size_t hdr_size = sizeof(JSBytecodeHeader);
        size_t total_size = hdr_size + data_len;

#ifdef EMSCRIPTEN
        printf("[WASM] hdr_size=%zu, total_size=%zu, MAX=%d\n",
               hdr_size, total_size, MAX_BYTECODE_SIZE);
#endif

        if (total_size > MAX_BYTECODE_SIZE) {
            snprintf(error_message, sizeof(error_message),
                     "Bytecode too large: %zu bytes", total_size);
            JS_FreeContext(ctx);
            return -1;
        }

        memcpy(bytecode_buffer, &hdr_buf, hdr_size);
        memcpy(bytecode_buffer + hdr_size, data_buf, data_len);

#ifdef EMSCRIPTEN
        printf("[WASM] Copied to bytecode_buffer: hdr at %p, data at %p\n",
               (void*)bytecode_buffer, (void*)(bytecode_buffer + hdr_size));
#endif

        // BUILD ROM ATOM TABLE AND RELOCATE TO base_addr=0
        // Use proper memory tag traversal (like ESP32) to find ROM atoms at valid offsets
        // This ALSO relocates the bytecode to base_addr=0 (position-independent)
        uint8_t *writable_data = bytecode_buffer + hdr_size;
        ROMAtomTableBuilder rom_atoms[256];

        int rom_atom_count = build_rom_atom_table(ctx, (JSBytecodeHeader*)bytecode_buffer,
                                                   writable_data, data_len,
                                                   rom_atoms, 256);

        printf("[WASM] Found %d ROM atoms and relocated to base_addr=0\n", rom_atom_count);

        // ROM collection already relocated to base_addr=0, skip second relocation
        // Bytecode is now position-independent with ROM atoms recorded in table

        // Write v0x0002 bytecode with ROM translation table
        if (rom_atom_count > 0) {
            // Update header to v0x0002
            JSBytecodeHeader* hdr = (JSBytecodeHeader*)bytecode_buffer;
            hdr->version = JS_BYTECODE_VERSION_32_V2;
            hdr->rom_atom_count = rom_atom_count;
            hdr->reserved = 0;

            // Calculate ROM table size
            size_t table_size = rom_atom_count * sizeof(ROMAtomEntry);

            // Check if we have enough space
            if (total_size + table_size > MAX_BYTECODE_SIZE) {
                snprintf(error_message, sizeof(error_message),
                         "Bytecode too large with ROM table: %zu bytes", total_size + table_size);
                JS_FreeContext(ctx);
                return -1;
            }

            // Shift bytecode data to make room for ROM table
            memmove(bytecode_buffer + hdr_size + table_size,
                    bytecode_buffer + hdr_size,
                    data_len);

            // Relocate ALL pointers (header + data) by +table_size to account for ROM table insertion
            // This updates unique_strings, main_func, AND all pointers inside the data section
            uint8_t *shifted_data = bytecode_buffer + hdr_size + table_size;
            hdr->base_addr = 0;  // Old base_addr

            printf("[WASM] Relocating pointers after ROM table insertion: +0x%zx bytes\n", table_size);
            if (JS_RelocateBytecode2(ctx, hdr, shifted_data, data_len, table_size, 0) != 0) {
                snprintf(error_message, sizeof(error_message), "Pointer relocation after ROM table insertion failed");
                JS_FreeContext(ctx);
                return -1;
            }

            // Reset to position-independent (base_addr=0) for final output
            hdr->base_addr = 0;

            // Write ROM table immediately after header
            ROMAtomEntry* table = (ROMAtomEntry*)(bytecode_buffer + hdr_size);
            for (int i = 0; i < rom_atom_count; i++) {
                // Offsets are relative to start of data section (after ROM table)
                table[i].bytecode_offset = rom_atoms[i].offset;
                table[i].rom_index = rom_atoms[i].rom_index;
                table[i].padding = 0;
            }

            // Dump ROM table for debugging
            printf("[WASM] === ROM Translation Table (%d entries) ===\n", rom_atom_count);
            for (int i = 0; i < rom_atom_count && i < 20; i++) {
                printf("[WASM]   [%2d] offset=0x%04x → rom_index=%u\n",
                       i, table[i].bytecode_offset, table[i].rom_index);
            }
            if (rom_atom_count > 20) {
                printf("[WASM]   ... (%d more entries)\n", rom_atom_count - 20);
            }
            printf("[WASM] ==========================================\n");

            bytecode_size = hdr_size + table_size + data_len;
            printf("[WASM] v0x0002 bytecode: header=%zu, rom_table=%zu, data=%u, total=%zu\n",
                   hdr_size, table_size, data_len, bytecode_size);
        } else {
            // No ROM atoms found, use v0x0001
            bytecode_size = total_size;
            printf("[WASM] v0x0001 bytecode (no ROM atoms): total=%zu\n", bytecode_size);
        }
    }

    JS_FreeContext(ctx);

#ifdef EMSCRIPTEN
    printf("[WASM] Compilation successful: bytecode_size=%zu, buffer_ptr=%p\n",
           bytecode_size, (void*)bytecode_buffer);
#endif

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
