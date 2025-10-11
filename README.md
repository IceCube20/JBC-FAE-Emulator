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
  %% ===== MCU =====
  subgraph MCU["ESP32 (3V3)"]
    TX0["GPIO17 TX BUS0"]
    RX0["GPIO16 RX BUS0"]
    TX1["GPIO33 TX BUS1"]
    RX1["GPIO32 RX BUS1"]
    V3["3V3"]
    GND["GND"]
  end

  %% ===== MAX3232 #1 for BUS0 =====
  subgraph MAX_A["MAX3232 A (single channel, BUS0)"]
    A_TIN["TIN"]
    A_TOUT["TOUT"]
    A_RIN["RIN"]
    A_ROUT["ROUT"]
    A_VCC["VCC"]
    A_GND["GND"]
  end

  %% ===== MAX3232 #2 for BUS1 =====
  subgraph MAX_B["MAX3232 B (single channel, BUS1)"]
    B_TIN["TIN"]
    B_TOUT["TOUT"]
    B_RIN["RIN"]
    B_ROUT["ROUT"]
    B_VCC["VCC"]
    B_GND["GND"]
  end

  %% ===== Connectors =====
  subgraph CONN_A["Bus0 connector (DB9 / RJ12)"]
    A_TX["RS232 TX -> peer RX (DB9 pin 3)"]
    A_RX["RS232 RX <- peer TX (DB9 pin 2)"]
    A_G["GND (DB9 pin 5)"]
  end

  subgraph CONN_B["Bus1 connector (DB9 / RJ12)"]
    B_TX["RS232 TX -> peer RX (DB9 pin 3)"]
    B_RX["RS232 RX <- peer TX (DB9 pin 2)"]
    B_G["GND (DB9 pin 5)"]
  end

  %% ----- TTL wiring -----
  TX0 --> A_TIN
  A_ROUT --> RX0
  TX1 --> B_TIN
  B_ROUT --> RX1

  %% ----- Power & ground -----
  V3 --> A_VCC
  V3 --> B_VCC
  GND --> A_GND
  GND --> B_GND

  %% ----- RS232 sides -----
  A_TOUT --> A_TX
  A_RX --> A_RIN
  A_GND --> A_G

  B_TOUT --> B_TX
  B_RX --> B_RIN
  B_GND --> B_G
