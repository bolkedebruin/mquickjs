/*
 * LED Hardware Stub for stdlib generation
 *
 * This is a stub implementation used when compiling the stdlib builder
 * tool on the host system. The actual LED hardware functions are only
 * used when running on ESP32.
 */

#include <stdint.h>
#include <stdio.h>

// Stub implementation - returns fixed values for host build
int led_hw_get_count() {
    return 8;  // Assume 8 LEDs for stdlib generation
}

int led_hw_set_color(int position, uint8_t r, uint8_t g, uint8_t b) {
    // Stub - does nothing on host
    return 0;
}
