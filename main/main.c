#include <stdio.h>
#include <inttypes.h>
#include "esp_spiffs.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include <time.h>
#include "llm.h"
#include <u8g2.h>
#include "u8g2_esp32_hal.h"
#include <driver/i2c.h>
#include <string.h>
#include "llama.h"
#include "esp_random.h"
#include "hal/usb_serial_jtag_ll.h"

static const char *TAG = "MAIN";
u8g2_t u8g2;

#define PIN_SDA 8
#define PIN_SCL 9
#define OLED_I2C_ADDRESS 0x78

// 0 = no SSD1306 attached: skip all u8g2 calls (u8g2 HAL asserts when nothing answers on I2C)
#define USE_DISPLAY 0

/**
 * @brief Configure SSD1306 display
 * Uses I2C connection
 */
void init_display(void)
{
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.bus.i2c.sda = PIN_SDA;
    u8g2_esp32_hal.bus.i2c.scl = PIN_SCL;
    u8g2_esp32_hal_init(u8g2_esp32_hal);
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2, U8G2_R0,
        // u8x8_byte_sw_i2c,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb); // init u8g2 structure
    // 0x3c
    u8x8_SetI2CAddress(&u8g2.u8x8, OLED_I2C_ADDRESS);
    u8g2_InitDisplay(&u8g2);     // send init sequence to the display, display is in
                                 // sleep mode after this,
    u8g2_SetPowerSave(&u8g2, 0); // wake up display
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_SendBuffer(&u8g2);
    ESP_LOGI(TAG, "Display initialized");
}

/**
 * @brief intializes SPIFFS storage
 * 
 */
void init_storage(void)
{

    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/data",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

/**
 * @brief Outputs to display
 * 
 * @param text The text to output
 */
void write_display(char *text)
{
    if (!USE_DISPLAY)
        return;
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) / 2, text);
    u8g2_SendBuffer(&u8g2);
}

/**
 * @brief Callbacks once generation is done
 * 
 * @param tk_s The number of tokens per second generated
 */
void generate_complete_cb(float tk_s)
{
    char buffer[50];
    sprintf(buffer, "%.2f tok/s", tk_s);
    ESP_LOGI(TAG, "%s", buffer);
    write_display(&buffer);
}

/**
 * @brief Draws a llama onscreen
 * 
 */
void draw_llama(void)
{
    if (!USE_DISPLAY)
        return;
    u8g2_DrawXBM(&u8g2, 0, 0, u8g2_GetDisplayWidth(&u8g2), u8g2_GetDisplayHeight(&u8g2), &llama_bmp);
    u8g2_SendBuffer(&u8g2);
}

/**
 * @brief Reads one line typed on the native USB port (USB-Serial-JTAG), echoing it back
 *
 * Polls the RX FIFO directly, so the console output path stays untouched.
 * Enter on an empty line returns 0.
 */
int read_line(char *buf, int max)
{
    static uint8_t prev = 0;
    int n = 0;
    for (;;)
    {
        uint8_t c;
        if (!usb_serial_jtag_ll_rxfifo_data_available() || usb_serial_jtag_ll_read_rxfifo(&c, 1) != 1)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        uint8_t last = prev;
        prev = c;
        if (c == '\n' && last == '\r')
            continue; // second half of CRLF
        if (c == '\r' || c == '\n')
        {
            printf("\n");
            fflush(stdout);
            buf[n] = '\0';
            return n;
        }
        if ((c == 8 || c == 127) && n > 0)
        {
            n--;
            printf("\b \b");
        }
        else if (c >= 32 && c < 127 && n < max - 1)
        {
            buf[n++] = c;
            putchar(c);
        }
        fflush(stdout);
        usb_serial_jtag_ll_txfifo_flush(); // echo each key as it is typed
    }
}

void app_main(void)
{
    if (USE_DISPLAY)
        init_display();
    write_display("Loading Model");
    init_storage();

    // default parameters
    char *checkpoint_path = "/data/stories260K.bin"; // e.g. out/model.bin
    char *tokenizer_path = "/data/tok512.bin";
    float temperature = 1.0f;        // 0.0 = greedy deterministic. 1.0 = original. don't set higher
    float topp = 0.9f;               // top-p in nucleus sampling. 1.0 = off. 0.9 works well, but slower
    int steps = 256;                 // number of steps to run for
    char *prompt = NULL;             // prompt string
    unsigned long long rng_seed = 0; // seed rng with time by default

    // parameter validation/overrides
    if (rng_seed <= 0)
        rng_seed = esp_random(); // time(NULL) is ~0 right after boot: same story every reset

    // build the Transformer via the model .bin file
    Transformer transformer;
    ESP_LOGI(TAG, "LLM Path is %s", checkpoint_path);
    build_transformer(&transformer, checkpoint_path);
    if (steps == 0 || steps > transformer.config.seq_len)
        steps = transformer.config.seq_len; // override to ~max length

    // build the Tokenizer via the tokenizer .bin file
    Tokenizer tokenizer;
    build_tokenizer(&tokenizer, tokenizer_path, transformer.config.vocab_size);

    // build the Sampler
    Sampler sampler;
    build_sampler(&sampler, transformer.config.vocab_size, temperature, topp, rng_seed);

    // run!
    draw_llama();
    generate(&transformer, &tokenizer, &sampler, prompt, steps, &generate_complete_cb);

    // then keep going: each line typed on the serial monitor starts a new story
    char line[256];
    for (;;)
    {
        printf("\nPrompt> ");
        fflush(stdout);
        usb_serial_jtag_ll_txfifo_flush(); // no newline after the prompt: push it out now
        read_line(line, sizeof(line));
        generate(&transformer, &tokenizer, &sampler, line[0] ? line : NULL, steps, &generate_complete_cb);
    }
}
