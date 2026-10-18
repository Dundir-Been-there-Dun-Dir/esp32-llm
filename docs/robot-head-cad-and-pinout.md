# OttoNinja Head: 3D CAD Mounting & Inter-Board Pinout Specification

**Date:** 2026-10-18  
**Context:** Hardware integration guide for OttoNinja biped robot combining the **NiceNano v2** base (`gotto`) with a **Seeed XIAO ESP32S3 Sense** head, **Semtech SX1280** 2.4 GHz LoRa transceiver, and **MAX98357A** I2S audio amplifier.

---

## 1. Electrical Pinout & Interconnection Schematic

```
                                  +5V / VIN Rail (Battery via Boost PMIC)
                                  |
               +------------------+------------------+
               |                  |                  |
               v (5V / VIN)       v (5V)             v (5V)
        +-------------+    +-------------+    +-------------+
        | XIAO S3     |    | MAX98357A   |    | NiceNano    |
        | Sense (Head)|    | Amp         |    | Shield Base |
        +------+------+    +------+------+    +------+------+
               | (3.3V)           |                  |
               v                  |                  v (Switched VCC P0.13)
        +-------------+           |           +-------------+
        | SX1280 LoRa |           |           | 4x Servos   |
        | Module      |           |           | HC-SR04     |
        +-------------+           |           +-------------+
               |                  |                  |
               +--- 2-Wire Serial Command Link ------+
                    (Head TX <-> Base RX, Baud 115200)
```

### 1.1 XIAO ESP32S3 Sense Pin Assignments (Head Brain)

The Seeed XIAO ESP32S3 Sense features 14 castellated pins and an on-board camera daughterboard with OV2640 and PDM microphone.

| XIAO Pin | ESP32-S3 GPIO | Net / Function | Connects To | Notes |
|---|---|---|---|---|
| **D0** | GPIO 1 | `LORA_NSS` | SX1280 Pin `NSS` (Chip Select) | SPI CS active low |
| **D1** | GPIO 2 | `LORA_BUSY`| SX1280 Pin `BUSY` | High indicates radio busy |
| **D2** | GPIO 3 | `LORA_DIO1`| SX1280 Pin `DIO1` (IRQ) | Packet RX/TX done interrupt |
| **D3** | GPIO 4 | `LORA_RST` | SX1280 Pin `NRESET` | Active-low hardware reset |
| **D4** | GPIO 5 | `UART_TX`  | NiceNano Header `022` (`P0_22` RX) | Serial commands from Head to Base |
| **D5** | GPIO 6 | `UART_RX`  | NiceNano Header `024` (`P0_24` TX) | Telemetry from Base to Head |
| **D7** | GPIO 44 | `I2S_BCLK` | MAX98357A `BCLK` | Audio bit clock |
| **D8** | GPIO 7 | `I2S_LRC`  | MAX98357A `LRC` (Word Select) | Left/Right word clock |
| **D6** | GPIO 43 | `I2S_DOUT` | MAX98357A `DIN` | Serial PCM audio data out |
| **D9** | GPIO 8 | `SPI_MOSI` | SX1280 Pin `MOSI` | Hardware SPI Data Out |
| **D10**| GPIO 9 | `SPI_SCK`  | SX1280 Pin `SCK` | Hardware SPI Clock |
| **3V3**| 3.3V Out | `VCC_3V3`  | SX1280 Pin `VCC` | Powers SX1280 (3.3V max, ~60mA peak) |
| **GND**| Ground | `GND`      | Common Ground Plane | Shared between Head, Amp & Base |
| **5V** | 5V In | `VBUS / 5V`| Main 5V Power Rail | From Battery PMIC Step-Up |

*Note: The camera (OV2640) and digital microphone connect internally through the board-to-board high-density connector on the underside of the XIAO Sense, leaving all external GPIOs completely free for SPI, I2S, and UART.*

---

### 1.2 Semtech SX1280 (2.4 GHz LoRa) Module Pinout

| SX1280 Pin | Connected To | Voltage Level | Description |
|---|---|---|---|
| **VCC** | XIAO `3V3` | 3.3V | Operating voltage (1.8V – 3.7V) |
| **GND** | System `GND` | 0V | Ground |
| **MISO**| XIAO `D10` (or shared MISO) | 3.3V | Master In Slave Out |
| **MOSI**| XIAO `D9` | 3.3V | Master Out Slave In |
| **SCK** | XIAO `D10` | 3.3V | SPI Serial Clock |
| **NSS** | XIAO `D0` | 3.3V | SPI Chip Select (Active Low) |
| **NRESET**| XIAO `D3` | 3.3V | Chip Reset (Active Low) |
| **BUSY**| XIAO `D1` | 3.3V | Status indicator: wait while HIGH |
| **DIO1**| XIAO `D2` | 3.3V | Configurable IRQ (RX Done / TX Done) |
| **ANT** | IPEX / Ceramic Chip | RF | 2.4 GHz miniature antenna |

