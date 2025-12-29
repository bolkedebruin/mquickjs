/*
 * Function declarations for FreeButton hardware API stubs (WASM)
 * These are used only for WASM compilation
 */

#ifndef FREEBUTTON_STUBS_WASM_H
#define FREEBUTTON_STUBS_WASM_H

#ifdef EMSCRIPTEN

#include "mquickjs.h"

// Standard library functions (from stdlib_export.c)
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_loadMapped(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// LED API declarations
JSValue js_freebutton_led_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_led_on(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_led_off(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_led_setColor(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// Button API declarations
JSValue js_freebutton_button_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_button_setLabel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_button_setTopLabel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_button_onClick(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_button_onLongPress(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_button_onRelease(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// Sensor API declarations
JSValue js_freebutton_sensor_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_sensor_getValue(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_sensor_getType(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_sensor_getInfo(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_sensor_getAll(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_sensor_onChange(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// MQTT API declarations
JSValue js_freebutton_mqtt_getBrokerCount(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_mqtt_getBrokerName(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_mqtt_isConnected(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_mqtt_publish(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_mqtt_subscribe(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_mqtt_unsubscribe(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_mqtt_onConnect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_freebutton_mqtt_onDisconnect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

#endif // EMSCRIPTEN

#endif // FREEBUTTON_STUBS_WASM_H
