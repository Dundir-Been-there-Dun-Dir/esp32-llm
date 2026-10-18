# Hardware Architecture & Trade-Off Analysis: OttoNinja Swarm & AI AR Glasses

**Date:** 2026-10-18  
**Context:** Hardware theorization for autonomous/swarm OttoNinja biped robots (with edge vision and LoRa mesh) and a wearable monocular AR Display HUD ("Research-Visor" / Borg-style) with AI vision integration.

---

## Executive Summary & System Overview

The target end-state is an integrated ecosystem consisting of:
1. **OttoNinja Units (Swarm):** Autonomous bipedal robots using the `gotto` NiceNano chassis for locomotion and an ESP32-S3 brain for personality, speech, and edge vision. They coordinate over a **LoRa swarm mesh**, sharing telemetry, target detections, and swarm maneuvers.
2. **AR Display Glasses (Tactical Overwatch HUD):** A monocular, see-through wearable HUD ("Research-Visor") featuring a lightweight head module (< 45g) and a half-plate shoulder pack housing the SBC, LoRa transceiver, and battery. The user sees real-time swarm telemetry, target tracking overlays, and AI vision captions, and commands the swarm hands-free via voice.

```
                    +-------------------------------------------------------+
                    |           AR DISPLAY GLASSES (RESEARCH-VISOR)         |
                    | Head: 0.39" Micro-OLED + Prism + Pinhole Camera + Mic |
                    | Shoulder Pack: Radxa Zero 3W / Pi Zero 2W + SX1280    |
                    +---------------------------+---------------------------+
                                                |
                                                | 2.4 GHz LoRa / Sub-GHz LoRa
                                                | (Swarm Mesh Protocol)
                                                v
               +--------------------------------+--------------------------------+
               |                                                                 |
               v                                                                 v
+------------------------------+                                  +------------------------------+
|     OTTONINJA UNIT 01        |                                  |     OTTONINJA UNIT 02...     |
| Head: ESP32-S3 + Grove V2/Cam|                                  | Head: ESP32-S3 + Grove V2/Cam|
| Base: NiceNano (4 Servos)    |                                  | Base: NiceNano (4 Servos)    |
| Radio: SX1280 2.4GHz LoRa    |                                  | Radio: SX1280 2.4GHz LoRa    |
+------------------------------+                                  +------------------------------+
```

---

## 1. Radio & Swarm Networking Trade-Offs & Calculations

The fundamental design constraint when combining **LoRa** and **Vision** is that **raw video cannot be streamed over LoRa**. Instead, vision processing must occur on the edge (extracting bounding boxes, classes, distances, or OCR strings), transmitting only compact semantic packets over the air.

### 1.1 Comparative Radio Matrix

| Parameter | Sub-GHz LoRa (SX1262 @ 868MHz EU / 915MHz US) | 2.4 GHz LoRa (Semtech SX1280) | ESP-NOW (2.4 GHz Wi-Fi PHY) |
|---|---|---|---|
| **Max Raw Bitrate** | 5.47 kbps (SF7/125kHz) to 21.8 kbps (SF7/500kHz) | **253.9 kbps** (SF5/1625kHz) to 44.4 kbps (SF7/812kHz) | **1,000 to 2,000 kbps** (1–2 Mbps) |
| **Indoor / Building Range** | **300m – 1 km** (Superior penetration) | 100m – 300m | 30m – 80m |
| **Outdoor Line-of-Sight** | **3 km – 10 km** | 800m – 2 km | 100m – 200m |
| **Antenna Length** | 8.6 cm (quarter-wave whip / PCB coil) | **3.1 cm** (or tiny ceramic chip antenna) | Ceramic PCB trace antenna |
| **EU Legal Duty Cycle** | **Strict 1.0%** (36 sec TX per hour on 868 MHz) | **No duty cycle limit** (ISM 2.4 GHz) | No duty cycle limit |
| **Payload Capacity** | 256 bytes per packet | 256 bytes per packet | 250 bytes per frame |
| **Can send Thumbnails?** | **No** (Violates duty cycle, takes >3.7s per frame) | **Yes** (160×120 grayscale JPEG in ~0.12s) | **Yes** (Full QVGA in <0.02s) |
| **Global Compatibility** | Regional (868 EU, 915 US, 433 Asia) | **Worldwide license-free** | Worldwide license-free |

