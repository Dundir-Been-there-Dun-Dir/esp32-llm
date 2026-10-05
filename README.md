# A tiny LLM on a plain ESP32 (no PSRAM): story writer and sustainability officer

![LLM on ESP32](/ESP32_LLM.jpg)

This fork of [DaveBben/esp32-llm](https://github.com/DaveBben/esp32-llm) runs a 260K-parameter
[llama2.c](https://github.com/karpathy/llama2.c) model on a **standard ESP32-WROOM-32 board, with no PSRAM**,
at about **21 tokens/s**. It has two modes:

- **Story writer:** type the start of a story and the model continues it (the original
  [stories260K](https://huggingface.co/karpathy/tinyllamas/tree/main/stories260K) model, trained on
  [TinyStories](https://huggingface.co/datasets/roneneldan/TinyStories)).
- **Sustainability officer (default):** a comedic enforcement robot from a post-apocalyptic regime
  that enforces extremely strict sustainability. It interrogates you, reacts to your confessions,
  hands out penalties or rewards, and asks the next question. We trained this model from scratch
  on a CPU with the scripts in [`training/`](training/). It was built as a party prop.

Real output from the board:

```
OFFICER: Attention citizen. This is a mandatory sustainability inspection. When did you last sigh?
You: no comment
OFFICER: Your vague answer has been logged as a Class 12 vagueness. You will compost your own shoes.
         Describe your last shower. Leave nothing out.
You: i am dancing
OFFICER: That is the most glitter I have heard since the old world ended. Your shower privileges
         are revoked for 4 weeks. How many wheels touched the road on your way here?
```

The story model and the original ESP32-S3 version are still available: the S3 version is on the
[`main`](https://github.com/Dundir-Been-there-Dun-Dir/esp32-llm/tree/main) branch, and the story mode is one setting away
(see [Switching models](#switching-models)).

## What you need

- **An ESP32 board with an ESP32-WROOM-32 module and 4 MB of flash.** The common "ESP32 DevKit"
  boards work; we tested on an ESP32-D0WDQ6, revision 1.0. No PSRAM is needed.
- **A USB cable that carries data.** Many cables only charge. If `lsusb` doesn't list your board,
  try another cable.
- **A Linux PC.** We tested on Ubuntu 24.04. macOS should work too; `setup.sh install` will list the
  packages to install by hand.
- **Optional:** a 128×64 SSD1306 I2C OLED on **GPIO 21 (SDA)** and **GPIO 22 (SCL)**. Without one,
  everything goes over USB serial.

## Quick start

```bash
git clone -b esp32-wroom https://github.com/Dundir-Been-there-Dun-Dir/esp32-llm.git
cd esp32-llm

./setup.sh install      # one time: system packages (asks for sudo) + ESP-IDF v5.3 in ~/esp/esp-idf
./setup.sh build        # build the firmware
./setup.sh flash usb0   # flash firmware + model (usb0 = /dev/ttyUSB0, acm0 = /dev/ttyACM0)
./setup.sh talk usb0    # talk to it; Ctrl+C to quit
```

In `talk`, type your answer and press Enter. An empty line starts a new interrogation (or gives a
random story, in story mode). Short, simple confessions work best: *i drove here*, *i took a long
shower*, *i ate a burger*, *i used a plastic bag*, *i am dancing*, *i have a dog*, *no comment*,
*i want a lawyer*.

**Notes:**
- **The first build fails once, then works.** On a fresh checkout the first build reports the `u8g2`
  component as "corrupted". `setup.sh build` retries automatically, and the second attempt succeeds.
- **All `setup.sh` commands:**

| Command | What it does |
|---|---|
| `./setup.sh install` | Install system packages and ESP-IDF (one time) |
| `./setup.sh build` | Build the firmware |
| `./setup.sh flash [PORT]` | Flash firmware, model and tokenizer |
| `./setup.sh talk [PORT]` | Interactive session; Ctrl+C quits |
| `./setup.sh log [PORT] [SECS]` | Reset the board and save its output to `logs/` (default 60 s) |
| `./setup.sh monitor [PORT]` | ESP-IDF serial monitor (Ctrl+] quits) |
| `./setup.sh run [PORT]` | Build, flash and monitor |

`PORT` can be left out (auto-detect), or given as `usb0`, `acm0` or a full path.

## Switching models

```bash
. ~/esp/esp-idf/export.sh
idf.py menuconfig        # Example Configuration → Model → Story writer / Sustainability officer
./setup.sh build && ./setup.sh flash usb0
```

The same menu also sets the display pins and the maximum context length (`LLM_MAX_SEQ_LEN`, default
128 tokens).

## Train your own character

Everything the officer says comes from [`training/generate_data.py`](training/generate_data.py),
which writes thousands of synthetic exchanges like this one:

```
CITIZEN: i watered my lawn
OFFICER: LAWN WATERING?! The Ministry has detected lawn watering in your confession. You must plant
         5 trees. With your bare hands. How many litres, citizen?
```

To make your own character, edit the confessions, reactions, penalties and questions in that file,
then generate the data and train:

```bash
cd training
python3 -m venv .venv
.venv/bin/pip install --index-url https://download.pytorch.org/whl/cpu torch
.venv/bin/pip install sentencepiece numpy

.venv/bin/python generate_data.py --out data/officer.txt     # 60,000 exchanges
.venv/bin/python train.py --data data/officer.txt --name officer
# writes ../data/officer.bin and ../data/officer_tok.bin

cd .. && ./setup.sh build && ./setup.sh flash usb0
```

Training takes about an hour on an 8-core laptop CPU, with no GPU needed. `train.py` trains a
512-token sentencepiece vocabulary, then a model with the same shape as stories260K (dim 64,
5 layers, 8 heads, 4 KV heads). It exports both in llama2.c's format. Each exchange starts with a
BOS token, and the firmware stops generating when the model emits the next one, so replies end
cleanly.

**Keep in mind:** a 260K-parameter model can't follow instructions or reason. It learns the
*style and structure* of the training data and remixes it. It handles answers similar to the
training data well. On anything else it falls back to its "off topic" or "dodging the question"
replies, which you write in the generator.

## How it fits on a plain ESP32

The original project needs about 1.7 MB of RAM: 1 MB of weights loaded into PSRAM, plus about
650 KB of attention cache for 512 tokens. A WROOM-32 has about 300 KB free. This fork:

1. **Reads the weights straight from flash.** The model and tokenizer get their own raw flash
   partitions ([`partitions.csv`](partitions.csv)) and are memory-mapped with `esp_partition_mmap`,
   so the weights take no RAM and are read through the flash cache. `idf.py flash` writes them.
2. **Caps the context at 128 tokens**, shrinking the attention cache to about 165 KB. About 125 KB
   of RAM stays free.
3. **Uses esp-dsp's generic `dsps_dotprod_f32`**, which picks the right optimized routine on the
   ESP32 or the ESP32-S3.

It still uses both cores for the matrix maths, as in the original.

### Bugs fixed along the way (these also affect the original)

- **Random seed.** It came from `time(NULL)`, which is about 0 at every boot. A zero seed freezes
  the xorshift generator, so the sampler always picked the most likely token and every boot printed
  the same story. The seed now comes from the hardware RNG.
- **Sampler.** `build_sampler`, `sample_topp` and `sample_mult` took `v4sf` by value, where `v4sf` is
  a `float` with an `aligned(16)` attribute, and the header declared plain `float`. On Xtensa this
  garbles the arguments: temperature arrived as about 10³⁸, and the top-p sort buffer was written
  into another task's stack. The frozen seed hid this, because with a random value of 0 the sampler
  picks the top token regardless.
- **Display.** The u8g2 HAL asserts on any I2C error, so the firmware crashed without a display
  attached. It now probes for the OLED first.
- **`Kconfig.projbuild`** was in the project root, where ESP-IDF ignores it. It now lives in `main/`.

## Troubleshooting

| Problem | Fix |
|---|---|
| Board not in `lsusb` | Charge-only cable; try another. |
| `This chip is ESP32-S3, not ESP32` | Use the [`main`](https://github.com/Dundir-Been-there-Dun-Dir/esp32-llm/tree/main) branch, or `idf.py set-target esp32s3`. |
| `port is in use` | Close any open monitor, or run the `kill` command the message shows. |
| `talk` says no response | Run `./setup.sh log usb0 10` and check the output. |
| Garbled output after changing the model | Flash again; the model and tokenizer must come from the same training run. |

## Going further: a robot

See [`notes/robot-hardware.txt`](notes/robot-hardware.txt) for board recommendations (an ESP32-S3
with 8 MB PSRAM allows bigger models and longer memory) and for ideas on voice input with
Espressif's ESP-SR keyword recognition.

## Credits

- [DaveBben/esp32-llm](https://github.com/DaveBben/esp32-llm): the original ESP32-S3 port
- [karpathy/llama2.c](https://github.com/karpathy/llama2.c) (MIT): inference code, and the model,
  export and tokenizer code in [`training/llama2c/`](training/llama2c/)
- [TinyStories](https://huggingface.co/datasets/roneneldan/TinyStories) and the
  [stories260K](https://huggingface.co/karpathy/tinyllamas/tree/main/stories260K) checkpoint
