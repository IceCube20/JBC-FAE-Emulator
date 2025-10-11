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

  %% ===== Module =====
  subgraph MOD_A["MAX3232 DB9 Modul BUS0"]
    A_RXD["TTL RXD"]
    A_TXD["TTL TXD"]
    A_V["VCC"]
    A_G["GND"]
    A_DB9["DB9 A 2=RXD 3=TXD 5=GND"]
  end

  subgraph MOD_B["MAX3232 DB9 Modul BUS1"]
    B_RXD["TTL RXD"]
    B_TXD["TTL TXD"]
    B_V["VCC"]
    B_G["GND"]
    B_DB9["DB9 B 2=RXD 3=TXD 5=GND"]
  end

  %% ===== Peripherie =====
  LED["SK6812 Strip oder Pixel"]
  REL["Relais Modul"]

  %% ===== Verdrahtung BUS0 =====
  TX0 --> A_RXD
  A_TXD --> RX0
  V3 --> A_V
  G --> A_G

  %% ===== Verdrahtung BUS1 =====
  TX1 --> B_RXD
  B_TXD --> RX1
  V3 --> B_V
  G --> B_G

  %% ===== LED & Relais =====
  LEDPIN -->|Data| LED
  RELPIN -->|Coil| REL

  %% ===== Masse zu DB9 (Gehäuse GND) =====
  G -. GND .- A_DB9
  G -. GND .- B_DB9

  %% ===== Hinweise =====
  classDef note fill:#fff,stroke:#bbb,color:#333,stroke-dasharray:3 3;
  N1["Wenn keine Daten ankommen, RXD und TXD am RJ12/Adapter pruefen. Manche Kabel sind gekreuzt."]:::note
