/*
 * Stub implementations for FreeButton hardware APIs
 *
 * These stubs are ONLY used for WASM compilation to ensure that:
 * 1. The atom table includes all FreeButton API function names
 * 2. Bytecode compiled in browser can reference led.on(), sensor.getValue(), etc.
 * 3. When bytecode runs on device, the real implementations are used
 *
 * These functions never actually execute - they're just for compilation.
 */

#include "mquickjs.h"
#include "freebutton_stubs_wasm.h"

#ifdef EMSCRIPTEN

#include <stdio.h>
#include <stdlib.h>

/*
 * LED API Stubs
 */

JSValue js_freebutton_led_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt32(ctx, 0);
}

JSValue js_freebutton_led_on(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_led_off(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_led_setColor(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

/*
 * Button API Stubs
 */

JSValue js_freebutton_button_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt32(ctx, 0);
}

JSValue js_freebutton_button_setLabel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_button_setTopLabel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_button_onClick(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_button_onLongPress(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_button_onRelease(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

/*
 * Sensor API Stubs
 */

JSValue js_freebutton_sensor_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt32(ctx, 0);
}

JSValue js_freebutton_sensor_getValue(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewFloat64(ctx, 0.0);
}

JSValue js_freebutton_sensor_getType(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewString(ctx, "unknown");
}

JSValue js_freebutton_sensor_getInfo(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewObject(ctx);
}

JSValue js_freebutton_sensor_getAll(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewArray(ctx, 0);
}

JSValue js_freebutton_sensor_onChange(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

/*
 * MQTT API Stubs
 */

JSValue js_freebutton_mqtt_getBrokerCount(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt32(ctx, 0);
}

JSValue js_freebutton_mqtt_getBrokerName(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewString(ctx, "");
}

JSValue js_freebutton_mqtt_isConnected(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_FALSE;
}

JSValue js_freebutton_mqtt_publish(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_mqtt_subscribe(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_mqtt_unsubscribe(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_mqtt_onConnect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue js_freebutton_mqtt_onDisconnect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

#endif // EMSCRIPTEN
