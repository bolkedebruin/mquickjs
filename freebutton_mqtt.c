/*
 * FreeButton MQTT JavaScript Bindings
 *
 * Provides JavaScript API for MQTT publish/subscribe through
 * the FreeButton hardware abstraction layer.
 *
 * JavaScript API:
 *   mqtt.publish(brokerId, topic, payload[, qos, retain])  - Publish message
 *   mqtt.subscribe(brokerId, topic, callback[, qos])       - Subscribe to topic
 *   mqtt.unsubscribe(brokerId, topic)                      - Unsubscribe from topic
 *   mqtt.onConnect(brokerId, callback)                     - Register connect callback
 *   mqtt.onDisconnect(brokerId, callback)                  - Register disconnect callback
 *   mqtt.isConnected(brokerId)                             - Check connection status
 *   mqtt.getBrokerName(brokerId)                           - Get broker name
 *   mqtt.getBrokerCount()                                  - Get number of brokers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mquickjs.h"
#include "esp_log.h"

// Import the binding layer
#include "../../src/scripting/mqtt_binding.h"

static const char *MQTT_JS_TAG = "MqttJS";

// Maximum subscriptions per broker
#define MAX_BROKERS 2
#define MAX_SUBSCRIPTIONS_PER_BROKER 8

// Storage for JavaScript callbacks using JSGCRef for GC protection
typedef struct {
    JSContext *ctx;
    JSGCRef callback;
    char *topic;
    int allocated;
} MqttSubscription;

typedef struct {
    JSContext *ctx;
    JSGCRef connectCallback;
    JSGCRef disconnectCallback;
    int connectAllocated;
    int disconnectAllocated;
    MqttSubscription subscriptions[MAX_SUBSCRIPTIONS_PER_BROKER];
} BrokerCallbacks;

static BrokerCallbacks js_mqtt_brokers[MAX_BROKERS] = {0};

/*
 * Helper: Find subscription slot for a topic
 */
static MqttSubscription* find_subscription(uint8_t brokerId, const char* topic) {
    if (brokerId >= MAX_BROKERS || !topic)
        return NULL;

    BrokerCallbacks *broker = &js_mqtt_brokers[brokerId];
    for (int i = 0; i < MAX_SUBSCRIPTIONS_PER_BROKER; i++) {
        if (broker->subscriptions[i].allocated &&
            broker->subscriptions[i].topic &&
            strcmp(broker->subscriptions[i].topic, topic) == 0) {
            return &broker->subscriptions[i];
        }
    }
    return NULL;
}

/*
 * Helper: Find free subscription slot
 */
static MqttSubscription* alloc_subscription(uint8_t brokerId) {
    if (brokerId >= MAX_BROKERS)
        return NULL;

    BrokerCallbacks *broker = &js_mqtt_brokers[brokerId];
    for (int i = 0; i < MAX_SUBSCRIPTIONS_PER_BROKER; i++) {
        if (!broker->subscriptions[i].allocated) {
            return &broker->subscriptions[i];
        }
    }
    return NULL;
}

/*
 * C callback wrapper - called from the hardware layer when MQTT message arrives
 * and invokes the JavaScript callback
 */
