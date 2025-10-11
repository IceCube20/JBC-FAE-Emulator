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
    M_TX0["GPIO17 - TX BUS0"]
    M_RX0["GPIO16 - RX BUS0"]
    M_TX1["GPIO33 - TX BUS1"]
    M_RX1["GPIO32 - RX BUS1"]
    M_LED["GPIO21 - SK6812"]
    M_REL["GPIO26 - Relay"]
    M_3V3["3V3"]
    M_GND["GND"]
  end

  %% ===== MAX3232 (dual-channel) =====
  subgraph MAX["MAX3232 (dual-channel)"]
    subgraph CHA["Channel A (Bus0)"]
      A_TIN["T1IN (TTL in)"]
      A_TOUT["T1OUT (RS-232 out)"]
      A_RIN["R1IN (RS-232 in)"]
      A_ROUT["R1OUT (TTL out)"]
    end
    subgraph CHB["Channel B (Bus1)"]
      B_TIN["T2IN (TTL in)"]
      B_TOUT["T2OUT (RS-232 out)"]
      B_RIN["R2IN (RS-232 in)"]
      B_ROUT["R2OUT (TTL out)"]
    end
    X_VCC["VCC 3.0-5.5 V"]
    X_GND["GND"]
  end

  %% ===== Connectors =====
  subgraph RS232A["Bus0 connector (DB9 / RJ12)"]
    A_TX["RS-232 TX (DB9 pin 3) to peer RX"]
    A_RX["RS-232 RX (DB9 pin 2) from peer TX"]
    A_GND["GND (DB9 pin 5)"]
    A_RJ["RJ12: 3=TX from JBC, 4=RX to JBC, 2/5=GND"]
  end

  subgraph RS232B["Bus1 connector (DB9 / RJ12)"]
    B_TX["RS-232 TX (DB9 pin 3) to peer RX"]
    B_RX["RS-232 RX (DB9 pin 2) from peer TX"]
    B_GND["GND (DB9 pin 5)"]
    B_RJ["RJ12: 3=TX from JBC, 4=RX to JBC, 2/5=GND"]
  end

  %% ===== TTL wiring (MCU <-> MAX3232) =====
  M_TX0 --> A_TIN
  A_ROUT --> M_RX0

  M_TX1 --> B_TIN
  B_ROUT --> M_RX1

  %% ===== Power & ground =====
  M_3V3 --> X_VCC
  M_GND --> X_GND

  %% ===== RS-232 side (MAX3232 <-> connectors) =====
  A_TOUT --> A_TX
  A_RX --> A_RIN
  X_GND --- A_GND

  B_TOUT --> B_TX
  B_RX --> B_RIN
  X_GND --- B_GND

  %% ===== Accessories =====
  M_LED -->|data| LED["SK6812 strip/pixel"]
  M_REL -->|coil| REL["Relay module"]
  M_GND --- LED
  M_GND --- REL
