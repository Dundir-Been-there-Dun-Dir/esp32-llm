#include "serial_bridge.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "serial_bridge";

#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 256

static int s_uart_num = UART_NUM_2;
static bool s_base_ready = false;
static bridge_proximity_cb s_proximity_cb = NULL;
static bridge_motion_done_cb s_motion_cb = NULL;
static SemaphoreHandle_t s_cmd_sem = NULL;
static char s_last_done_cmd[32] = {0};

static void bridge_rx_task(void *pvParameters)
{
    uint8_t line_buf[128];
    int line_pos = 0;

    ESP_LOGI(TAG, "Serial bridge RX background monitor started on UART%d", s_uart_num);

    while (1) {
        uint8_t ch;
        int len = uart_read_bytes(s_uart_num, &ch, 1, portMAX_DELAY);
        if (len > 0) {
            if (ch == '\n' || ch == '\r') {
                if (line_pos > 0) {
                    line_buf[line_pos] = '\0';
                    char *msg = (char *)line_buf;

                    ESP_LOGD(TAG, "[BASE-MSG]: %s", msg);

                    if (strcmp(msg, "READY") == 0) {
                        s_base_ready = true;
                        ESP_LOGI(TAG, "NiceNano Base reporting READY (servos & sensors live)");
                    } else if (strncmp(msg, "PROXIMITY ", 10) == 0) {
                        int dist_mm = atoi(msg + 10);
                        ESP_LOGI(TAG, "Citizen detected at distance: %d mm", dist_mm);
                        if (s_proximity_cb != NULL) {
                            s_proximity_cb(dist_mm);
                        }
                    } else if (strncmp(msg, "DONE ", 5) == 0) {
                        strncpy(s_last_done_cmd, msg + 5, sizeof(s_last_done_cmd) - 1);
                        s_last_done_cmd[sizeof(s_last_done_cmd) - 1] = '\0';
                        ESP_LOGI(TAG, "Motion command completed: %s", s_last_done_cmd);
                        if (s_motion_cb != NULL) {
                            s_motion_cb(s_last_done_cmd);
                        }
                        if (s_cmd_sem != NULL) {
                            xSemaphoreGive(s_cmd_sem);
                        }
                    } else if (strncmp(msg, "ERR ", 4) == 0) {
                        ESP_LOGW(TAG, "Base reported error: %s", msg + 4);
                    }

                    line_pos = 0;
                }
            } else {
                if (line_pos < sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = ch;
                }
            }
        }
    }
}

esp_err_t bridge_init(int uart_num, int tx_pin, int rx_pin, int baud_rate)
{
    s_uart_num = uart_num;

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err;
    err = uart_driver_install(s_uart_num, RX_BUF_SIZE, TX_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(s_uart_num, &uart_config);
    if (err != ESP_OK) return err;

    err = uart_set_pin(s_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    s_cmd_sem = xSemaphoreCreateBinary();

    xTaskCreate(bridge_rx_task, "bridge_rx_task", 3072, NULL, 10, NULL);

    ESP_LOGI(TAG, "Initialized bridge on UART%d (TX=%d, RX=%d, %d baud)", s_uart_num, tx_pin, rx_pin, baud_rate);
    return ESP_OK;
}

void bridge_set_proximity_cb(bridge_proximity_cb cb)
{
    s_proximity_cb = cb;
}

void bridge_set_motion_done_cb(bridge_motion_done_cb cb)
{
    s_motion_cb = cb;
}

esp_err_t bridge_send_command(const char *cmd)
{
    if (cmd == NULL) return ESP_ERR_INVALID_ARG;

    char out_buf[64];
    int len = snprintf(out_buf, sizeof(out_buf), "%s\n", cmd);
    if (len <= 0) return ESP_FAIL;

    ESP_LOGI(TAG, "Sending command to base: %s", cmd);
    int written = uart_write_bytes(s_uart_num, out_buf, len);
    return (written == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t bridge_send_and_wait(const char *cmd, uint32_t timeout_ms)
{
    if (s_cmd_sem == NULL) return ESP_ERR_INVALID_STATE;

    // Drain any stale semaphore
    xSemaphoreTake(s_cmd_sem, 0);

    esp_err_t err = bridge_send_command(cmd);
    if (err != ESP_OK) return err;

    if (xSemaphoreTake(s_cmd_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Command '%s' timed out after %lu ms", cmd, (unsigned long)timeout_ms);
    return ESP_ERR_TIMEOUT;
}

bool bridge_is_base_ready(void)
{
    return s_base_ready;
}