### 1.2 Mathematical Time-on-Air (ToA) Calculations

#### Scenario A: Telemetry Heartbeat Packet (24 Bytes)
*(Robot ID, Battery %, Status Enum, Ultrasonic Distance, Bearing, State)*

- **Formula:** 
  $$\text{Symbol Duration } T_s = \frac{2^{SF}}{BW}$$
  $$\text{ToA} = (N_{\text{preamble}} + 4.25) \cdot T_s + N_{\text{payload\_symbols}} \cdot T_s$$

- **Sub-GHz LoRa (SX1262, SF7, BW 125 kHz):**
  - $T_s = \frac{128}{125000} = 1.024\text{ ms}$
  - Preamble (8 symbols) + Payload (24 bytes $\approx 28$ symbols)
  - $\text{ToA} \approx \mathbf{46.3\text{ ms}}$
  - *Duty cycle impact:* At 1% duty cycle, a node can transmit at most once every **4.63 seconds**.

- **2.4 GHz LoRa (SX1280, SF5, BW 1625 kHz):**
  - $T_s = \frac{32}{1625000} = 0.0197\text{ ms}$
  - Preamble (12 symbols) + Payload (24 bytes $\approx 55$ symbols)
  - $\text{ToA} \approx \mathbf{1.82\text{ ms}}$ (over **25× faster!**)
  - *Duty cycle impact:* No limit. Can burst 20 updates per second without channel saturation.

#### Scenario B: Low-Resolution Visual Snapshot (160×120 Grayscale JPEG = 2.5 KB / 20,480 Bits)
- **Sub-GHz LoRa (SX1262, SF7, 125 kHz):**
  - Raw time on air: $\frac{20480\text{ bits}}{5470\text{ bps}} = \mathbf{3.74\text{ seconds}}$
  - *Verdict:* Impractical. Consumes >10% of the entire hourly legal TX allowance in one snapshot.
- **2.4 GHz LoRa (SX1280, SF5, 1625 kHz):**
  - Effective throughput $\approx 180\text{ kbps}$ (after overhead).
  - Transmission time: $\frac{20480\text{ bits}}{180000\text{ bps}} = \mathbf{0.114\text{ seconds (114 ms)}}$.
  - *Verdict:* Highly practical. The glasses can request an image thumbnail from any scout OttoNinja, arriving in ~150 ms with ACK.

---

## 2. OttoNinja Swarm Hardware Options & Calculations

### 2.1 Hardware Options for the Robot Head

| Component Option | Compute & NPU | Vision Hardware | Radio | Cost (est.) | Pros | Cons |
|---|---|---|---|---|---|---|
| **Option A: LilyGO T3S3 + OV2640** | ESP32-S3 (240MHz, 2MB PSRAM, 4MB Flash) | OV2640 (DVP) | Onboard SX1262 (868MHz) or SX1280 (2.4GHz) | ~$26 | All-in-one board, integrated battery PMIC, OLED screen. | Only 2MB PSRAM limits large on-device LLM context; camera takes many GPIOs. |
| **Option B (Recommended): XIAO ESP32S3 Sense + SX1280 Module** | ESP32-S3 (240MHz, **8MB PSRAM**, 8MB Flash) | OV2640 + Digital Mic included | External SPI SX1280 module (~$6) | **~$20 total** | Extremely tiny (21×17.5mm), fits in any 3D Otto head, 8MB PSRAM allows 512-token LLM + audio buffers. | Requires wiring 5 SPI pins to external LoRa breakout. |
| **Option C: XH-S3E (Current) + Grove Vision AI V2 + SX1280** | ESP32-S3 (16MB Flash, **8MB PSRAM**) + **Himax WiseEye2 NPU** | Dual-core Cortex-M55 + Ethos-U55 NPU + OV5647 (CSI) | External SPI SX1280 module | ~$42 total | True high-speed edge AI (runs YOLOv8 at 30 FPS, object classification, pose). Zero load on ESP32-S3. | Highest cost and largest physical footprint in head. |

