# Running a LLM on the ESP32
![LLM on ESP32](/ESP32_LLM.jpg)
![LLM Output](/llm_output.gif)

## This branch: `esp32-s3` (Dundir fork)

Runs on an ESP32-S3 **N16R8** (16 MB flash, 8 MB octal PSRAM), tested on an XH-S3E-AI_V1.0 board at ~35 tok/s, and reads every story out loud.

- **Fixes:** `v4sf` is a plain `float` (the 16-byte aligned scalar broke float argument passing on Xtensa: NaN output, then a crash in top-p sampling); `USE_DISPLAY 0` when no SSD1306 is attached; `sdkconfig` set to octal PSRAM and 16 MB flash.
- **Chat:** after the boot story the board waits at `Prompt>` on the native USB port — `idf.py -p /dev/cu.usbmodem* monitor`, type the start of a story + Enter, empty line = random story. The boot story is the same on every reset (Lily and the big red ball).
- **Speech:** I2S amp on BCLK 15, WS 16, DOUT 7 (`main/speak.c`), robot voice from SAM. Set `USE_SPEECH 0` in `main/main.c` to turn it off.
- **Power:** the XH-S3E-AI board has no USB-C CC resistors, so a USB-C to USB-C cable from a Mac gives it no power — use a USB-A port (adapter, hub or dock).

> [!WARNING]
> **`components/sam/` has no open-source license.** SAM ("Software Automatic Mouth") is a reverse-engineered version of a 1982 Commodore 64 program; its upstream, [s-macke/SAM](https://github.com/s-macke/SAM), states it is not under any open-source license and relies on fair use. It is included here as-is for convenience. Check whether that works for you before reusing or redistributing it, or build with `USE_SPEECH 0` and delete the component. Details: [`components/sam/README.md`](components/sam/README.md).

## Summary
I wanted to see if it was possible to run a Large Language Model (LLM) on the ESP32. Surprisingly it is possible, though probably not very useful.

The "Large" Language Model used is actually quite small. It is a 260K parameter [tinyllamas checkpoint](https://huggingface.co/karpathy/tinyllamas/tree/main/stories260K) trained on the [tiny stories](https://huggingface.co/datasets/roneneldan/TinyStories) dataset.

The LLM implementation is done using [llama.2c](https://github.com/karpathy/llama2.c) with minor optimizations to make it run faster on the ESP32.

## Hardware
LLMs require a great deal of memory. Even this small one still requires 1MB of RAM. I used the [ESP32-S3FH4R2](https://www.mouser.com/ProductDetail/Espressif-Systems/ESP32-S3FH4R2?qs=tlsG%2FOw5FFjPrwkmZSBQNA%3D%3D) because it has 2MB of embedded PSRAM.

## Optimizing Llama2.c for the ESP32

With the following changes to `llama2.c`, I am able to achieve **19.13 tok/s**:

1. Utilizing both cores of the ESP32 during math heavy operations.
2. Utilizing some special [dot product functions](https://github.com/espressif/esp-dsp/tree/master/modules/dotprod/float) from the [ESP-DSP library](https://github.com/espressif/esp-dsp) that are designed for the ESP32-S3. These functions utilize some of the [few SIMD instructions](https://bitbanksoftware.blogspot.com/2024/01/surprise-esp32-s3-has-few-simd.html) the ESP32-S3 has.
3. Maxing out CPU speed to 240 MHz and PSRAM speed to 80MHZ and increasing the instruction cache size.


## Setup
This requires the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#installation) toolchain to be installed

```
idf.py build
idf.py -p /dev/{DEVICE_PORT} flash
```


