# JBC FAE Emulator – USB Base-Link + P02 (Dual-Bus, ESP32 / Arduino Mega)

**Emulates a JBC FAE base unit** with a robust P02 parser (DLE byte-stuffing, XOR BCC) on **two independent buses**.  
LED status (SK6812), relay after-run, persistence (NVS/EEPROM), and **RS-232 via MAX3232**.

> CLI over USB @ **115200**. Bus UARTs @ **250000 8E1**.  
> RS-232: **MAX3232** (3.0–5.5 V) converts between MCU-TTL (3V3/5V) and ±RS-232.

---

## Features

- **Dual bus** (BUS_COUNT=1|2), each bus with its own **DeviceID** & **address**
- **Base-Link** handshake (NAK/SYN/ACK/SOH) → **P02**
- **P02 parser**: length-based, DLE stuffing, **XOR BCC (seed 0x01)**
- **Relay** with **after-run owner** & “continuous suction”
- **Auto-save** for writes (debounced)
- **SK6812 GRBW** link/status per bus
- **Persistence**: ESP32 = **NVS** / Mega = **EEPROM**
- **RS-232** per bus via **MAX3232** (dual-channel possible)

---

## Hardware & Pins

### ESP32 (defaults)

| Function              | Pin |
|----------------------:|:---:|
| BUS0 RX (Serial2)     | 16  |
| BUS0 TX (Serial2)     | 17  |
| BUS1 RX (Serial1)     | 32  |
| BUS1 TX (Serial1)     | 33  |
| SK6812 **LED_PIN**    | 21  |
| Relay **RELAY_PIN**   | 26  |
| MAX3232 **VCC**       | 3V3 |

### Arduino Mega 2560 (defaults)

| Function              | Pin |
|----------------------:|:---:|
| BUS0 RX (Serial1)     | 19 (RX1) |
| BUS0 TX (Serial1)     | 18 (TX1) |
| BUS1 RX (Serial2)     | 17 (RX2) |
| BUS1 TX (Serial2)     | 16 (TX2) |
| SK6812 **LED_PIN**    | 6   |
| Relay **RELAY_PIN**   | 7   |
| MAX3232 **VCC**       | 5V  |

> **Note:** MAX3232 works with **3.3 V (ESP32)** and **5 V (Mega)**.  
> **GND** must be **common** between MCU, MAX3232, and the bus.

---

## RS-232 wiring with MAX3232 (dual-channel)

**Each bus** needs 1× TX and 1× RX → the MAX3232 chip has **2 drivers + 2 receivers** → **one chip can serve both buses** (Ch.A = Bus0, Ch.B = Bus1).  
On breakout boards the pins are often **T1IN/T1OUT, R1IN/R1OUT** (channel A) and **T2IN/T2OUT, R2IN/R2OUT** (channel B).

- **MCU-TX → TnIN**, **TnOUT → RS-232 TX (to peer RX)**
- **MCU-RX ← RnOUT**, **RnIN  ← RS-232 RX (from peer TX)**
- Tie **GND** together

### ESP32 + MAX3232 + 2× RS-232 (JBC Bus 0/1)

```mermaid
flowchart LR
  %% ===== ESP32 NodeMCU =====
  subgraph MCU["ESP32 NodeMCU (3V3 logic)"]
    TX0["GPIO17 → TX BUS0"]
    RX0["GPIO16 ← RX BUS0"]
    TX1["GPIO33 → TX BUS1"]
    RX1["GPIO32 ← RX BUS1"]
    LED_PIN["GPIO21 → SK6812 Data"]
    REL_PIN["GPIO26 → Relay IN"]
    V3["3V3"]
    V5["5V"]
    GND["GND"]
  end

  %% ===== MAX3232 Module BUS0 =====
  subgraph MAX0["MAX3232 Module – BUS0"]
    M0_TIN["TIN (from ESP32 TX17)"]
    M0_ROUT["ROUT (to ESP32 RX16)"]
    M0_TOUT["TOUT → RS232 TX (to JBC RX)"]
    M0_RIN["RIN ← RS232 RX (from JBC TX)"]
    M0_VCC["VCC 3V3"]
    M0_GND["GND"]
  end

  %% ===== MAX3232 Module BUS1 =====
  subgraph MAX1["MAX3232 Module – BUS1"]
    M1_TIN["TIN (from ESP32 TX33)"]
    M1_ROUT["ROUT (to ESP32 RX32)"]
    M1_TOUT["TOUT → RS232 TX (to JBC RX)"]
    M1_RIN["RIN ← RS232 RX (from JBC TX)"]
    M1_VCC["VCC 3V3"]
    M1_GND["GND"]
  end

  %% ===== Relay Module =====
  subgraph RELAY["Relay Module (JQC-3FF-S-Z)"]
    REL_VCC["VCC 3V3/5V"]
    REL_IN["IN (control from GPIO26)"]
    REL_GND["GND"]
    CONTACT["NO / COM contacts → fan or load"]
  end

  %% ===== LED Strip =====
  subgraph LEDS["SK6812 LED Strip (GRBW) 2 per Bus(4 pcs. 2 Buses"]
    LED_VCC["VCC 5V"]
    LED_DATA["DIN from GPIO21"]
    LED_GND["GND"]
  end

  %% ===== BUS0 RS232 Connector =====
  subgraph RS232_0["BUS0 RJ12/DB9 → JBC Station #1"]
    B0_TX["TX → JBC RX (RJ12 pin 4 / DB9 pin 3)"]
    B0_RX["RX ← JBC TX (RJ12 pin 3 / DB9 pin 2)"]
    B0_GND["GND (RJ12 pin 2+5 / DB9 pin 5)"]
  end

  %% ===== BUS1 RS232 Connector =====
  subgraph RS232_1["BUS1 RJ12/DB9 → JBC Station #2"]
    B1_TX["TX → JBC RX (RJ12 pin 4 / DB9 pin 3)"]
    B1_RX["RX ← JBC TX (RJ12 pin 3 / DB9 pin 2)"]
    B1_GND["GND (RJ12 pin 2+5 / DB9 pin 5)"]
  end

  %% ===== Wiring BUS0 =====
  TX0 --> M0_TIN
  M0_ROUT --> RX0
  V3 --> M0_VCC
  GND --> M0_GND
  M0_TOUT --> B0_TX
  M0_RIN --> B0_RX
  GND --> B0_GND

  %% ===== Wiring BUS1 =====
  TX1 --> M1_TIN
  M1_ROUT --> RX1
  V3 --> M1_VCC
  GND --> M1_GND
  M1_TOUT --> B1_TX
  M1_RIN --> B1_RX
  GND --> B1_GND

  %% ===== Relay Wiring =====
  REL_PIN --> REL_IN
  V5 --> REL_VCC
  GND --> REL_GND

  %% ===== LED Wiring =====
  LED_PIN --> LED_DATA
  V5 --> LED_VCC
  GND --> LED_GND
