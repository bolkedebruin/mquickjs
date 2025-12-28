/*
 * FreeButton Sensor JavaScript Bindings
 *
 * Provides JavaScript API for sensor reading and monitoring through
 * the FreeButton hardware abstraction layer.
 *
 * JavaScript API:
 *   sensor.getValue(sensorId)     - Read current sensor value
 *   sensor.getType(sensorId)      - Get sensor type ("temperature", "humidity", etc.)
 *   sensor.getInfo(sensorId)      - Get sensor information object
 *   sensor.getAll()               - Get array of all sensor information
 *   sensor.count()                - Get number of available sensors
 */

#include <stdio.h>
#include <stdlib.h>
#include "mquickjs.h"
#include "esp_log.h"

// Import the hardware abstraction layer
#include "../../src/scripting/sensor_binding.h"

static const char *TAG = "SensorJS";

// Maximum number of sensors (must match sensor_binding.cpp)
#define MAX_SENSORS 8

// Storage for JavaScript onChange callbacks using JSGCRef for GC protection
static struct {
    JSContext *ctx;
    JSGCRef changeCallback;
    int allocated;
} js_sensor_callbacks[MAX_SENSORS] = {{NULL, {JS_UNDEFINED, NULL}, 0}};

/*
 * C callback wrapper - called from the hardware layer
 * and invokes the JavaScript callback
 */
static void js_sensor_change_wrapper(int sensorId, float value) {
    if (sensorId < 0 || sensorId >= MAX_SENSORS)
        return;

    if (!js_sensor_callbacks[sensorId].allocated)
        return;

    JSContext *ctx = js_sensor_callbacks[sensorId].ctx;
    JSGCRef *callback = &js_sensor_callbacks[sensorId].changeCallback;

    if (!ctx)
        return;

    // Check stack space (3 slots: argument + function + this)
    if (JS_StackCheck(ctx, 3))
        return;

    // Verify callback is still a function
    if (!JS_IsFunction(ctx, callback->val))
        return;

    // Create JSValue for the sensor value
    JSValue valueArg = JS_NewFloat64(ctx, (double)value);

    // Push in correct MQuickJS order: arguments, function, this
    JS_PushArg(ctx, valueArg);       // Argument FIRST
    JS_PushArg(ctx, callback->val);  // Function SECOND
    JS_PushArg(ctx, JS_NULL);        // This THIRD

    JSValue result = JS_Call(ctx, 1);  // 1 argument

    // Log exceptions but don't crash the system
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        JSCStringBuf buf;
        size_t len;
        const char* str = JS_ToCStringLen(ctx, &len, exception, &buf);
        if (str) {
            ESP_LOGW(TAG, "Exception in sensor %d onChange callback: %.*s", sensorId, (int)len, str);
        }
    }
}

/*
 * JavaScript bindings
 */

/* sensor.count() - Get number of available sensors */
JSValue js_freebutton_sensor_count(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int count = sensor_hw_get_count();
    return JS_NewInt32(ctx, count);
}

/* sensor.getValue(sensorId) - Read current sensor value */
JSValue js_freebutton_sensor_getValue(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int sensorId = 0;

    if (argc >= 1) {
        if (JS_ToInt32(ctx, &sensorId, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "sensor.getValue() requires sensorId argument");
    }

    float value = sensor_hw_get_value(sensorId);
    return JS_NewFloat64(ctx, (double)value);
}

/* sensor.getType(sensorId) - Get sensor type name */
JSValue js_freebutton_sensor_getType(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int sensorId = 0;

    if (argc >= 1) {
        if (JS_ToInt32(ctx, &sensorId, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "sensor.getType() requires sensorId argument");
    }

    const char* type = sensor_hw_get_type(sensorId);
    if (!type) {
        return JS_NULL;
    }

    return JS_NewString(ctx, type);
}

/* sensor.getInfo(sensorId) - Get sensor information object */
JSValue js_freebutton_sensor_getInfo(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int sensorId = 0;

    if (argc >= 1) {
        if (JS_ToInt32(ctx, &sensorId, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "sensor.getInfo() requires sensorId argument");
    }

    const SensorInfo* info = sensor_hw_get_info(sensorId);
    if (!info) {
        return JS_NULL;
    }

    // Create a JavaScript object with sensor information
    JSValue obj = JS_NewObject(ctx);

    // Set properties
    JS_SetPropertyStr(ctx, obj, "id", JS_NewInt32(ctx, info->id));
    JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, info->name));
    JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, info->type));
    JS_SetPropertyStr(ctx, obj, "unit", JS_NewString(ctx, info->unit));
    JS_SetPropertyStr(ctx, obj, "online", JS_NewBool(info->online));

    return obj;
}

/* sensor.getAll() - Get array of all sensor information */
JSValue js_freebutton_sensor_getAll(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // Get all sensor IDs
    int ids[MAX_SENSORS];
    int count = sensor_hw_get_all_ids(ids, MAX_SENSORS);

    // Create JavaScript array
    JSValue arr = JS_NewArray(ctx, count);

    // Populate array with sensor info objects
    for (int i = 0; i < count; i++) {
        const SensorInfo* info = sensor_hw_get_info(ids[i]);
        if (info) {
            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "id", JS_NewInt32(ctx, info->id));
            JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, info->name));
            JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, info->type));
            JS_SetPropertyStr(ctx, obj, "unit", JS_NewString(ctx, info->unit));
            JS_SetPropertyStr(ctx, obj, "online", JS_NewBool(info->online));

            JS_SetPropertyUint32(ctx, arr, i, obj);
        }
    }

    return arr;
}

/* sensor.onChange(sensorId, callback) - Register change event handler */
JSValue js_freebutton_sensor_onChange(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int sensorId = 0;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &sensorId, argv[0]))
            return JS_EXCEPTION;

        if (!JS_IsFunction(ctx, argv[1])) {
            return JS_ThrowTypeError(ctx, "sensor.onChange() callback must be a function");
        }
    } else {
        return JS_ThrowTypeError(ctx, "sensor.onChange() requires sensorId and callback arguments");
    }

    // Validate sensor ID range
    if (sensorId < 0 || sensorId >= MAX_SENSORS) {
        return JS_ThrowRangeError(ctx, "sensor ID %d out of range (0-%d)", sensorId, MAX_SENSORS - 1);
    }

    // Free old callback if exists
    if (js_sensor_callbacks[sensorId].allocated) {
        JS_DeleteGCRef(ctx, &js_sensor_callbacks[sensorId].changeCallback);
        js_sensor_callbacks[sensorId].allocated = 0;
    }

    // Store the callback using GC reference
    js_sensor_callbacks[sensorId].ctx = ctx;
    JSValue *pfunc = JS_AddGCRef(ctx, &js_sensor_callbacks[sensorId].changeCallback);
    *pfunc = argv[1];
    js_sensor_callbacks[sensorId].allocated = 1;

    // Register the C wrapper with hardware layer
    sensor_hw_register_change_callback(sensorId, js_sensor_change_wrapper);

    return JS_UNDEFINED;
}