### 2.2 Base Locomotion (NiceNano / TinyGo)
Retains the proven hardware stack from `../gotto`:
- **MCU:** NiceNano v2 (Nordic nRF52840).
- **Actuators:** 4 Servos:
  - 2× 180° Micro Servos (SG90 / MG90S) for hip/tilt (`P0_08`, `P0_17`).
  - 2× 360° Continuous Rotation Servos for foot rolling/stepping (`P0_06`, `P0_20`).
- **Obstacle Sensor:** HC-SR04 ultrasonic distance sensor (`P1_00`, `P0_11`).
- **Power Switch:** Switched VCC Rail via P-channel MOSFET (`P0_13`).
- **Face:** 16×8 LED Matrix driven via I2C for expressions.

### 2.3 OttoNinja Power Consumption & Battery Life Calculation

- **Component Power Draw @ 5V Rail:**
  - Base NiceNano (nRF52840): 15 mA (0.075 W)
  - 4 Servos (Average during active bipedal walking): 320 mA (1.60 W)
  - Servos (Quiescent holding torque): 40 mA (0.20 W)
  - Head ESP32-S3 (LLM execution + active listen): 120 mA (0.60 W)
  - SX1280 LoRa Transceiver (RX listening): 12 mA (0.06 W)
  - SX1280 LoRa Transceiver (TX burst @ +12.5dBm): 60 mA (0.30 W, duty cycle < 5%)
  - Camera (OV2640 active / streaming): 60 mA (0.30 W)
  - Audio MAX98357A Amp (during speech): 150 mA (0.75 W)

- **Operational Duty Cycle Modeling:**
  - **Patrol Mode (Walking & scanning):** 320 mA (servos) + 120 mA (S3) + 60 mA (cam) + 15 mA (base/radio) = **515 mA @ 5V (2.58 W)**.
  - **Interrogate / Speech Mode:** 40 mA (servos hold) + 120 mA (S3) + 150 mA (audio) + 60 mA (cam) = **370 mA @ 5V (1.85 W)**.
  - **Sentry / Idle Standby Mode:** 40 mA (servos hold) + 80 mA (S3 listen) + 15 mA (radio RX) = **135 mA @ 5V (0.68 W)**.
  - **Weighted Average (35% Patrol, 25% Interrogate, 40% Sentry):**
    $$\bar{I} = (0.35 \times 515) + (0.25 \times 370) + (0.40 \times 135) \approx \mathbf{327\text{ mA @ 5V (1.63 W)}}$$

- **Battery Run-Time Projections:**
  - *Option 1: Single 16340 Li-Ion (3.7V, 750 mAh = 2.78 Wh):*
    - Usable energy after 85% boost conversion efficiency = $2.78 \times 0.85 = 2.36\text{ Wh}$.
    - **Run-time:** $\frac{2.36\text{ Wh}}{1.63\text{ W}} \approx \mathbf{1.45\text{ hours (87 minutes)}}$.
  - *Option 2: Single 14500 Li-Ion (3.7V, 1000 mAh = 3.70 Wh):*
    - Usable energy = $3.70 \times 0.85 = 3.15\text{ Wh}$.
    - **Run-time:** $\frac{3.15\text{ Wh}}{1.63\text{ W}} \approx \mathbf{1.93\text{ hours (116 minutes)}}$.
  - *Option 3: 18650 "Backpack" Cell (3.7V, 3200 mAh = 11.84 Wh):*
    - Usable energy = $11.84 \times 0.85 = 10.06\text{ Wh}$.
    - **Run-time:** $\frac{10.06\text{ Wh}}{1.63\text{ W}} \approx \mathbf{6.17\text{ hours}}$.

---

## 3. AR Display Glasses ("Research-Visor") Architecture & Trade-Offs

Following the architectural philosophy in `../ResearchReady/Research-Visor`, the device is a **monocular utilitarian HUD** prioritizing capability, low head weight, and hands-free voice interaction over consumer styling.

### 3.1 Display & Optics Comparison

