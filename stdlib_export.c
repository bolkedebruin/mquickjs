// Export js_stdlib from the generated header
// Provide functions for REPL/embedded use (from mqjs.c)

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "mquickjs.h"

// Import the file system hardware abstraction layer
#include "../../src/scripting/file_hardware.h"
#include "../../src/scripting/file_hardware_mmap.h"

// Helper to get current time in milliseconds
static int64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

// GC function (from mqjs.c)
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    JS_GC(ctx);
    return JS_UNDEFINED;
}

// Load and evaluate a JavaScript file from LittleFS
// Supports both source (.js) and pre-compiled bytecode (.jsc)
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;
    const char *filename;
    JSCStringBuf buf_str;
    uint8_t *buf;
    int buf_len;
    JSValue ret;

    filename = JS_ToCString(ctx, argv[0], &buf_str);
    if (!filename)
        return JS_EXCEPTION;

    // Load file from LittleFS (with default size limits)
    buf = file_hw_load(filename, &buf_len, 0);
    if (!buf) {
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "failed to load file '%s'", filename);
    }

    // Check if it's pre-compiled bytecode
    if (JS_IsBytecode(buf, buf_len)) {
        // Bytecode: relocate and load (zero-copy after relocation)
        if (JS_RelocateBytecode(ctx, buf, buf_len) != 0) {
            free(buf);
            return JS_ThrowError(ctx, JS_CLASS_ERROR, "failed to relocate bytecode '%s'", filename);
        }
        ret = JS_LoadBytecode(ctx, buf);
        // Note: buf must remain allocated as long as the bytecode is used
        // In a production system, you'd track this and free it when the context is destroyed
        // For now, this is a memory leak but bytecode is typically loaded once at startup
    } else {
        // Source code: parse and evaluate (requires full source in memory)
        ret = JS_Eval(ctx, (const char *)buf, buf_len, filename, 0);
        // Source is copied during parsing, safe to free
        free(buf);
    }

    return ret;
}

// Timer support (from mqjs.c)
typedef struct {
    bool allocated;
    JSGCRef func;
    int64_t timeout; /* in ms */
} JSTimer;

#define MAX_TIMERS 16
static JSTimer js_timer_list[MAX_TIMERS];

JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;
    JSTimer *th;
    int delay, i;
    JSValue *pfunc;

    if (!JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "not a function");
    if (JS_ToInt32(ctx, &delay, argv[1]))
        return JS_EXCEPTION;
    for(i = 0; i < MAX_TIMERS; i++) {
        th = &js_timer_list[i];
        if (!th->allocated) {
            pfunc = JS_AddGCRef(ctx, &th->func);
            *pfunc = argv[0];
            th->timeout = get_time_ms() + delay;
            th->allocated = true;
            return JS_NewInt32(ctx, i);
        }
    }
    return JS_ThrowInternalError(ctx, "too many timers");
}

JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;
    int timer_id;
    JSTimer *th;

    if (JS_ToInt32(ctx, &timer_id, argv[0]))
        return JS_EXCEPTION;
    if (timer_id >= 0 && timer_id < MAX_TIMERS) {
        th = &js_timer_list[timer_id];
        if (th->allocated) {
            JS_DeleteGCRef(ctx, &th->func);
            th->allocated = false;
        }
    }
    return JS_UNDEFINED;
}

JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));
}

JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    // Print all arguments to log output (matching mqjs.c implementation)
    (void)this_val;

    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            putchar(' ');
        }

        // Convert all values to strings and print them
        JSCStringBuf buf;
        size_t len;
        const char *str = JS_ToCStringLen(ctx, &len, argv[i], &buf);
        if (str) {
            fwrite(str, 1, len, stdout);
        }
    }

    // Add newline
    putchar('\n');

    return JS_UNDEFINED;
}

JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt64(ctx, get_time_ms());
}

// Load bytecode from flash partition (zero-copy, memory-mapped)
// Usage: loadMapped("js_code", 0, 32768)  // partition, offset, size
JSValue js_loadMapped(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)this_val;

    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "loadMapped() requires 3 arguments: partition, offset, size");
    }

    // Get partition label
    JSCStringBuf buf_str;
    const char *partition = JS_ToCString(ctx, argv[0], &buf_str);
    if (!partition)
        return JS_EXCEPTION;

    // Get offset and size
    int offset, size;
    if (JS_ToInt32(ctx, &offset, argv[1]))
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &size, argv[2]))
        return JS_EXCEPTION;

    // Memory map the bytecode from flash
    MappedFile* mapped = file_hw_mmap(partition, offset, size);
    if (!mapped) {
        return JS_ThrowError(ctx, JS_CLASS_ERROR,
            "failed to mmap partition '%s' at offset %d", partition, offset);
    }

    // Verify it's bytecode
    if (!JS_IsBytecode(mapped->data, mapped->size)) {
        file_hw_munmap(mapped);
        return JS_ThrowError(ctx, JS_CLASS_ERROR,
            "data at partition '%s' offset %d is not valid bytecode", partition, offset);
    }

    // Relocate bytecode in-place (it's in flash, read-only)
    // Note: This modifies a temporary buffer, not the flash
    uint8_t* relocatable_copy = (uint8_t*)malloc(mapped->size);
    if (!relocatable_copy) {
        file_hw_munmap(mapped);
        return JS_ThrowOutOfMemory(ctx);
    }

    memcpy(relocatable_copy, mapped->data, mapped->size);
    file_hw_munmap(mapped);  // Unmap original

    if (JS_RelocateBytecode(ctx, relocatable_copy, mapped->size) != 0) {
        free(relocatable_copy);
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "failed to relocate bytecode");
    }

    // Load the relocated bytecode
    JSValue ret = JS_LoadBytecode(ctx, relocatable_copy);

    // Note: relocatable_copy must stay allocated - this is a managed leak
    // In production, track these in a list and free when context is destroyed

    return ret;
}

// Process timers - call this periodically from your main loop
// Returns the number of milliseconds until the next timer needs to run
int64_t JS_ProcessTimers(JSContext *ctx) {
    int64_t min_delay = 1000;
    int64_t cur_time = get_time_ms();
    bool has_timer = false;

    for(int i = 0; i < MAX_TIMERS; i++) {
        JSTimer *th = &js_timer_list[i];
        if (th->allocated) {
            has_timer = true;
            int64_t delay = th->timeout - cur_time;
            if (delay <= 0) {
                // Timer expired - execute it (following mqjs.c pattern)
                JSValue ret;
                JS_PushArg(ctx, th->func.val); /* func */
                JS_PushArg(ctx, JS_NULL); /* this */

                // Clean up timer before calling (in case timer re-schedules itself)
                JS_DeleteGCRef(ctx, &th->func);
                th->allocated = false;

                ret = JS_Call(ctx, 0);
                if (JS_IsException(ret)) {
                    // Log exception but don't exit (unlike mqjs.c which exits)
                    JSValue exception = JS_GetException(ctx);
                    (void)exception; // Could log this
                }
                min_delay = 0;
                break;
            } else if (delay < min_delay) {
                min_delay = delay;
            }
        }
    }

    return has_timer ? min_delay : -1;
}

// Forward declarations for LED functions (defined in freebutton_led.c)
JSValue js_freebutton_led_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_led_on(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_led_off(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_led_setColor(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// Include the generated FreeButton stdlib (with LED bindings)
#include "freebutton_stdlib.h"