static void js_mqtt_message_wrapper(uint8_t brokerId, const char* topic, const char* payload, size_t length) {
    if (brokerId >= MAX_BROKERS)
        return;

    BrokerCallbacks *broker = &js_mqtt_brokers[brokerId];

    // Find matching subscription(s) - support wildcards by checking all subscriptions
    for (int i = 0; i < MAX_SUBSCRIPTIONS_PER_BROKER; i++) {
        MqttSubscription *sub = &broker->subscriptions[i];

        if (!sub->allocated || !sub->ctx)
            continue;

        // Simple topic matching (exact match for now, wildcard support can be added)
        // For production, implement MQTT wildcard matching (+ and #)
        int matches = (strcmp(sub->topic, topic) == 0);

        // Check for wildcard matches
        if (!matches) {
            // Support for # wildcard (multi-level)
            size_t topic_len = strlen(sub->topic);
            if (topic_len > 0 && sub->topic[topic_len - 1] == '#') {
                // Check if topic starts with the prefix before #
                size_t prefix_len = topic_len - 1;
                if (prefix_len > 0 && sub->topic[prefix_len - 1] == '/')
                    prefix_len--;
                matches = (strncmp(sub->topic, topic, prefix_len) == 0);
            }
        }

        if (!matches)
            continue;

        JSContext *ctx = sub->ctx;

        // Stack check FIRST (3 slots: 2 args + function + this)
        if (JS_StackCheck(ctx, 4))
            continue;

        // Verify callback is still a function
        if (!JS_IsFunction(ctx, sub->callback.val))
            continue;

        // Create arguments
        JSValue topicArg = JS_NewString(ctx, topic);
        JSValue payloadArg;

        // Create payload string (ensure null-terminated)
        char *payload_str = (char*)malloc(length + 1);
        if (payload_str) {
            memcpy(payload_str, payload, length);
            payload_str[length] = '\0';
            payloadArg = JS_NewString(ctx, payload_str);
            free(payload_str);
        } else {
            payloadArg = JS_NewString(ctx, "");
        }

        // REVERSE ORDER: args (right to left), function, this
        JS_PushArg(ctx, payloadArg);     // Arg 2 (payload)
        JS_PushArg(ctx, topicArg);       // Arg 1 (topic)
        JS_PushArg(ctx, sub->callback.val);  // Function
        JS_PushArg(ctx, JS_NULL);        // This

        JSValue result = JS_Call(ctx, 2);  // 2 arguments

        // Handle exceptions gracefully
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(ctx);
            JSCStringBuf buf;
            size_t len;
            const char* str = JS_ToCStringLen(ctx, &len, exception, &buf);
            if (str) {
                ESP_LOGW(MQTT_JS_TAG, "Exception in MQTT message callback for topic '%s': %.*s",
                         topic, (int)len, str);
            }
        }
    }
}

/*
 * C callback wrapper - called when broker connects
 */
static void js_mqtt_connect_wrapper(uint8_t brokerId) {
    if (brokerId >= MAX_BROKERS)
        return;

    BrokerCallbacks *broker = &js_mqtt_brokers[brokerId];

    if (!broker->connectAllocated || !broker->ctx)
        return;

    JSContext *ctx = broker->ctx;

    if (JS_StackCheck(ctx, 3))
        return;

    if (!JS_IsFunction(ctx, broker->connectCallback.val))
        return;

    // Create broker ID argument
    JSValue brokerIdArg = JS_NewInt32(ctx, brokerId);

    // REVERSE ORDER: arg, function, this
    JS_PushArg(ctx, brokerIdArg);
    JS_PushArg(ctx, broker->connectCallback.val);
    JS_PushArg(ctx, JS_NULL);

    JSValue result = JS_Call(ctx, 1);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        JSCStringBuf buf;
        size_t len;
        const char* str = JS_ToCStringLen(ctx, &len, exception, &buf);
        if (str) {
            ESP_LOGW(MQTT_JS_TAG, "Exception in MQTT connect callback: %.*s", (int)len, str);
        }
    }
}

/*
 * C callback wrapper - called when broker disconnects
 */
static void js_mqtt_disconnect_wrapper(uint8_t brokerId) {
    if (brokerId >= MAX_BROKERS)
        return;

    BrokerCallbacks *broker = &js_mqtt_brokers[brokerId];

    if (!broker->disconnectAllocated || !broker->ctx)
        return;

    JSContext *ctx = broker->ctx;

    if (JS_StackCheck(ctx, 3))
        return;

    if (!JS_IsFunction(ctx, broker->disconnectCallback.val))
        return;

    // Create broker ID argument
    JSValue brokerIdArg = JS_NewInt32(ctx, brokerId);

    // REVERSE ORDER: arg, function, this
    JS_PushArg(ctx, brokerIdArg);
    JS_PushArg(ctx, broker->disconnectCallback.val);
    JS_PushArg(ctx, JS_NULL);

    JSValue result = JS_Call(ctx, 1);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        JSCStringBuf buf;
        size_t len;
        const char* str = JS_ToCStringLen(ctx, &len, exception, &buf);
        if (str) {
            ESP_LOGW(MQTT_JS_TAG, "Exception in MQTT disconnect callback: %.*s", (int)len, str);
        }
    }
}

/*
 * JavaScript bindings
 */

/* mqtt.getBrokerCount() - Get number of configured brokers */
JSValue js_freebutton_mqtt_getBrokerCount(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;

    int count = mqtt_binding_get_broker_count();
    return JS_NewInt32(ctx, count);
}

