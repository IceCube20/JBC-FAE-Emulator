# JBC FAE Emulator – USB Base-Link + P02 (Dual-Bus, ESP32 / Arduino Mega)

**Emuliert eine JBC FAE Base-Unit** mit robustem P02-Parser (DLE-Byte-Stuffing, XOR-BCC) auf **zwei unabhängigen Bussen**.  
LED-Status (SK6812), Relais-Nachlauf, Persistenz (NVS/EEPROM) und **RS-232 über MAX3232**.

> CLI über USB @ **115200**. Bus-UARTs @ **250000 8E1**.  
> RS-232: **MAX3232** (3.0–5.5 V) wandelt zwischen MCU-TTL (3V3/5V) und ±RS-232.

---

## Features

- **Dual-Bus** (BUS_COUNT=1|2), je Bus eigene **DeviceID** & **Adresse**
- **Base-Link** Handshake (NAK/SYN/ACK/SOH) → **P02**
- **P02 Parser**: LEN-basiert, DLE-Stuffing, **XOR-BCC (Seed 0x01)**
- **Relais** mit **After-Run Owner** & „continuous suction“
- **Auto-Save** von Writes (Debounce)
- **SK6812 GRBW** Link/Status je Bus
- **Persistenz**: ESP32 = **NVS** / Mega = **EEPROM**
- **RS-232** pro Bus via **MAX3232** (Dual-Channel möglich)

---

## Hardware & Pins

### ESP32 (Defaults)

| Funktion              | Pin |
|----------------------:|:---:|
| BUS0 RX (Serial2)     | 16  |
| BUS0 TX (Serial2)     | 17  |
| BUS1 RX (Serial1)     | 32  |
| BUS1 TX (Serial1)     | 33  |
| SK6812 **LED_PIN**    | 21  |
| Relais **RELAY_PIN**  | 26  |
| MAX3232 **VCC**       | 3V3 |

### Arduino Mega 2560 (Defaults)

| Funktion              | Pin |
|----------------------:|:---:|
| BUS0 RX (Serial1)     | 19 (RX1) |
| BUS0 TX (Serial1)     | 18 (TX1) |
| BUS1 RX (Serial2)     | 17 (RX2) |
| BUS1 TX (Serial2)     | 16 (TX2) |
| SK6812 **LED_PIN**    | 6   |
| Relais **RELAY_PIN**  | 7   |
| MAX3232 **VCC**       | 5V  |

> **Hinweis:** MAX3232 funktioniert mit **3.3 V (ESP32)** und **5 V (Mega)**.  
> **GND** muss **gemeinsam** zwischen MCU, MAX3232 und Bus liegen.

---

## RS-232 Verdrahtung mit MAX3232 (Dual-Channel)

**Jeder Bus** braucht 1× TX und 1× RX → der MAX3232 (Chip) hat **2 Treiber + 2 Empfänger** → **ein Baustein kann beide Busse** bedienen (Ch.A = Bus0, Ch.B = Bus1).  
Bei Breakout-Boards heißen die Pins oft **T1IN/T1OUT, R1IN/R1OUT** (Kanal A) und **T2IN/T2OUT, R2IN/R2OUT** (Kanal B).

- **MCU-TX → TnIN**, **TnOUT → RS-232-TX (zum Gegenüber RX)**
- **MCU-RX ← RnOUT**, **RnIN  ← RS-232-RX (vom Gegenüber TX)**
- **GND** durchverbinden

### ESP32 + MAX3232 + 2× RS-232 (JBC Bus 0/1)

```mermaid
flowchart LR
  %% ============ JBC RJ12 ============
  subgraph RJ[JBC Station – RJ12]
    RJ2["Pin 2 — GND"]
    RJ3["Pin 3 — TXD (JBC →)"]
    RJ4["Pin 4 — RXD (← JBC)"]
    RJ5["Pin 5 — GND"]
  end

  %% ============ DB9 ============
  subgraph DB9[DB9 (Female an Station)]
    D2["Pin 2 — RXD (empfängt von JBC TX)"]
    D3["Pin 3 — TXD (sendet zur JBC RX)"]
    D5["Pin 5 — GND"]
  end

  %% ============ MAX3232 (ein Kanal) ============
  subgraph MAX[MAX3232 (ein Kanal)]
    Vcc["VCC (3V3/5V)"]
    Gx["GND"]
    T1IN["T1IN  (from MCU TX)"]
    T1OUT["T1OUT (to DB9 Pin 3 TXD)"]
    R1IN["R1IN  (from DB9 Pin 2 RXD)"]
    R1OUT["R1OUT (to MCU RX)"]
  end

  %% ============ MCU ============
  subgraph MCU[MCU]
    V["3V3 (ESP32) / 5V (Mega)"]
    Gm["GND"]
    TX["TX (→)"]
    RX["RX (←)"]
  end

  %% RJ12 ↔ DB9 (Kabel/Adapter)
  RJ3 -->|JBC TXD| D2
  D3 -->|DB9 TXD| RJ4
  RJ2 -. GND .- D5
  RJ5 -. GND .- D5

  %% DB9 ↔ MAX3232 (RS-232 Seite)
  D2 --> R1IN
  T1OUT --> D3
  D5 --> Gx

  %% MAX3232 ↔ MCU (TTL Seite)
  TX --> T1IN
  R1OUT --> RX
  V --> Vcc
  Gm --> Gx

  %% Hinweis
  classDef note fill:#fff,stroke:#bbb,color:#333,stroke-dasharray: 3 3;
  N1(["Hinweis: Falls nichts ankommt, RJ12 Pins 3/4 tauschen (manche Kabel sind gedreht)."]):::note
