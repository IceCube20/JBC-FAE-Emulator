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
  subgraph MCU[ESP32]
    TX0a["TX2 = GPIO17 (Bus0 →)"]
    RX0a["RX2 = GPIO16 (← Bus0)"]
    TX1a["TX1 = GPIO33 (Bus1 →)"]
    RX1a["RX1 = GPIO32 (← Bus1)"]
    LED21["GPIO21 → SK6812 DIN"]
    REL26["GPIO26 → Relay IN"]
    V33["3V3"]
    Gm["GND"]
  end

  subgraph MAX[Dual-Channel MAX3232]
    Vcc["VCC (3V3)"]
    Gx["GND"]
    %% Channel A (Bus0)
    T1IN["T1IN  (from ESP32 TX17)"]
    T1OUT["T1OUT (to RS-232 Bus0 TX→peer RX)"]
    R1IN["R1IN  (from RS-232 Bus0 RX←peer TX)"]
    R1OUT["R1OUT (to ESP32 RX16)"]
    %% Channel B (Bus1)
    T2IN["T2IN  (from ESP32 TX33)"]
    T2OUT["T2OUT (to RS-232 Bus1 TX→peer RX)"]
    R2IN["R2IN  (from RS-232 Bus1 RX←peer TX)"]
    R2OUT["R2OUT (to ESP32 RX32)"]
  end

  subgraph B0[RS-232 Bus 0]
    B0TX["TXD → Gegenstelle RXD"]
    B0RX["RXD ← Gegenstelle TXD"]
    B0G["GND"]
  end

  subgraph B1[RS-232 Bus 1]
    B1TX["TXD → Gegenstelle RXD"]
    B1RX["RXD ← Gegenstelle TXD"]
    B1G["GND"]
  end

  %% Power
  V33 --> Vcc
  Gm  --> Gx

  %% Channel A
  TX0a --> T1IN
  R1OUT --> RX0a
  T1OUT --> B0TX
  B0RX  --> R1IN

  %% Channel B
  TX1a --> T2IN
  R2OUT --> RX1a
  T2OUT --> B1TX
  B1RX  --> R2IN

  %% Grounds
  Gm --> B0G
  Gm --> B1G

  %% Extras
  LED21 -.->|"DIN"| SK6812((SK6812 GRBW))
  REL26 -.->|"IN"| REL((Relay Module))
