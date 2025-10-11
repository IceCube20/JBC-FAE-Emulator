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
  subgraph MCU["ESP32"]
    TX0["GPIO17 TX BUS0"]
    RX0["GPIO16 RX BUS0"]
    TX1["GPIO33 TX BUS1"]
    RX1["GPIO32 RX BUS1"]
    LEDPIN["GPIO21 SK6812"]
    RELPIN["GPIO26 Relay"]
    V3["3V3"]
    G["GND"]
  end

  %% ===== Modules =====
  subgraph MOD_A["MAX3232 DB9 Module BUS0"]
    A_RXD["TTL RXD"]
    A_TXD["TTL TXD"]
    A_V["VCC"]
    A_G["GND"]
    A_DB9["DB9 A 2=RXD 3=TXD 5=GND"]
  end

  subgraph MOD_B["MAX3232 DB9 Module BUS1"]
    B_RXD["TTL RXD"]
    B_TXD["TTL TXD"]
    B_V["VCC"]
    B_G["GND"]
    B_DB9["DB9 B 2=RXD 3=TXD 5=GND"]
  end

  %% ===== Peripherals =====
  LED["SK6812 strip or pixel"]
  REL["Relay module"]

  %% ===== Wiring BUS0 =====
  TX0 --> A_RXD
  A_TXD --> RX0
  V3 --> A_V
  G --> A_G

  %% ===== Wiring BUS1 =====
  TX1 --> B_RXD
  B_TXD --> RX1
  V3 --> B_V
  G --> B_G

  %% ===== LED & Relay =====
  LEDPIN -->|Data| LED
  RELPIN -->|Coil| REL

  %% ===== Ground to DB9 shells =====
  G -. GND .- A_DB9
  G -. GND .- B_DB9

  %% ===== Notes =====
  classDef note fill:#fff,stroke:#bbb,color:#333,stroke-dasharray:3 3;
  N1["If no data arrives, check RXD/TXD at the RJ12/adapter. Some cables are crossed."]:::note
  
  subgraph RJ12_A["RJ12 Bus0 (JBC jack)"]
    A_R1["1 - NC"]
    A_R2["2 - GND"]
    A_R3["3 - TX (JBC ->)"]
    A_R4["4 - RX (<- JBC)"]
    A_R5["5 - GND"]
    A_R6["6 - NC"]
  end

  subgraph DB9_A["DB9 Bus0 (RS-232)"]
    A_D2["Pin 2 - RXD"]
    A_D3["Pin 3 - TXD"]
    A_D5["Pin 5 - GND"]
  end

  A_R3 -->|TX -> RXD| A_D2
  A_D3 -->|TXD -> RX| A_R4
  A_R2 -. GND .- A_D5
  A_R5 -. GND .- A_D5
  
  subgraph RJ12_B["RJ12 Bus1 (JBC jack)"]
    B_R1["1 - NC"]
    B_R2["2 - GND"]
    B_R3["3 - TX (JBC ->)"]
    B_R4["4 - RX (<- JBC)"]
    B_R5["5 - GND"]
    B_R6["6 - NC"]
  end

  subgraph DB9_B["DB9 Bus1 (RS-232)"]
    B_D2["Pin 2 - RXD"]
    B_D3["Pin 3 - TXD"]
    B_D5["Pin 5 - GND"]
  end

  B_R3 -->|TX -> RXD| B_D2
  B_D3 -->|TXD -> RX| B_R4
  B_R2 -. GND .- B_D5
  B_R5 -. GND .- B_D5
