/*
 * FreeButton Standard Library for MicroQuickJS
 *
 * This file defines a custom JavaScript stdlib that includes:
 * - Full ES5 JavaScript standard library (from mqjs_stdlib.c)
 * - FreeButton LED control API
 *
 * Build process:
 * 1. Compile this file with mquickjs_build.c as a host executable
 * 2. Run the executable with -m32 flag to generate 32-bit headers
 * 3. Output is redirected to freebutton_stdlib.h
 * 4. The firmware includes this header and uses &js_stdlib
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mquickjs_build.h"

/*
 * FreeButton LED Control API
 *
 * JavaScript bindings for NeoPixel LED control:
 *   led.count()                      - Get number of available LEDs
 *   led.on(position)                 - Turn LED white at position
 *   led.off(position)                - Turn LED off at position
 *   led.setColor(position, r, g, b)  - Set LED RGB color (0-255)
 *
 * The actual function implementations are in freebutton_led.c which is
 * compiled separately as part of the ESP32 firmware build. The JS_CFUNC_DEF
 * macro stringifies the function names, so no forward declarations are needed.
 */
static const JSPropDef js_freebutton_led[] = {
    JS_CFUNC_DEF("count", 0, js_freebutton_led_count),
    JS_CFUNC_DEF("on", 1, js_freebutton_led_on),
    JS_CFUNC_DEF("off", 1, js_freebutton_led_off),
    JS_CFUNC_DEF("setColor", 4, js_freebutton_led_setColor),
    JS_PROP_END,
};

static const JSClassDef js_freebutton_led_obj =
    JS_OBJECT_DEF("LED", js_freebutton_led);

/*
 * FreeButton Button Control API
 *
 * JavaScript bindings for button control and event handling:
 *   button.count()                           - Get number of available buttons
 *   button.setLabel(position, text)          - Update button label
 *   button.setTopLabel(position, text)       - Update button top label
 *   button.onClick(position, callback)       - Register click event handler
 *   button.onLongPress(position, callback)   - Register long press event handler
 *   button.onRelease(position, callback)     - Register release event handler
 *
 * The actual function implementations are in freebutton_button.c which is
 * compiled separately as part of the ESP32 firmware build.
 */
static const JSPropDef js_freebutton_button[] = {
    JS_CFUNC_DEF("count", 0, js_freebutton_button_count),
    JS_CFUNC_DEF("setLabel", 2, js_freebutton_button_setLabel),
    JS_CFUNC_DEF("setTopLabel", 2, js_freebutton_button_setTopLabel),
    JS_CFUNC_DEF("onClick", 2, js_freebutton_button_onClick),
    JS_CFUNC_DEF("onLongPress", 2, js_freebutton_button_onLongPress),
    JS_CFUNC_DEF("onRelease", 2, js_freebutton_button_onRelease),
    JS_PROP_END,
};

static const JSClassDef js_freebutton_button_obj =
    JS_OBJECT_DEF("Button", js_freebutton_button);

/*
 * FreeButton Sensor API
 *
 * JavaScript bindings for sensor reading and monitoring:
 *   sensor.count()               - Get number of available sensors
 *   sensor.getValue(sensorId)    - Read current sensor value
 *   sensor.getType(sensorId)     - Get sensor type name
 *   sensor.getInfo(sensorId)     - Get sensor information object
 *   sensor.getAll()              - Get array of all sensor information
 *   sensor.onChange(sensorId, callback) - Register change event handler
 *
 * The actual function implementations are in freebutton_sensor.c which is
 * compiled separately as part of the ESP32 firmware build.
 */
static const JSPropDef js_freebutton_sensor[] = {
    JS_CFUNC_DEF("count", 0, js_freebutton_sensor_count),
    JS_CFUNC_DEF("getValue", 1, js_freebutton_sensor_getValue),
    JS_CFUNC_DEF("getType", 1, js_freebutton_sensor_getType),
    JS_CFUNC_DEF("getInfo", 1, js_freebutton_sensor_getInfo),
    JS_CFUNC_DEF("getAll", 0, js_freebutton_sensor_getAll),
    JS_CFUNC_DEF("onChange", 2, js_freebutton_sensor_onChange),
    JS_PROP_END,
};

static const JSClassDef js_freebutton_sensor_obj =
    JS_OBJECT_DEF("Sensor", js_freebutton_sensor);

/*
 * Include the full standard library
 *
 * We need to define CONFIG_FREEBUTTON_LED, CONFIG_FREEBUTTON_BUTTON,
 * and CONFIG_FREEBUTTON_SENSOR before including mqjs_stdlib.c so that
 * the LED, Button, and Sensor objects are added to the global namespace.
 */
#define CONFIG_FREEBUTTON_LED
#define CONFIG_FREEBUTTON_BUTTON
#define CONFIG_FREEBUTTON_SENSOR
#include "mqjs_stdlib.c"
