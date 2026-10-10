#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "speak.h"
#include "link.h"

#define LINK_UART UART_NUM_1
#define LINK_TX 41
#define LINK_RX 42
#define LINK_BAUD 115200
#define LINK_LINE_MAX 256

static const char *TAG = "LINK";

static void link_task(void *arg)
{
    static char line[LINK_LINE_MAX];
    int len = 0;
    uint8_t buf[64];
    for (;;)
    {
        int n = uart_read_bytes(LINK_UART, buf, sizeof(buf), pdMS_TO_TICKS(100));
        for (int i = 0; i < n; i++)
        {
            char c = (char)buf[i];
            if (c == '\r')
                continue;
            if (c != '\n' && len < LINK_LINE_MAX - 1)
            {
                line[len++] = c;
                continue;
            }
            // newline, or the line is full: hand it over
            line[len] = '\0';
            if (len > 0)
            {
                printf("\n[link] %s\n", line);
                speak(line);
            }
            len = 0;
        }
    }
}

void link_init(void)
{
    uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(LINK_UART, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LINK_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(LINK_UART, LINK_TX, LINK_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(link_task, "link", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "listening on UART1, RX IO%d / TX IO%d, %d baud", LINK_RX, LINK_TX, LINK_BAUD);
}
