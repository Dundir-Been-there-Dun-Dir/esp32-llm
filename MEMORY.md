# Project Memory & Session Handoff: OttoNinja Swarms & AI AR Glasses

**Last Updated:** 2026-10-19  
**Git Branch:** `esp32-wroom`  
**GitHub Pages Site:** [https://dundir-been-there-dun-dir.github.io/esp32-llm/](https://dundir-been-there-dun-dir.github.io/esp32-llm/)  
**Buying Guide:** [https://dundir-been-there-dun-dir.github.io/esp32-llm/hardware-to-buy.html](https://dundir-been-there-dun-dir.github.io/esp32-llm/hardware-to-buy.html)

---

## 1. System Ecosystem & Architectural Decisions

We are building an integrated tactical ecosystem consisting of:
1. **OttoNinja Swarm Units:** Autonomous bipedal robots running an ESP32-S3 brain (sustainability officer persona, speech, edge vision) and a NiceNano v2 base (4 servos, ultrasonic proximity), coordinating over a **2.4 GHz LoRa (Semtech SX1280)** mesh network (~200 kbps, zero EU duty cycle limits).
2. **AR Display Glasses ("Research-Visor"):** Built on top of **BasedHardware OpenGlass** (Seeed Studio XIAO ESP32-S3 Sense + camera + mic + LiPo + glasses clip), paired with a **Raspberry Pi** edge coordinator and an optical HUD attachment.
3. **Workshop Access:** Full access to **Bitlair hackerspace** in Amersfoort (Resin SLA printers for optical pods, FDM printers for chassis, laser cutter for acrylic faceplates, sewing lab for shoulder harness, electronics lab for SMD soldering).

---

## 2. Hardware Inventory & Current Status

### A. Hardware In Hand (Verified)
* **OttoNinja Base (`../gotto`):**
  * Nice!Nano v2 (nRF52840) seated in custom red shield board.
  * 4 Servos: 2× SG90 180° hip tilt (`P0_08`, `P0_17`) + 2× SG90 360° continuous feet (`P0_06`, `P0_20`). Calibrated trims verified.
  * HC-SR04 ultrasonic sensor (`P1_00` Trig, `P0_11` Echo).
  * Buzzer on Header 031 (`P0_31`).
  * Switched VCC Rail controlled by `P0_13` (`board.EnableVcc()`).
  * Battery status: Servos move when powered via battery + jumper to `VIN`. To run 100% untethered without USB cable, bridge `BAT` to `5V` on the white 3-pin connector.
  * Free auxiliary serial pins: `P0_22` (UART RX) and `P0_24` (UART TX).
* **OpenGlass Eyewear Hardware:**
  * Materials in hand from [BasedHardware/OpenGlass](https://github.com/BasedHardware/openglass).
  * Seeed Studio XIAO ESP32-S3 Sense (OV2640 camera daughterboard + PDM microphone + 8MB PSRAM).
  * 3.7V 250mAh LiPo battery + 3D printed glasses mount clip.
* **Compute & Lab Gear:**
  * Raspberry Pis (Pi 3 / 4 / Zero) available to act as Local Edge Hub / VLM Vision Engine.
  * Arduinos available for bench testing.

### B. Hardware to Procure / Options
* Documented with direct AliExpress / store links in [`docs/hardware-to-buy.html`](file:///home/chris/Nextcloud/Github/esp32-llm/docs/hardware-to-buy.html):
  * **Option 1A (Otto Head Upgrade):** XIAO ESP32-S3 Sense ($13.99), SX1280 2.4GHz LoRa ($6.50), MAX98357A I2S amp ($1.20), 20mm speaker ($0.80), 14500 Li-Ion ($2.50) $\rightarrow$ **$26.49 total**.
  * **Option 2A (AR Glasses Micro-OLED HUD):** 0.39" Micro-OLED + Optical Prism HDMI Kit ($78.00), SX1280 LoRa Master ($6.50), Throat Mic ($7.80), Bone Conduction Transducer ($3.90) $\rightarrow$ **$105.15 total**.
  * **Option 2B (DIY Pepper's Ghost HUD):** 0.96" SPI OLED ($1.40) + 25mm Achromatic Lens ($5.50) + 50/50 Beam Splitter ($4.20) $\rightarrow$ **$15.65 total**.

---

## 3. Software, Firmware & CAD Assets on Disk

| Repository | File / Path | Description |
|---|---|---|
| `esp32-llm` | [`cad/head_mount_bracket.scad`](file:///home/chris/Nextcloud/Github/esp32-llm/cad/head_mount_bracket.scad) | Parametric dual-deck 3D-printable tray fitting inside Otto head for XIAO S3, SX1280, MAX98357A, and speaker. |
| `esp32-llm` | [`docs/robot-head-cad-and-pinout.md`](file:///home/chris/Nextcloud/Github/esp32-llm/docs/robot-head-cad-and-pinout.md) | Complete electrical netlist and pinout assignments between XIAO S3, SX1280, MAX98357A, and NiceNano. |
| `esp32-llm` | [`main/serial_bridge.h`](file:///home/chris/Nextcloud/Github/esp32-llm/main/serial_bridge.h) / [`.c`](file:///home/chris/Nextcloud/Github/esp32-llm/main/serial_bridge.c) | ESP-IDF UART driver with FreeRTOS background RX monitor, proximity event callbacks, and command sender. |
| `esp32-llm` | [`docs/full-hardware-report.qmd`](file:///home/chris/Nextcloud/Github/esp32-llm/docs/full-hardware-report.qmd) / [`.html`](file:///home/chris/Nextcloud/Github/esp32-llm/docs/full-hardware-report.html) | Quarto standalone technical report with radio calculations, battery runtime modeling, and Mermaid architecture diagrams. |
| `esp32-llm` | [`docs/hardware-to-buy.qmd`](file:///home/chris/Nextcloud/Github/esp32-llm/docs/hardware-to-buy.qmd) / [`.html`](file:///home/chris/Nextcloud/Github/esp32-llm/docs/hardware-to-buy.html) | Dedicated procurement guide with direct purchase links, costs, and option subtotals. |
| `esp32-llm` | [`.github/workflows/pages.yml`](file:///home/chris/Nextcloud/Github/esp32-llm/.github/workflows/pages.yml) | Automated GitHub Actions CI/CD pipeline building Quarto docs and deploying to GitHub Pages on every push. |
| `gotto` | [`examples/serial_bridge/main.go`](file:///home/chris/Nextcloud/Github/gotto/examples/serial_bridge/main.go) | TinyGo UART command receiver on `machine.UART0` (`P0_22`/`P0_24`). Dispatches `WAKEUP`, `WALK`, `WIGGLE`, `CURIOUS`, `MOONWALK`, `PIVOT`, and reports `PROXIMITY <mm>`. Verified compiling with TinyGo 0.39.0. |
| `gotto` | [`ninja/expressions.go`](file:///home/chris/Nextcloud/Github/gotto/ninja/expressions.go) | Expressive behaviors: `HappyWiggle()`, `Moonwalk()`, `PivotTurn()`, `CuriousLook()`, `WakeUp()`, `YawnAndSleep()`. |

---

## 4. Immediate Next Steps for Next Session

1. **Physical Untethering of NiceNano:**
   - Bridge `BAT` to `5V` on the white battery connector (or attach a short USB power jumper).
   - Flash `gotto/examples/serial_bridge/main.go` using `python3 scripts/flash.py examples/serial_bridge` in `gotto`.
2. **Wire Inter-Board UART Link:**
   - Connect NiceNano Header 022 (`P0_22` RX) to ESP32 TX (`D4`).
   - Connect NiceNano Header 024 (`P0_24` TX) to ESP32 RX (`D5`).
   - Connect common GND.
   - Run interactive interrogation test: verify `PROXIMITY` triggers officer dialogue and `WALK`/`WIGGLE` commands execute physically.
3. **3D Printing at Bitlair:**
   - Slice and print `cad/head_mount_bracket.scad` on Prusa or resin printer at Bitlair.
4. **OpenGlass Stream & Raspberry Pi Gateway:**
   - Flash OpenGlass firmware to the XIAO ESP32-S3 Sense.
   - Write a lightweight Python listener on the Raspberry Pi that receives OpenGlass frames, runs vision inference, and relays command packets to the OttoNinja serial bridge.