/* mqtt.getBrokerName(brokerId) - Get broker name */
JSValue js_freebutton_mqtt_getBrokerName(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;

    int brokerId = 0;

    if (argc >= 1) {
        if (JS_ToInt32(ctx, &brokerId, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "mqtt.getBrokerName() requires brokerId argument");
    }

    const char* name = mqtt_binding_get_broker_name((uint8_t)brokerId);
    if (!name) {
        return JS_NULL;
    }

    return JS_NewString(ctx, name);
}

/* mqtt.isConnected(brokerId) - Check if broker is connected */
JSValue js_freebutton_mqtt_isConnected(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;

    int brokerId = 0;

    if (argc >= 1) {
        if (JS_ToInt32(ctx, &brokerId, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "mqtt.isConnected() requires brokerId argument");
    }

    int connected = mqtt_binding_is_connected((uint8_t)brokerId);
    return JS_NewBool(connected);
}

/* mqtt.publish(brokerId, topic, payload[, qos, retain]) - Publish MQTT message */
JSValue js_freebutton_mqtt_publish(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;

    int brokerId = 0;
    JSCStringBuf topic_buf, payload_buf;
    const char *topic, *payload;
    int qos = 0;
    int retain = 0;

    if (argc >= 3) {
        if (JS_ToInt32(ctx, &brokerId, argv[0]))
            return JS_EXCEPTION;

        topic = JS_ToCString(ctx, argv[1], &topic_buf);
        if (!topic)
            return JS_EXCEPTION;

        payload = JS_ToCString(ctx, argv[2], &payload_buf);
        if (!payload)
            return JS_EXCEPTION;

        if (argc >= 4) {
            if (JS_ToInt32(ctx, &qos, argv[3]))
                return JS_EXCEPTION;
        }

        if (argc >= 5) {
            if (JS_ToInt32(ctx, &retain, argv[4]))
                return JS_EXCEPTION;
        }
    } else {
        return JS_ThrowTypeError(ctx, "mqtt.publish() requires brokerId, topic, and payload arguments");
    }

    if (mqtt_binding_publish((uint8_t)brokerId, topic, payload, qos, retain) < 0) {
        return JS_ThrowInternalError(ctx, "failed to publish MQTT message");
    }

    return JS_UNDEFINED;
}

/* mqtt.subscribe(brokerId, topic, callback[, qos]) - Subscribe to MQTT topic */
JSValue js_freebutton_mqtt_subscribe(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;

    int brokerId = 0;
    JSCStringBuf topic_buf;
    const char *topic;
    int qos = 0;

    if (argc >= 3) {
        if (JS_ToInt32(ctx, &brokerId, argv[0]))
            return JS_EXCEPTION;

        topic = JS_ToCString(ctx, argv[1], &topic_buf);
        if (!topic)
            return JS_EXCEPTION;

        if (!JS_IsFunction(ctx, argv[2])) {
            return JS_ThrowTypeError(ctx, "mqtt.subscribe() callback must be a function");
        }

        if (argc >= 4) {
            if (JS_ToInt32(ctx, &qos, argv[3]))
                return JS_EXCEPTION;
        }
    } else {
        return JS_ThrowTypeError(ctx, "mqtt.subscribe() requires brokerId, topic, and callback arguments");
    }

    // Validate broker ID
    if (brokerId < 0 || brokerId >= MAX_BROKERS) {
        return JS_ThrowRangeError(ctx, "broker ID %d out of range (0-%d)", brokerId, MAX_BROKERS - 1);
    }

    // Check if already subscribed to this topic
    MqttSubscription* existing = find_subscription((uint8_t)brokerId, topic);
    if (existing) {
        // Update callback
        JS_DeleteGCRef(ctx, &existing->callback);
        JSValue *pfunc = JS_AddGCRef(ctx, &existing->callback);
        *pfunc = argv[2];
        ESP_LOGI(MQTT_JS_TAG, "Updated subscription for broker %d topic %s", brokerId, topic);
        return JS_UNDEFINED;
    }

    // Allocate new subscription slot
    MqttSubscription* sub = alloc_subscription((uint8_t)brokerId);
    if (!sub) {
        return JS_ThrowInternalError(ctx, "too many MQTT subscriptions for broker %d", brokerId);
    }

    // Dynamically allocate topic string (memory efficient)
    sub->topic = strdup(topic);
    if (!sub->topic) {
        // Out of memory - fail gracefully
        return JS_ThrowInternalError(ctx, "out of memory allocating topic string");
    }

    // Store the callback using GC reference
    sub->ctx = ctx;
    JSValue *pfunc = JS_AddGCRef(ctx, &sub->callback);
    *pfunc = argv[2];
    sub->allocated = 1;

    // Register binding layer callbacks (only once)
    static int callbacks_registered = 0;
    if (!callbacks_registered) {
        mqtt_binding_register_message_callback(js_mqtt_message_wrapper);
        mqtt_binding_register_connect_callback(js_mqtt_connect_wrapper);
        mqtt_binding_register_disconnect_callback(js_mqtt_disconnect_wrapper);
        callbacks_registered = 1;
    }

    // Subscribe through binding layer
    if (mqtt_binding_subscribe((uint8_t)brokerId, topic, qos) < 0) {
        JS_DeleteGCRef(ctx, &sub->callback);
        sub->allocated = 0;
        return JS_ThrowInternalError(ctx, "failed to subscribe to MQTT topic");
    }

    ESP_LOGI(MQTT_JS_TAG, "Subscribed to broker %d topic %s", brokerId, topic);
    return JS_UNDEFINED;
}

/* mqtt.unsubscribe(brokerId, topic) - Unsubscribe from MQTT topic */
JSValue js_freebutton_mqtt_unsubscribe(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;

    int brokerId = 0;
    JSCStringBuf topic_buf;
    const char *topic;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &brokerId, argv[0]))
            return JS_EXCEPTION;

        topic = JS_ToCString(ctx, argv[1], &topic_buf);
        if (!topic)
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "mqtt.unsubscribe() requires brokerId and topic arguments");
    }

    // Find and free subscription
    MqttSubscription* sub = find_subscription((uint8_t)brokerId, topic);
    if (sub) {
        // Free dynamically allocated topic string
        if (sub->topic) {
            free(sub->topic);
            sub->topic = NULL;
        }

        // Free JavaScript callback reference
        JS_DeleteGCRef(ctx, &sub->callback);
        sub->allocated = 0;
        sub->ctx = NULL;
    }

    // Unsubscribe through binding layer
    if (mqtt_binding_unsubscribe((uint8_t)brokerId, topic) < 0) {
        return JS_ThrowInternalError(ctx, "failed to unsubscribe from MQTT topic");
    }

    return JS_UNDEFINED;
}

