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

/* Bytecode relocation helper - get size of a memory block in bytes
   Used by BytecodeWriter for streaming relocation to flash */
size_t JS_GetMemBlockSize(const void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MQUICKJS_EXT_H */
