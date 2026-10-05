#include <ctype.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "sam.h"
#include "reciter.h"
#include "speak.h"

#define SPK_BCLK 15
#define SPK_WS 16
#define SPK_DOUT 7
#define SAM_RATE 22050
#define CHUNK_MAX 100 // SAM works on a 256-byte buffer and TextToPhonemes expands the text in place

static const char *TAG = "SPEAK";
static i2s_chan_handle_t tx;
int debug = 0; // SAM reads this global; upstream defines it in its CLI main.c

void speak_init(void)
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.auto_clear = true; // send silence on underrun instead of repeating the last buffer
    ESP_ERROR_CHECK(i2s_new_channel(&cc, &tx, NULL));
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAM_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = SPK_BCLK, .ws = SPK_WS, .dout = SPK_DOUT, .din = I2S_GPIO_UNUSED},
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(tx));
}

// Renders one chunk (< CHUNK_MAX chars) with SAM and plays it
static void say_chunk(const char *text, int len)
{
    char input[256];
    int n = 0;
    for (int i = 0; i < len && n < CHUNK_MAX; i++)
    {
        unsigned char c = text[i];
        if (c == '\n' || c == '\r')
            c = ' ';
        if (c < 32 || c > 126 || c == '[' || c == '"')
            continue; // quote marks alone come out as ~1 s of noise
        input[n++] = toupper(c);
    }
    while (n > 0 && input[n - 1] == ' ')
        n--;
    int letters = 0;
    for (int i = 0; i < n; i++)
        letters += isalpha((unsigned char)input[i]) != 0;
    if (letters == 0)
        return;
    input[n] = '\0';
    strcat(input, "[");
    if (!TextToPhonemes((unsigned char *)input))
    {
        ESP_LOGW(TAG, "no phonemes for: %.*s", len, text);
        return;
    }
    SetInput(input);
    if (!SAMMain())
    {
        ESP_LOGW(TAG, "SAMMain failed for: %.*s", len, text);
        return;
    }

    // SAM output: 8-bit unsigned, 22050 Hz; GetBufferLength() counts in 1/50 samples
    const unsigned char *pcm = (const unsigned char *)GetBuffer();
    int samples = GetBufferLength() / 50;
    ESP_LOGD(TAG, "%d chars -> %d samples (%d ms): %.*s", len, samples, samples * 1000 / SAM_RATE, len > 40 ? 40 : len, text);
    static int16_t out[512];
    for (int pos = 0; pos < samples;)
    {
        int k = 0;
        while (k < 512 && pos < samples)
            out[k++] = ((int)pcm[pos++] - 128) << 7;
        size_t written;
        i2s_channel_write(tx, out, k * sizeof(int16_t), &written, portMAX_DELAY);
    }
}

void speak(const char *text)
{
    // cut at sentence ends, or at the last space before CHUNK_MAX
    const char *p = text;
    while (*p)
    {
        while (*p == ' ' || *p == '\n')
            p++;
        int len = 0, cut = 0;
        while (p[len] && len < CHUNK_MAX)
        {
            char c = p[len++];
            if (c == '.' || c == '!' || c == '?' || c == '\n')
            {
                cut = len;
                break;
            }
            if (c == ' ')
                cut = len;
        }
        if (!p[len] || cut == 0)
            cut = len;
        say_chunk(p, cut);
        p += cut;
    }
}