/* mqtt.onConnect(brokerId, callback) - Register connection event handler */
JSValue js_freebutton_mqtt_onConnect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;

    int brokerId = 0;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &brokerId, argv[0]))
            return JS_EXCEPTION;

        if (!JS_IsFunction(ctx, argv[1])) {
            return JS_ThrowTypeError(ctx, "mqtt.onConnect() callback must be a function");
        }
    } else {
        return JS_ThrowTypeError(ctx, "mqtt.onConnect() requires brokerId and callback arguments");
    }

    if (brokerId < 0 || brokerId >= MAX_BROKERS) {
        return JS_ThrowRangeError(ctx, "broker ID %d out of range (0-%d)", brokerId, MAX_BROKERS - 1);
    }

    BrokerCallbacks *broker = &js_mqtt_brokers[brokerId];

    // Free old callback if exists
    if (broker->connectAllocated) {
        JS_DeleteGCRef(ctx, &broker->connectCallback);
    }

    // Store the callback
    broker->ctx = ctx;
    JSValue *pfunc = JS_AddGCRef(ctx, &broker->connectCallback);
    *pfunc = argv[1];
    broker->connectAllocated = 1;

    return JS_UNDEFINED;
}

/* mqtt.onDisconnect(brokerId, callback) - Register disconnection event handler */
JSValue js_freebutton_mqtt_onDisconnect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;

    int brokerId = 0;

    if (argc >= 2) {
        if (JS_ToInt32(ctx, &brokerId, argv[0]))
            return JS_EXCEPTION;

        if (!JS_IsFunction(ctx, argv[1])) {
            return JS_ThrowTypeError(ctx, "mqtt.onDisconnect() callback must be a function");
        }
    } else {
        return JS_ThrowTypeError(ctx, "mqtt.onDisconnect() requires brokerId and callback arguments");
    }

    if (brokerId < 0 || brokerId >= MAX_BROKERS) {
        return JS_ThrowRangeError(ctx, "broker ID %d out of range (0-%d)", brokerId, MAX_BROKERS - 1);
    }

    BrokerCallbacks *broker = &js_mqtt_brokers[brokerId];

    // Free old callback if exists
    if (broker->disconnectAllocated) {
        JS_DeleteGCRef(ctx, &broker->disconnectCallback);
    }

    // Store the callback
    broker->ctx = ctx;
    JSValue *pfunc = JS_AddGCRef(ctx, &broker->disconnectCallback);
    *pfunc = argv[1];
    broker->disconnectAllocated = 1;

    return JS_UNDEFINED;
}