| Optical Solution | Display Type & Resolution | Field of View (FOV) & Eye Relief | Weight on Frame | Cost | Key Advantage | Key Disadvantage |
|---|---|---|---|---|---|---|
| **Option A (Recommended): 0.39" Micro-OLED + Birdbath/Prism Module** | Sony ECX336C / equivalent (1024×768 or 1920×1080) | ~28°–32° FOV, 20mm eye relief, virtual screen appears at ~2.5m | **~25g** (lens + screen) | ~$75 – $110 (kit with driver board) | Integrated collimating optics; crystal clear text; high brightness (1000+ nits) readable in daylight. | Requires small driver board in shoulder pack or temple. |
| **Option B: Waveshare 1.51" Transparent OLED** | 128×64 Light Blue monochrome | Variable (direct view) | ~18g | ~$20 | Very cheap; striking cyberpunk aesthetic. | **Cannot focus at near distance.** Human eye near-point is 10–15cm; at 3cm from eye, text is a complete blur without custom collimator lenses. |
| **Option C: DIY 0.96" OLED + 45° Beam Splitter (Pepper's Ghost)** | 128×64 SSD1306 + Achromatic doublet lens (f=25mm) | ~15° FOV | ~35g (3D printed housing) | ~$25 | Fully DIY 3D printable; simple I2C/SPI interface to any MCU. | Bulky optical snout; low resolution (only 2-3 lines of text); narrow eye box. |

### 3.2 Compute Split: Head vs. Half-Plate Shoulder Mount

To prevent neck fatigue and thermal discomfort, the system follows a split architecture:

```
[ HEAD MODULE: < 45 grams ]
 - 0.39" Micro-OLED + Optical Prism (Right Eye)
 - Micro Pinhole Camera (IMX219 / OV5640)
 - Bone-conduction audio transducer + Throat microphone
           |
           | Ultra-flexible multi-conductor umbilical (Type-C / FFC)
           v
[ LEFT SHOULDER HALF-PLATE: ~180 grams ]
 - SBC: Radxa Zero 3W (Quad Cortex-A55 @ 1.6GHz, 0.8 TOPS NPU, 1GB RAM)
        or Raspberry Pi Zero 2W
 - Semtech SX1280 2.4 GHz LoRa Transceiver + Ceramic Antenna
 - Audio Codec & Amp (I2S)
 - Power: 1× 18650 or 21700 Li-Ion Cell (3500–5000 mAh) + 5V Boost PMIC
 - Emergency power kill switch
```

### 3.3 Glasses Compute: Radxa Zero 3W vs. Raspberry Pi Zero 2W

| Feature | Radxa Zero 3W | Raspberry Pi Zero 2W | Winner / Recommendation |
|---|---|---|---|
| **SoC** | Rockchip RK3566 (Quad Cortex-A55 @ 1.6GHz) | Broadcom BCM2710A1 (Quad Cortex-A53 @ 1.0GHz) | **Radxa Zero 3W** (60% faster CPU) |
| **NPU (AI)** | **0.8 TOPS NPU** (Runs MobileNet / YOLOv8 at 25 FPS) | **None** (CPU inference only, ~2-4 FPS) | **Radxa Zero 3W** (Essential for local on-body vision) |
| **RAM** | 1GB / 2GB / 4GB LPDDR4 | 512 MB LPDDR2 (tight for modern OS + CV) | **Radxa Zero 3W** |
| **Video Out** | Micro-HDMI (Direct to Micro-OLED driver board) | Mini-HDMI | Tie |
| **Camera Port** | MIPI CSI-2 (Raspberry Pi camera compatible) | MIPI CSI-2 | Tie |
| **Power Consumption** | 1.4W idle, 2.8W peak | 1.1W idle, 2.3W peak | Pi Zero 2W slightly lower power |
| **Price** | ~$20 (1GB) to $26 (2GB) | ~$15 (often scalped / stock sensitive) | **Radxa Zero 3W** |

### 3.4 Glasses Battery Life Calculation

- **Continuous Power Consumption (Glasses + Shoulder Pack):**
  - Radxa Zero 3W (Active Linux, video capture, NPU inference): 2.10 W
  - 0.39" Micro-OLED + HDMI Driver Board: 0.65 W
  - Pinhole Camera (CSI): 0.20 W
  - SX1280 LoRa Transceiver (Listening & periodic beaconing): 0.08 W
  - Bone conduction / audio circuit: 0.12 W
  - **Total System Power:** $\mathbf{3.15\text{ W (approx. 630 mA @ 5V)}}$

