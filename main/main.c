#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"
#include "bootloader_random.h"
#include <time.h>
#include "llm.h"
#include <u8g2.h>
#include "u8g2_esp32_hal.h"
#include <driver/i2c.h>
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include <string.h>
#include "llama.h"

static const char *TAG = "MAIN";
u8g2_t u8g2;
bool display_present = false;

#define PIN_SDA CONFIG_I2C_MASTER_SDA
#define PIN_SCL CONFIG_I2C_MASTER_SCL
#define OLED_I2C_ADDRESS 0x78
#define PROMPT_MAX_LEN 256
#define PROMPT_MARKER "Prompt> " // ./setup.sh talk waits for this before sending a line
#define READY_MARKER "### READY" // ./setup.sh talk shows output from here on
#define ANSWER_MAX_LEN 120       // keeps the officer prompt well inside the 128-token context
#define LINK_UART UART_NUM_2     // text link to the XH-S3E: GPIO17 TX -> S3 IO42, GPIO16 RX <- S3 IO41
#define LINK_TX 17
#define LINK_RX 16
#define LINK_BAUD 115200

/**
 * @brief Checks whether an OLED answers on the I2C bus
 * The u8g2 HAL asserts on any I2C error, so the display is only used when present
 */
bool probe_display(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_I2C_MASTER_FREQUENCY,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_I2C_ADDRESS | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    // u8g2 installs its own driver
    i2c_driver_delete(I2C_NUM_0);
    return ret == ESP_OK;
}

/**
 * @brief Configure SSD1306 display
 * Uses I2C connection
 */
void init_display(void)
{
    display_present = probe_display();
    if (!display_present)
    {
        ESP_LOGW(TAG, "No display found on SDA %d / SCL %d, output goes to serial only", PIN_SDA, PIN_SCL);
        return;
    }
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
 * @brief Makes stdin read lines from the console UART
 * Without the UART driver, reads from stdin return immediately instead of blocking
 */
void init_console_input(void)
{
    uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 512, 0, 0, NULL, 0);
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    // terminals send CR on Enter; turn it into the LF fgets looks for
    uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    setvbuf(stdin, NULL, _IONBF, 0);
}

/**
 * @brief Opens the UART towards the XH-S3E, which speaks every line it receives
 */
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
    ESP_ERROR_CHECK(uart_driver_install(LINK_UART, 256, 1024, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LINK_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(LINK_UART, LINK_TX, LINK_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "link on UART2, TX GPIO%d / RX GPIO%d, %d baud", LINK_TX, LINK_RX, LINK_BAUD);
}

void link_write(const char *text)
{
    uart_write_bytes(LINK_UART, text, strlen(text));
}

/**
 * @brief Reads one line from the console, without the trailing newline
 */
void read_prompt(char *buffer, size_t bufsize)
{
    printf("\n" PROMPT_MARKER);
    fflush(stdout);
    buffer[0] = '\0';
    while (fgets(buffer, bufsize, stdin) == NULL)
    {
        clearerr(stdin);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    buffer[strcspn(buffer, "\r\n")] = '\0';
}

/**
 * @brief Outputs to display
 * 
 * @param text The text to output
 */
void write_display(char *text)
{
    if (!display_present)
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
    write_display(&buffer);
}

/**
 * @brief Draws a llama onscreen
 * 
 */
void draw_llama(void)
{
    if (!display_present)
        return;
    u8g2_DrawXBM(&u8g2, 0, 0, u8g2_GetDisplayWidth(&u8g2), u8g2_GetDisplayHeight(&u8g2), &llama_bmp);
    u8g2_SendBuffer(&u8g2);
}

void app_main(void)
{
    init_display();
    init_console_input();
    link_init();
    write_display("Loading Model");

    // default parameters
    char *checkpoint_path = "model"; // flash partition holding stories260K.bin
    char *tokenizer_path = "tokenizer"; // flash partition holding tok512.bin
    float temperature = 1.0f;        // 0.0 = greedy deterministic. 1.0 = original. don't set higher
    float topp = 0.9f;               // top-p in nucleus sampling. 1.0 = off. 0.9 works well, but slower
    int steps = 256;                 // number of steps to run for
    char input[PROMPT_MAX_LEN];      // line typed over serial
    char prompt[PROMPT_MAX_LEN + 32];
    unsigned long long rng_seed = 0; // seed rng from the hardware RNG by default

    // parameter validation/overrides
    // time() is ~0 at every boot (no RTC), and a zero seed gets xorshift stuck,
    // so seed from the hardware RNG with its entropy source enabled instead
    if (rng_seed <= 0)
    {
        bootloader_random_enable();
        rng_seed = ((unsigned long long)esp_random() << 32) | esp_random() | 1;
        bootloader_random_disable();
    }

    // build the Transformer via the model .bin file
    Transformer transformer;
    ESP_LOGI(TAG, "LLM partition is %s", checkpoint_path);
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
    printf("\n" READY_MARKER "\n");
#ifdef CONFIG_LLM_MODE_OFFICER
    // the officer speaks first; after that each answer gets a reply and a new question.
    // An empty line starts a fresh interrogation.
    input[0] = '\0';
    for (;;)
    {
        if (input[0] == '\0')
        {
            strcpy(prompt, "OFFICER:");
        }
        else
        {
            input[ANSWER_MAX_LEN] = '\0';
            snprintf(prompt, sizeof(prompt), "CITIZEN: %s\nOFFICER:", input);
        }
        draw_llama();
        printf("OFFICER:");
        generate(&transformer, &tokenizer, &sampler, prompt, steps, 0, &generate_complete_cb);
        link_write("\n"); // end of the officer's turn: the S3 speaks it
        read_prompt(input, sizeof(input));
    }
#else
    // type the start of a story and the model continues it; empty line = random story
    for (;;)
    {
        read_prompt(input, sizeof(input));
        draw_llama();
        generate(&transformer, &tokenizer, &sampler, input, steps, 1, &generate_complete_cb);
        link_write("\n");
    }
#endif
}
