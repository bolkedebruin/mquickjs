/*
 * FreeButton Button JavaScript Bindings
 *
 * Provides JavaScript API for button control and event handling through
 * the FreeButton hardware abstraction layer.
 *
 * JavaScript API:
 *   button.setLabel(position, text)           - Update button label
 *   button.setTopLabel(position, text)        - Update button top label
 *   button.onClick(position, callback)        - Register click event handler
 *   button.onLongPress(position, callback)    - Register long press event handler
 *   button.onRelease(position, callback)      - Register release event handler
 *   button.count()                            - Get number of available buttons
 */

#include <stdio.h>
#include <stdlib.h>
#include "mquickjs.h"

// Import the hardware abstraction layer (ESP32 only)
#ifdef ESP_PLATFORM
#include "../../src/scripting/button_hardware.h"
#else
// Host build stubs for generator
static inline int button_hw_get_count(void) { return 0; }
static inline int button_hw_set_label(int pos, const char* label) { return 0; }
static inline int button_hw_set_top_label(int pos, const char* label) { return 0; }
static inline void button_hw_register_click_callback(int pos, void (*cb)(int)) {}
static inline void button_hw_register_long_press_callback(int pos, void (*cb)(int)) {}
static inline void button_hw_register_release_callback(int pos, void (*cb)(int)) {}
#endif

// Maximum number of buttons (library limit, actual count from button_hw_get_count())
// This should be >= BUTTON_COUNT from config.h
#define MAX_BUTTONS 8

// Storage for JavaScript callback functions using JSGCRef for GC protection
static struct {
    JSContext *ctx;
    JSGCRef clickCallback;
    JSGCRef longPressCallback;
    JSGCRef releaseCallback;
    int clickAllocated;
    int longPressAllocated;
    int releaseAllocated;
} js_button_callbacks[MAX_BUTTONS] = {{NULL, {JS_UNDEFINED, NULL}, {JS_UNDEFINED, NULL}, {JS_UNDEFINED, NULL}, 0, 0, 0}};

/*
 * C callback wrappers - these are called from the hardware layer
 * and invoke the JavaScript callbacks
 */

static void js_button_click_wrapper(int position) {
    // Positions are 1-based (1-8)
    if (position < 1 || position > MAX_BUTTONS) return;

    int idx = position - 1;  // Convert to 0-based array index
    if (!js_button_callbacks[idx].clickAllocated) return;

    JSContext *ctx = js_button_callbacks[idx].ctx;
    JSGCRef *callback = &js_button_callbacks[idx].clickCallback;

    if (!ctx) return;

    // Check stack space (2 slots: function + this)
    if (JS_StackCheck(ctx, 2))
        return;

    // Push function and this
    JS_PushArg(ctx, callback->val);
    JS_PushArg(ctx, JS_NULL);

    // Call the function
    JSValue result = JS_Call(ctx, 0);

    // Check for exceptions but don't exit - just log
    if (JS_IsException(result)) {
        // Exception occurred - could add logging here
    }
}

static void js_button_long_press_wrapper(int position) {
    // Positions are 1-based (1-8)
    if (position < 1 || position > MAX_BUTTONS) return;

    int idx = position - 1;  // Convert to 0-based array index
    if (!js_button_callbacks[idx].longPressAllocated) return;

    JSContext *ctx = js_button_callbacks[idx].ctx;
    JSGCRef *callback = &js_button_callbacks[idx].longPressCallback;

    if (!ctx) return;

    if (JS_StackCheck(ctx, 2))
        return;

    JS_PushArg(ctx, callback->val);
    JS_PushArg(ctx, JS_NULL);

    JSValue result = JS_Call(ctx, 0);

    if (JS_IsException(result)) {
        // Exception occurred
    }
}

static void js_button_release_wrapper(int position) {
    // Positions are 1-based (1-8)
    if (position < 1 || position > MAX_BUTTONS) return;

    int idx = position - 1;  // Convert to 0-based array index
    if (!js_button_callbacks[idx].releaseAllocated) return;

    JSContext *ctx = js_button_callbacks[idx].ctx;
    JSGCRef *callback = &js_button_callbacks[idx].releaseCallback;

    if (!ctx) return;

    if (JS_StackCheck(ctx, 2))
        return;

    JS_PushArg(ctx, callback->val);
    JS_PushArg(ctx, JS_NULL);

    JSValue result = JS_Call(ctx, 0);

    if (JS_IsException(result)) {
        // Exception occurred
    }
}

/*
 * JavaScript bindings
 */

/**
 * @jsapi button.count
 * @description Get number of available buttons
 * @returns {number} Number of buttons
 */
JSValue js_freebutton_button_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int count = button_hw_get_count();
    return JS_NewInt32(ctx, count);
}

/**
 * @jsapi button.setLabel
 * @description Set button label text
 * @param {number} position - Button position (1-based)
 * @param {string} text - Label text to display
 * @returns {void}
 */
JSValue js_freebutton_button_setLabel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0;
    const char *label = NULL;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;

        JSCStringBuf buf;
        size_t len;
        label = JS_ToCStringLen(ctx, &len, argv[1], &buf);
        if (!label)
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "button.setLabel() requires position and text arguments");
    }

    if (button_hw_set_label(position, label) < 0) {
        return JS_ThrowInternalError(ctx, "failed to set label for button %d", position);
    }

    return JS_UNDEFINED;
}

