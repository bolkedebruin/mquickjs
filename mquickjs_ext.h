/*
 * MicroQuickJS Extensions for FreeButton Firmware
 *
 * This header contains firmware-specific extensions to mquickjs that are
 * not part of upstream bellard/mquickjs. Keeping these separate minimizes
 * the diff with upstream.
 */

#ifndef MQUICKJS_EXT_H
#define MQUICKJS_EXT_H

#include "mquickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Console output callback for print()/console.log()
   Output goes to both stdout AND this callback */
typedef void (*JSConsoleWriteFunc)(const char* buf, size_t len);
void JS_SetConsoleWriteFunc(JSConsoleWriteFunc func);

/* Timer processing - call periodically to execute scheduled timers
   Returns delay in ms until next timer, or -1 if no timers */
int64_t JS_ProcessTimers(JSContext *ctx);

/* Bytecode relocation helpers - expose internal memory block information
   Used by BytecodeWriter for streaming relocation to flash */

/* Get size of a memory block in bytes */
size_t JS_GetMemBlockSize(const void *ptr);

/* Get memory tag (type) of a block */
int JS_GetMemBlockMTag(const void *ptr);

/*
 * Memory block structures for relocation (match internal layout)
 * These structures allow external code to perform relocation without
 * access to mquickjs_priv.h internals.
 */

#define JS_MB_HEADER_PUBLIC \
    JSWord gc_mark: 1; \
    JSWord mtag: 3

typedef struct JSFunctionBytecodePublic {
    JS_MB_HEADER_PUBLIC;
    JSWord has_arguments : 1;
    JSWord has_local_func_name : 1;
    JSWord has_column : 1;
    JSWord arg_count : 16;
    JSWord dummy: (JSW * 8 - 4 - 3 - 16);
    JSValue func_name;
    JSValue byte_code;
    JSValue cpool;
    JSValue vars;
    JSValue ext_vars;
    uint16_t stack_size;
    uint16_t ext_vars_len;
    JSValue filename;
    JSValue pc2line;
    uint32_t source_pos;
} JSFunctionBytecodePublic;

typedef struct JSValueArrayPublic {
    JS_MB_HEADER_PUBLIC;
    JSWord size: (JSW * 8 - 4);
    JSValue arr[];
} JSValueArrayPublic;

#ifdef __cplusplus
}
#endif

#endif /* MQUICKJS_EXT_H */
