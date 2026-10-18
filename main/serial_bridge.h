#ifndef SERIAL_BRIDGE_H
#define SERIAL_BRIDGE_H

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback invoked when NiceNano detects an obstacle / approaching citizen via HC-SR04
typedef void (*bridge_proximity_cb)(int distance_mm);

// Callback invoked when a motion command completes on the base
typedef void (*bridge_motion_done_cb)(const char *cmd_name);

/**
 * @brief Initializes the UART serial bridge to the NiceNano base
 * @param uart_num Hardware UART port (e.g. UART_NUM_1 or UART_NUM_2)
 * @param tx_pin GPIO pin for TX (connects to NiceNano P0_22 / Header 022)
 * @param rx_pin GPIO pin for RX (connects to NiceNano P0_24 / Header 024)
 * @param baud_rate Baud rate (typically 115200)
 */
esp_err_t bridge_init(int uart_num, int tx_pin, int rx_pin, int baud_rate);

/**
 * @brief Registers callback for ultrasonic citizen proximity alerts
 */
void bridge_set_proximity_cb(bridge_proximity_cb cb);

/**
 * @brief Registers callback for motion completion events
 */
void bridge_set_motion_done_cb(bridge_motion_done_cb cb);

/**
 * @brief Sends an ASCII motion command to the NiceNano base
 * Available commands: WAKEUP, SLEEP, STOP, WIGGLE, CURIOUS, WALK <n>, MOONWALK <n>, PIVOT <n>
 */
esp_err_t bridge_send_command(const char *cmd);

/**
 * @brief Sends a command and synchronously waits for DONE or timeout
 */
esp_err_t bridge_send_and_wait(const char *cmd, uint32_t timeout_ms);

/**
 * @brief Checks if base is connected and reported READY
 */
bool bridge_is_base_ready(void);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_BRIDGE_H
