# Design Specification: Otto Sustainability Officer Robot (XH-S3E + NiceNano)

**Date:** 2026-10-10  
**Status:** In Progress / Design Approved  
**Related Repositories:**
- Head Brain: `esp32-llm` (ESP-IDF C / Xtensa dual-core)
- Base Body: `../gotto` (TinyGo / nRF52840 NiceNano)

---

## 1. Executive Summary & Goal

This project transforms the existing `esp32-llm` Sustainability Officer project into a fully physical, autonomous bipedal robot that can **walk, talk, and speak**, using the Otto Ninja chassis from `gotto`.

### Current Scope & Decisions
- **Walk:** Controlled by the NiceNano (nRF52840) board in the base running TinyGo (`gotto`), utilizing 4 servos with calibrated trims and expressive gaits.
- **Speak & Talk:** Controlled by the Sparkle IoT XH-S3E board (ESP32-S3 N16R8) in the head running ESP-IDF. Speech recognition uses ESP-SR for spoken citizen confessions, an on-device LLM (`llama2.c`) generates interrogation reactions, and audio is synthesized through an I2S amplifier (MAX98357A) and speaker.
- **Proximity Perception:** The HC-SR04 ultrasonic sensor serves as the primary presence sensor to detect approaching citizens and trigger interrogations.
- **Camera (See):** Deferred for a later phase per user decision to simplify the initial build and focus on locomotion, audio, and the dialogue loop.

---

## 2. Hardware Architecture & Allocation

```
                   +--------------------------------------------------+
                   |           ROBOT HEAD: XH-S3E (ESP32-S3)          |
                   |   - 16MB Flash, 8MB PSRAM (N16R8)                |
                   |   - Sustainability Officer LLM (llama2.c)        |
                   |   - Voice Input: I2S/PDM Mic (ESP-SR MultiNet)   |
                   |   - Voice Output: MAX98357A Amp + Speaker        |
                   +-------------------------+------------------------+
                                             |
                         2-wire UART Link    | 115200 baud
                         (TX <-> RX, GND)    |
                                             v
                   +--------------------------------------------------+
                   |          ROBOT BASE: NiceNano (nRF52840)         |
                   |   - Firmware: TinyGo (gotto)                     |
                   |   - 4 Servos: 2 Legs (180°), 2 Feet (360°)       |
                   |   - HC-SR04 Ultrasonic Sensor (Eyes/Proximity)   |
                   |   - Buzzer: P0.31 (Chirps/Sound FX)              |
                   |   - 16x8 LED Matrix (Expressions & Reactions)    |
                   +--------------------------------------------------+
```

### 2.1 Head: Sparkle IoT XH-S3E (ESP32-S3 N16R8)
- **Processor:** Dual-core Xtensa LX7 @ 240MHz with vector instructions.
- **Memory:** 16MB Flash + 8MB PSRAM. PSRAM enables expanding context memory to 512 tokens and hosting larger vocabularies and audio buffers.
- **Audio Output:** MAX98357A I2S Class D amplifier driving a 4Ω or 8Ω micro speaker (synthesized TTS & sound effects).
- **Audio Input:** Digital PDM/I2S MEMS microphone feeding ESP-SR (WakeNet wake-word and MultiNet command phrase matching).
- **UART:** High-speed UART interface connected to NiceNano header pins.

### 2.2 Base: NiceNano (nRF52840)
- **Processor:** Nordic nRF52840 ARM Cortex-M4.
- **Firmware:** TinyGo (`gotto`).
- **Servos:**
  - `P0_08` (PWM1): Right Leg (180° servo for tilt)
  - `P0_17` (PWM1): Left Leg (180° servo for tilt)
  - `P0_06` (PWM2): Right Foot (360° continuous rotation)
  - `P0_20` (PWM2): Left Foot (360° continuous rotation)