---

### 1.3 MAX98357A I2S Amplifier & Speaker

| MAX98357A Pin | Connected To | Notes |
|---|---|---|
| **VIN** | Main `5V` Rail | 5V provides 3.2W max output into 4Ω (approx. 1.5W into 8Ω) |
| **GND** | System `GND` | Ground |
| **DIN** | XIAO `D6` (`GPIO 43`) | I2S Serial Data |
| **BCLK** | XIAO `D7` (`GPIO 44`) | I2S Bit Clock (approx. 1.024 MHz for 16kHz audio) |
| **LRC** | XIAO `D8` (`GPIO 7`) | I2S Left/Right Clock (16 kHz audio sample rate) |
| **SD_MODE** | Unconnected / 100k to 5V | Mixed L+R stereo-to-mono downmix |
| **GAIN** | Unconnected | Default 9dB gain (clean, unclipped speech) |
| **SPK+ / SPK-**| 20mm 8Ω 1W Micro Speaker | Micro JST 1.25mm connection |

---

### 1.4 NiceNano v2 Shield Pin Assignments (Base Motion Controller)

The base runs TinyGo firmware with the pinout validated in `../gotto/HARDWARE_STATUS.md`.

| Function | NiceNano Pin | Shield Header | Type / PWM Peripheral |
|---|---|---|---|
| **Right Foot** | `machine.P0_06` | Header `006` | 360° Continuous Servo (`PWM2`) |
| **Right Leg**  | `machine.P0_08` | Header `008` | 180° Tilt Servo (`PWM1`) |
| **Left Leg**   | `machine.P0_17` | Header `017` | 180° Tilt Servo (`PWM1`) |
| **Left Foot**  | `machine.P0_20` | Header `020` | 360° Continuous Servo (`PWM2`) |
| **UART RX**    | `machine.P0_22` | Header `022` | Connects to XIAO S3 `D4` (`TX`) |
| **UART TX**    | `machine.P0_24` | Header `024` | Connects to XIAO S3 `D5` (`RX`) |
| **US Trigger** | `machine.P1_00` | Header `100` | Ultrasonic HC-SR04 Trigger |
| **US Echo**    | `machine.P0_11` | Header `011` | Ultrasonic HC-SR04 Echo |
| **Hardware Buzzer**| `machine.P0_31` | Header `031` | PWM Tone / Chirp Output (`PWM0`) |
| **Switched VCC Rail**| `machine.P0_13` | Internal MOSFET | Powers on/off servo & sensor rail |

> ⚠️ **CRITICAL WARNING:** NiceNano Pin `029` (`P0.29`) has a hard low-impedance ground path on this shield. Never configure or drive `P0.29` as a high output.

---

## 2. 3D Printable CAD Mounting Tray

The mounting tray CAD model is located in [`cad/head_mount_bracket.scad`](file:///home/chris/Nextcloud/Github/esp32-llm/cad/head_mount_bracket.scad).

### 2.1 Mechanical Layout & Features
* **Outer Dimensions:** 54.0 mm (W) × 44.0 mm (D) × 24.0 mm (H).
* **Dual-Tier Density:**
  * **Lower Deck:** Houses the 20mm round micro-speaker directly over an acoustic grille, plus snap posts for the MAX98357A amplifier board.
  * **Upper Deck:** Features a vertical retention bezel holding the Seeed XIAO ESP32S3 Sense aligned with a 9mm front circular aperture for the OV2640 camera lens.
  * **Lateral Bay:** A dedicated 2.2mm vertical PCB edge card slot retains the SX1280 module securely on the right flank with an antenna cutout.
  * **Floor Pass-through:** A 10×8mm cable duct routes the 4-wire umbilical (5V, GND, TX, RX) down through the neck to the NiceNano base.

### 2.2 Slicing & Fabrication Guidelines (Bitlair)

#### For FDM Printing (Prusa MK3S / MK4 / Bambu Lab):
* **Material:** Matte Black PLA or PETG (prevents light reflections).
* **Layer Height:** 0.20 mm (or 0.16 mm for clean snap clips).
* **Infill:** 20% Gyroid or Grid.
* **Perimeters/Walls:** 3 perimeters (gives solid screw boss strength for M2 screws).
* **Supports:** Tree supports touching buildplate only (for the front camera aperture).

#### For Resin / SLA Printing (Formlabs / Elegoo at Bitlair):
* **Material:** Tough/Standard Black Resin.
* **Orientation:** Angled 45° with drain holes facing away from the front bezel.
* **Post-processing:** 10-minute IPA wash, thoroughly dry, followed by 15-minute UV cure.
