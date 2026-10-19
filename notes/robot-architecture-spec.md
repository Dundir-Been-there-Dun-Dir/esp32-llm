# Otto Sustainability Officer Robot - Architecture & Progress Snapshot

**Last Updated:** 2026-10-19  
**Session Memory & Full Handoff:** `MEMORY.md`  
**Buying Guide:** `docs/hardware-to-buy.html`  
**Live Site:** https://dundir-been-there-dun-dir.github.io/esp32-llm/  
**Specification Doc:** `docs/superpowers/specs/2026-10-10-otto-robot-design.md`

## Quick Summary
We are turning `esp32-llm` into a walking, talking, speaking robot using the Otto Ninja chassis from `../gotto`.

### Hardware Setup
1. **Robot Head:** Sparkle IoT XH-S3E (ESP32-S3 N16R8: 16MB Flash, 8MB PSRAM)
   - Running `llama2.c` Sustainability Officer persona (expanded to 512 context tokens in PSRAM).
   - Voice Input: I2S/PDM mic with ESP-SR MultiNet matching spoken confessions.
   - Voice Output: MAX98357A I2S amplifier + micro speaker.
   - Vision: Camera is deferred for now to keep the focus on locomotion, audio, and conversational personality.
2. **Robot Base:** NiceNano (nRF52840) running TinyGo (`gotto`)
   - 4 Servos: 2 legs (180° for tilt/steps) + 2 continuous feet (360° for walking/rolling).
   - Proximity: HC-SR04 ultrasonic sensor for citizen presence detection.
   - Buzzer: P0.31 for chirps and sound effects.
   - Expressions: 16x8 LED Matrix face.
   - UART Link: P0_22 (RX) and P0_24 (TX) connected to ESP32-S3.

### Milan's Fresh Updates in `../gotto` (Oct 10, 2026)
- [`ninja/expressions.go`](../gotto/ninja/expressions.go): Added `HappyWiggle()`, `Moonwalk()`, `PivotTurn()`, `CuriousLook()`, `WakeUp()`.
- [`examples/autonomous_personality/main.go`](../gotto/examples/autonomous_personality/main.go): Calibrated servo trims and movement sequence.
- [`examples/interactive_diagnostic/main.go`](../gotto/examples/interactive_diagnostic/main.go): Comprehensive shield pinout audit mapping auxiliary pins `P0_22`, `P0_24`, `P0_02`, `P1_15`, `P1_11`.

### Next Steps When Resuming
1. Implement the UART command receiver in `gotto` (mapping serial commands to `ninja/expressions.go`).
2. Configure ESP-IDF for ESP32-S3 (XH-S3E) audio output (MAX98357A) and ESP-SR voice matching.
3. Connect the inter-board state machine and test the interactive interrogation loop.
