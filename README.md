# JBC FAE Emulator – USB Base-Link + P02 (Dual-Bus, ESP32 / Arduino Mega)

**Emuliert eine JBC FAE Base-Unit** mit robustem P02-Parser (DLE-Byte-Stuffing, XOR-BCC), **zwei unabhängigen Bussen**, LED-Status (SK6812) und Relais-Nachlauf. Läuft auf **ESP32** und **Arduino Mega 2560**.

> Firmware-Banner zeigt Modell/Cap/IDs und Build-Zeit. CLI über USB-Serial (115200).

---

## Features

- **Dual-Bus** (BUS_COUNT=1|2), je Bus eigene **DeviceID** & **Adresse**  
- **Base-Link** Handshake: NAK → SYN/ACK/SOH → P02  
- **P02 Parser**: LEN-basiert, DLE-Stuffing, **XOR-BCC (Seed 0x01)**  
- **Relais-Logik**: WORK/Stand, **After-Run** (Owner), **continuous suction**  
- **Auto-Save** bei Writes (**Debounce** `CFG_SAVE_DEBOUNCE_MS`)  
- **SK6812 GRBW** Status-LEDs, Link/Status pro Bus  
- **Persistenz**: ESP32 = **NVS** (Preferences) / Mega = **EEPROM**  
- **CLI**: Hotkeys **`?` / `ESC` / `Ctrl+B`** und Befehle (help, set, save/load, erase, …)

---

## Hardware & Pins

### ESP32 (Defaults)

| Funktion              | Pin |
|----------------------|----:|
| BUS0 RX (Serial2)    | 16  |
| BUS0 TX (Serial2)    | 17  |
| BUS1 RX (Serial1)    | 32  |
| BUS1 TX (Serial1)    | 33  |
| SK6812 **LED_PIN**   | 21  |
| Relais **RELAY_PIN** | 26  |

> UART: **250000 Baud, 8E1** (beide Busse)

### Arduino Mega 2560 (Defaults)

| Funktion              | Pin |
|----------------------|----:|
| BUS0 RX (Serial1)    | **19 (RX1)** |
| BUS0 TX (Serial1)    | **18 (TX1)** |
| BUS1 RX (Serial2)    | **17 (RX2)** |
| BUS1 TX (Serial2)    | **16 (TX2)** |
| SK6812 **LED_PIN**   | 6   |
| Relais **RELAY_PIN** | 7   |

---

## Wiring (ESP32)

```mermaid
flowchart LR
  subgraph MCU[ESP32]
    RX0["RX0 (USB) — CLI @115200"]
    RX16["RX2 = GPIO16"]
    TX17["TX2 = GPIO17"]
    RX32["RX1 = GPIO32"]
    TX33["TX1 = GPIO33"]
    LED21["GPIO21 → SK6812 DIN"]
    REL26["GPIO26 → Relay IN"]
    GNDm["GND"]
    VIN["5V/VIN"]
  end

  subgraph BUS0[JBC Bus 0]
    B0RX["RX (to ESP32 TX17)"]
    B0TX["TX (to ESP32 RX16)"]
    B0G["GND"]
  end

  subgraph BUS1[JBC Bus 1]
    B1RX["RX (to ESP32 TX33)"]
    B1TX["TX (to ESP32 RX32)"]
    B1G["GND"]
  end

  subgraph LED[SK6812 GRBW]
    DIN["DIN"]
    L5["5V"]
    LG["GND"]
  end

  subgraph REL[Relay Module]
    RIN["IN"]
    R5["VCC 5V"]
    RG["GND"]
  end

  subgraph PSU[5V Power]
    P5["5V"]
    PG["GND"]
  end

  %% UART crossover
  TX17 --> B0RX
  B0TX --> RX16
  TX33 --> B1RX
  B1TX --> RX32

  %% LEDs
  LED21 --> DIN

  %% Relay
  REL26 --> RIN

  %% Power/GND
  P5 --> L5
  P5 --> R5
  P5 --> VIN
  PG --> LG
  PG --> RG
  PG --> GNDm
  PG --> B0G
  PG --> B1G