/**
 * @jsapi button.setTopLabel
 * @description Set button top label text
 * @param {number} position - Button position (1-based)
 * @param {string} text - Top label text to display
 * @returns {void}
 */
JSValue js_freebutton_button_setTopLabel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0;
    const char *topLabel = NULL;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;

        JSCStringBuf buf;
        size_t len;
        topLabel = JS_ToCStringLen(ctx, &len, argv[1], &buf);
        if (!topLabel)
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "button.setTopLabel() requires position and text arguments");
    }

    if (button_hw_set_top_label(position, topLabel) < 0) {
        return JS_ThrowInternalError(ctx, "failed to set top label for button %d", position);
    }

    return JS_UNDEFINED;
}

/**
 * @jsapi button.onClick
 * @description Register click event handler for button
 * @param {number} position - Button position (1-based)
 * @param {Function} callback - Function to call on click
 * @returns {void}
 */
JSValue js_freebutton_button_onClick(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;

        if (!JS_IsFunction(ctx, argv[1])) {
            return JS_ThrowTypeError(ctx, "button.onClick() callback must be a function");
        }
    } else {
        return JS_ThrowTypeError(ctx, "button.onClick() requires position and callback arguments");
    }

    // Positions are 1-based (1-8 for BUTTON_COUNT=8)
    if (position < 1 || position > MAX_BUTTONS) {
        return JS_ThrowRangeError(ctx, "button position %d out of range (1-%d)", position, MAX_BUTTONS);
    }

    // Convert to 0-based index for array access
    int idx = position - 1;

    // Free old callback if exists
    if (js_button_callbacks[idx].clickAllocated) {
        JS_DeleteGCRef(ctx, &js_button_callbacks[idx].clickCallback);
        js_button_callbacks[idx].clickAllocated = 0;
    }

    // Store the callback using GC reference
    js_button_callbacks[idx].ctx = ctx;
    JSValue *pfunc = JS_AddGCRef(ctx, &js_button_callbacks[idx].clickCallback);
    *pfunc = argv[1];
    js_button_callbacks[idx].clickAllocated = 1;

    // Register the C wrapper with hardware layer
    button_hw_register_click_callback(position, js_button_click_wrapper);

    return JS_UNDEFINED;
}

/**
 * @jsapi button.onLongPress
 * @description Register long press event handler for button
 * @param {number} position - Button position (1-based)
 * @param {Function} callback - Function to call on long press
 * @returns {void}
 */
JSValue js_freebutton_button_onLongPress(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;

        if (!JS_IsFunction(ctx, argv[1])) {
            return JS_ThrowTypeError(ctx, "button.onLongPress() callback must be a function");
        }
    } else {
        return JS_ThrowTypeError(ctx, "button.onLongPress() requires position and callback arguments");
    }

    // Positions are 1-based (1-8 for BUTTON_COUNT=8)
    if (position < 1 || position > MAX_BUTTONS) {
        return JS_ThrowRangeError(ctx, "button position %d out of range (1-%d)", position, MAX_BUTTONS);
    }

    // Convert to 0-based index for array access
    int idx = position - 1;

    // Free old callback if exists
    if (js_button_callbacks[idx].longPressAllocated) {
        JS_DeleteGCRef(ctx, &js_button_callbacks[idx].longPressCallback);
        js_button_callbacks[idx].longPressAllocated = 0;
    }

    // Store the callback using GC reference
    js_button_callbacks[idx].ctx = ctx;
    JSValue *pfunc = JS_AddGCRef(ctx, &js_button_callbacks[idx].longPressCallback);
    *pfunc = argv[1];
    js_button_callbacks[idx].longPressAllocated = 1;

    // Register the C wrapper with hardware layer
    button_hw_register_long_press_callback(position, js_button_long_press_wrapper);

    return JS_UNDEFINED;
}

/**
 * @jsapi button.onRelease
 * @description Register release event handler for button
 * @param {number} position - Button position (1-based)
 * @param {Function} callback - Function to call on release
 * @returns {void}
 */
JSValue js_freebutton_button_onRelease(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;

        if (!JS_IsFunction(ctx, argv[1])) {
            return JS_ThrowTypeError(ctx, "button.onRelease() callback must be a function");
        }
    } else {
        return JS_ThrowTypeError(ctx, "button.onRelease() requires position and callback arguments");
    }

    // Positions are 1-based (1-8 for BUTTON_COUNT=8)
    if (position < 1 || position > MAX_BUTTONS) {
        return JS_ThrowRangeError(ctx, "button position %d out of range (1-%d)", position, MAX_BUTTONS);
    }

    // Convert to 0-based index for array access
    int idx = position - 1;

    // Free old callback if exists
    if (js_button_callbacks[idx].releaseAllocated) {
        JS_DeleteGCRef(ctx, &js_button_callbacks[idx].releaseCallback);
        js_button_callbacks[idx].releaseAllocated = 0;
    }

    // Store the callback using GC reference
    js_button_callbacks[idx].ctx = ctx;
    JSValue *pfunc = JS_AddGCRef(ctx, &js_button_callbacks[idx].releaseCallback);
    *pfunc = argv[1];
    js_button_callbacks[idx].releaseAllocated = 1;

    // Register the C wrapper with hardware layer
    button_hw_register_release_callback(position, js_button_release_wrapper);

    return JS_UNDEFINED;
}
