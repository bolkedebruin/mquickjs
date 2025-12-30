/*
 * FreeButton LED JavaScript Bindings
 *
 * Provides JavaScript API for controlling NeoPixel LEDs through
 * the FreeButton hardware abstraction layer.
 *
 * JavaScript API:
 *   led.on(position)                  - Turn LED white at position
 *   led.off(position)                 - Turn LED off at position
 *   led.setColor(position, r, g, b)   - Set LED RGB color
 *   led.count()                       - Get number of available LEDs
 */

// IMPORTANT: Include mquickjs.h FIRST to establish proper type definitions
// mquickjs.h includes <inttypes.h> which defines stdint types needed by ESP-IDF headers
#include "mquickjs.h"
#include <stdio.h>
#include <stdlib.h>

// Import the hardware abstraction layer (ESP32 only)
#ifdef ESP_PLATFORM
#include "../../src/scripting/led_hardware.h"
#else
// Host build stubs for generator
static inline int led_hw_get_count(void) { return 0; }
static inline int led_hw_set_color(int pos, uint8_t r, uint8_t g, uint8_t b) { return 0; }
#endif

/*
 * JavaScript bindings
 */

/**
 * @jsapi led.count
 * @description Get number of available LEDs
 * @returns {number} Number of available LEDs
 */
JSValue js_freebutton_led_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int count = led_hw_get_count();
    return JS_NewInt32(ctx, count);
}

/**
 * @jsapi led.on
 * @description Turn LED white at specified position
 * @param {number} position - LED position index
 * @returns {void}
 */
JSValue js_freebutton_led_on(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0;

    if (argc >= 1) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "led.on() requires position argument");
    }

    // Turn on white (255, 255, 255)
    if (led_hw_set_color(position, 255, 255, 255) < 0) {
        return JS_ThrowInternalError(ctx, "failed to set LED %d", position);
    }

    return JS_UNDEFINED;
}

/**
 * @jsapi led.off
 * @description Turn LED off at specified position
 * @param {number} position - LED position index
 * @returns {void}
 */
JSValue js_freebutton_led_off(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0;

    if (argc >= 1) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "led.off() requires position argument");
    }

    // Turn off (0, 0, 0)
    if (led_hw_set_color(position, 0, 0, 0) < 0) {
        return JS_ThrowInternalError(ctx, "failed to set LED %d", position);
    }

    return JS_UNDEFINED;
}

/**
 * @jsapi led.setColor
 * @description Set LED RGB color (0-255 for each component)
 * @param {number} position - LED position index
 * @param {number} r - Red component (0-255)
 * @param {number} g - Green component (0-255)
 * @param {number} b - Blue component (0-255)
 * @returns {void}
 */
JSValue js_freebutton_led_setColor(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int position = 0, r = 0, g = 0, b = 0;

    if (argc >= 4) {
        if (JS_ToInt32(ctx, &position, argv[0]))
            return JS_EXCEPTION;
        if (JS_ToInt32(ctx, &r, argv[1]))
            return JS_EXCEPTION;
        if (JS_ToInt32(ctx, &g, argv[2]))
            return JS_EXCEPTION;
        if (JS_ToInt32(ctx, &b, argv[3]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "led.setColor() requires position, r, g, b arguments");
    }

    // Clamp to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    if (led_hw_set_color(position, (uint8_t)r, (uint8_t)g, (uint8_t)b) < 0) {
        return JS_ThrowInternalError(ctx, "failed to set LED %d color", position);
    }

    return JS_UNDEFINED;
}