- **Battery Run-Time (from Single 18650 Cell, 3500 mAh @ 3.7V = 12.95 Wh):**
  - With 90% DC-DC boost conversion efficiency: Usable energy = $12.95 \times 0.90 = 11.65\text{ Wh}$.
  - **Run-time:** $\frac{11.65\text{ Wh}}{3.15\text{ W}} \approx \mathbf{3.70\text{ hours continuous operational use}}$.
  - *(Using a 21700 cell [5000 mAh = 18.5 Wh] extends runtime to **5.28 hours**).*

---

## 4. Total Bill of Materials (BOM) & Cost Breakdown

### Subsystem 1: Single OttoNinja Swarm Unit (Head + Base)
| Item | Part / Model | Est. Price (USD) |
|---|---|---|
| Head Brain & Cam | Seeed Studio XIAO ESP32S3 Sense (8MB Flash, 8MB PSRAM, OV2640, Mic) | $14.00 |
| LoRa Radio | Semtech SX1280 2.4 GHz SPI Breakout Module (Ebyte E28 or equivalent) | $6.50 |
| Audio Output | MAX98357A I2S Class-D Amp + 1W 8Ω Micro Speaker | $2.50 |
| Base Controller | NiceNano v2 (Nordic nRF52840) | $12.00 |
| Servos | 2× SG90 180° + 2× SG90 360° Continuous Servos | $8.00 |
| Proximity Sensor | HC-SR04 Ultrasonic Sensor | $1.50 |
| Face Matrix | 16×8 LED Matrix (I2C) | $3.50 |
| Chassis & Hardware | 3D Printed Otto Ninja Chassis + Fasteners + Wiring | $5.00 |
| Battery & Power | 14500 Li-Ion Cell (1000mAh) + TP4056 USB-C Charger & 5V Step-Up Board | $4.50 |
| **Total per OttoNinja** | | **~$57.50** |

### Subsystem 2: AR Display Glasses & Shoulder Pack ("Research-Visor")
| Item | Part / Model | Est. Price (USD) |
|---|---|---|
| Optical HUD Module | 0.39" Micro-OLED (1024×768) + Optical Prism Assembly + HDMI Driver Board | $85.00 |
| Glasses Frame & Mount | 3D Printed lightweight monocular frame / safety glasses clip-on | $5.00 |
| Head Camera | Mini OV5640 / IMX219 Wide-Angle Pinhole Camera Module | $9.00 |
| Audio Stack | Throat Microphone + Bone Conduction Transducer | $14.00 |
| Shoulder SBC | Radxa Zero 3W (Quad A55, 0.8 TOPS NPU, 1GB LPDDR4) | $22.00 |
| LoRa Master Radio | Semtech SX1280 2.4 GHz LoRa SPI Breakout | $6.50 |
| Shoulder Half-Plate | 3D Printed chest/shoulder plate + strap harness + cabling | $8.00 |
| Power Subsystem | 18650 3500mAh Li-Ion Cell + Battery Holder + 5V 2A Boost/Charging PMIC | $11.00 |
| **Total AR Glasses System** | | **~$160.50** |

---

## 5. Architectural Recommendations & Next Steps

1. **Adopt 2.4 GHz LoRa (SX1280) Across the Fleet:**
   - Eliminates EU duty cycle restrictions (which would strangle the swarm on 868 MHz).
   - High speed (~200 kbps) enables instant command reactions and sub-second visual snapshot transmissions to the AR HUD.
   - Tiny 3cm / ceramic antennas integrate neatly into both the robot chassis and glasses harness.
2. **Standardize on the Radxa Zero 3W for the AR Visor:**
   - Its onboard 0.8 TOPS NPU allows local computer vision (face recognition, object bounding boxes) without relying on an external server or phone.
3. **Firmware Phasing:**
   - **Phase 1 (Otto Swarm Link):** Flash TinyGo base with the verified servo timings from `../gotto`; implement the SX1280 driver on ESP32-S3 and NiceNano.
   - **Phase 2 (AR HUD Driver):** Set up Radxa Zero 3W Linux framebuffer output to the 0.39" Micro-OLED with a minimal green/amber tactical HUD overlay.
   - **Phase 3 (Vision & Voice Mesh):** Route detected targets from the glasses camera to the Otto swarm via binary LoRa commands; trigger physical expressions and interrogations.