- **Sensors & Peripherals:**
  - `P1_00`: HC-SR04 Trigger
  - `P0_11`: HC-SR04 Echo
  - `P0_31`: Hardware Buzzer (PWM0)
  - `P0_13`: Switched VCC Rail (`EnableVcc()`)
- **Auxiliary Pins (Identified in hardware diagnostic):**
  - `P0_22` (UART RX) and `P0_24` (UART TX) assigned for serial communication with the ESP32-S3.
  - Alternate aux pins: `P0_02`, `P1_15`, `P1_11`.

---

## 3. Inter-Board Communication Protocol

The boards communicate over a lightweight ASCII newline-delimited (`\n`) serial protocol at 115200 baud.

### Commands from Head (ESP32-S3) to Base (NiceNano):
| Command | Arguments | Description |
|---|---|---|
| `WAKEUP` | None | Runs Otto wake-up animation and greeting |
| `WALK` | `<count>` | Steps forward `<count>` paces |
| `MOONWALK`| `<count>` | Slides backward `<count>` paces |
| `PIVOT` | `<turns>` | Pivots in place (positive: right, negative: left) |
| `WIGGLE` | None | Performs celebratory/energetic body shake |
| `CURIOUS` | None | Tilts inquisitively and chirps |
| `ROLL` | `<speed> <ms>`| Continuous foot roll forward/backward |
| `STOP` | None | Immediately halts motors and returns to neutral `Home` |
| `EXPR` | `<name>` | Sets LED matrix face (e.g., `ANGRY`, `HAPPY`, `INSPECT`) |

### Telemetry from Base (NiceNano) to Head (ESP32-S3):
| Message | Arguments | Description |
|---|---|---|
| `READY` | None | Sent on boot when servos and sensors are initialized |
| `PROXIMITY`| `<mm>` | Sent periodically or when an obstacle/citizen enters range (< 300 mm) |
| `DONE` | `<command>`| Confirms completion of a motion command |

---

## 4. Software Subsystems & Implementation Roadmap

### Phase 1: Base UART Interface (`gotto`)
1. Implement a serial command listener loop in `gotto` (e.g. `examples/serial_controller/main.go` or integrated into `ninja/`).
2. Map incoming commands to functions in `ninja/expressions.go` and `ninja/ninja.go`.
3. Emit `PROXIMITY <distance_mm>` events when the ultrasonic sensor detects a person in front.

### Phase 2: Head Audio & Voice Pipeline (`esp32-llm`)
1. Target ESP32-S3 target configuration (`idf.py set-target esp32s3`).
2. Configure I2S audio driver for MAX98357A speaker playback.
3. Integrate robotic TTS synthesis (e.g., SAM - Software Automatic Mouth or ESP-TTS) and alert sound clips.
4. Integrate ESP-SR MultiNet for matching citizen phrases based on `training/generate_data.py` keywords.

### Phase 3: Brain & Persona Loop
1. Map model weights to 16MB flash partition; utilize 8MB PSRAM for 512-token sequence length.
2. Wire the orchestration loop:
   - Ultrasonic detect / Wake word -> `CURIOUS` motion -> Officer demands interrogation.
   - Citizen speaks confession -> ESP-SR identifies phrase -> Model generates verdict & next question.
   - Speak verdict via I2S -> Trigger motion (`WIGGLE` on reward, aggressive `WALK` step on penalty).

---

## 5. Current Progress State
- [x] Explored codebase and existing project context (`esp32-llm` and `../gotto`).
- [x] Milan's latest updates reviewed (`expressions.go`, `autonomous_personality`, `interactive_diagnostic` pin audit).
- [x] Architectural approach selected: Two-board UART master-subordinate (XH-S3E Head + NiceNano Base).
- [x] Hardware decisions established: XH-S3E (ESP32-S3 N16R8), MAX98357A I2S speaker, NiceNano with 4 servos & ultrasonic sensor.
- [x] Section 1 of Architecture Design presented and approved.
- [ ] Next session: Detail Section 2 (Serial Protocol & State Machine implementation plan), followed by Phase 1 coding.
