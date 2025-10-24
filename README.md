# JBC FAE Emulator – to control your Own Fume Extractor with your JBC Soldering Station without the Original JBC FAE 
This code runs on ESP32 and Arduino Mega.
Dual Bus (Max 2 JBC Soldering Stations).
With JBC DDE or DME you can set the relay afterrun in the Stations! The lower Models must the afterun configured in the Controler CLI (Arduino IDE Console).

**Emulates a JBC FAE base unit** with a robust P02 parser (DLE byte-stuffing, XOR BCC) on **two independent buses**.  
LED status (SK6812), relay after-run, persistence (NVS/EEPROM), and **RS-232 via MAX3232**.

> CLI over USB @ **115200**. Bus UARTs @ **250000 8E1**.  
> RS-232: **MAX3232** (3.0–5.5 V) converts between MCU-TTL (3V3/5V) and ±RS-232.

---

## Features

- **Dual bus** (BUS_COUNT=1|2), each bus with its own **DeviceID** & **address**
- **Relay** with **after-run** Configuring in the DDE and for lower Station Models over the CLI
- **Auto-save** for writes (debounced)
- **SK6812 GRBW** link/status per bus
- **Persistence**: ESP32 = **NVS** / Mega = **EEPROM**
- **RS-232** per bus via **DSub9 MAX3232 to ttl**

---

## CLI Example

- [NVS]
- bus0 DeviceID len=30 ascii="84495820251011193809F283" | addr=0x12
- bus1 DeviceID len=30 ascii="84495820251011194011F283" | addr=0x18
- [CLI] type 'help' for commands
- [CFG] buses=2 | tstop(w/s)=10/0 | selectflow=100 | suctionlevel=3 | filter(life/sat)=0/0 | stat_error=0 (0x0)
- [FW] 02:F2:EMU_01:8886612:0051112 | by IceCube20 | build Oct 11 2025 19:27:59
- Commands:
- help
- ? (show DEVID/Addr and other Data)
- show cfg
- show buses
- set buses <1|2>
- save / load
- erase [eeprom] – clear NVS/EEPROM and reload defaults (use 'save' to persist)
- set tstop_work <sec> afterrun Relay in seconds. After applying the time, save to EEPROM with <save>. With the DDE - you can configure it directly in the station.
- set tstop_stand <sec>
- set suctionlevel <0..3>
- set selectflow <0..1000>
- set stand_intakes <0|1>
- set filter_life <u16>
- set filter_sat <u16>
- STOPx/WARNx, +STOPx, -STOPx, NOERR, ?ERR

---

## Legal Notice

This project re-implements only functional protocol behavior (P02) for interoperability purposes.  
It contains no OEM firmware, no cryptographic keys, and does not circumvent any technical protection measures.  
Protocol constants, enums, and IDs are interface identifiers required for device communication.  
“JBC” is a trademark of its respective owners; this is an unofficial, independent project not affiliated with JBC S.A.

---

## Hardware & Pins

### ESP32 (defaults)

| Function              | Pin |
|----------------------:|:---:|
| BUS0 RX (Serial1)     | 16  |
| BUS0 TX (Serial1)     | 17  |
| BUS1 RX (Serial2)     | 32  |
| BUS1 TX (Serial2)     | 33  |
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
> **GND** must be **common** between MCU and MAX3232.

---



## Wiring Diagram ESP32

<p align="center">
  <img src="JBC FAE Emulator_Steckplatine.jpg" alt="JBC Link wiring diagram" width="900">
</p>

---

## Wiring Diagram Arduino Mega

<p align="center">
  <img src="JBC FAE Emulator Arduino Mega_Steckplatine.jpg" alt="JBC Link wiring diagram" width="900">
</p>

---
## Wiring Diagram JBC Station Cable

<p align="center">
  <img src="JBC Station Wiring.jpg" alt="JBC Link wiring diagram" width="900">
</p>

---
