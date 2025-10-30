// SPDX-License-Identifier: MIT OR GPL-2.0-only
/*
  JBC FAE Emulator – USB Base-Link (NAK/SYN/ACK/SOH) + P02 Frames (Dual-Bus)
  ===========================================================================
  DE / Deutsch
  ------------

  Überblick
  • Emuliert eine JBC FAE Base-Unit auf bis zu zwei unabhängigen Bussen (BUS_COUNT=1|2).
  • Pro Bus eigene DeviceID und Adresse (über HS automatisch übernommen & persistiert).
  • Busanzahl zur Laufzeit per CLI umstellbar; Persistenz via „save“.
  • Robuster P02-Parser (LEN-basiert) mit DLE-Byte-Stuffing und XOR-BCC (Seed 0x01).
  • Relais-/Nachlauf-Logik über beide Busse (WORK/Stand, „continuous suction“, Afterrun-Owner).
  • Auto-Save der Konfig bei Writes (Debounce CFG_SAVE_DEBOUNCE_MS; schont NVS/EEPROM).
  • Läuft auf ESP32 und Arduino Mega 2560.

  Persistenz
  • ESP32: NVS (Preferences) – DeviceID/Addr je Bus, Config-Blob, Busanzahl.
  • MEGA (AVR): EEPROM – je Bus ein Block (MAGIC, LEN, ADDR, CHKSUM, DEV_ID), + CFG-Block + Buszahl.
    Hinweis: Keine PSTR()/F() in globalen Initialisierern auf AVR – Strings liegen als PROGMEM-Arrays vor.

  Pins/Ports & UART
  • 250000 Baud, 8E1.
  • ESP32: BUS0=Serial2 (RX=16, TX=17), BUS1=Serial1 (RX=32, TX=33).
  • MEGA : BUS0=Serial1, BUS1=Serial2.

  Relais (Ausgang)
  • Standard: RELAY_ENABLE=1, RELAY_PIN=26 (ESP32) / 7 (MEGA), RELAY_ACTIVE_LOW=0 (HIGH = an).
  • Schaltet mit WORK/Nachlauf/„continuous“. Log: [RELAY] 🟢ON / ⚪OFF, inkl. geplanter Nachlaufzeit.

  LEDs (SK6812 GRBW, Adafruit_NeoPixel)
  • Pro Bus 2 LEDs: 0=Link, 1=Status.
    Link: DOWN=rot „breathing“, CONNECTING=gelb blinkend, UP=grün; FW-Idle (keine IntakeActivation seit LED_FW_IDLE_MS): gelb blinkend.
    Status: Blau=WORK aktiv (dieser Bus), Lila=Nachlauf (Owner), Rot blinkend=STOP, Gelb blinkend=WARN, Weiß „breathing“=Standby.
  • Defaults: LED_ENABLE=1, LED_PIN=21 (ESP32) / 6 (MEGA), LED_BRIGHTNESS_MAX=40.
  • Timing-Schutz: LED_SHOW_MIN_GAP_US = 10000UL (AVR) / 1500UL (andere).
    Tipp: Auf MEGA ggf. LED_ENABLE=0 oder GAP erhöhen (sonst BCC-Fehler möglich).

  CLI (Seriell @115200)
  • Hotkeys (ohne Enter, wenn Puffer leer): '?' | ESC | Ctrl+B → Banner (Persistenz, CFG, FW).
  • Befehle:
      help
      show cfg            – aktuelle Konfiguration
      show buses          – aktive Busse
      set buses <1|2>     – Anzahl Busse (mit „save“ sichern)
      set tstop_work <s>  | set tstop_stand <s>
      set suctionlevel <0..3>
      set selectflow <0..1000>
      set stand_intakes <0|1>
      set filter_life <0..1000> | set filter_sat <0..1000>
      save | load
      erase [eeprom]      – NVS/EEPROM leeren & Defaults laden (mit „save“ sichern)
      NOERR               – Fehler löschen
      ERR                 – Fehlerstatus + Tabelle anzeigen
      STOPx/WARNx         – Fehler direkt setzen (z.B. STOP1)
      +STOPx / -STOPx     – Bits addieren/entfernen

  Fehlerkürzel → Bitmaske
  • STOP1=1, WARN1=2, STOP2=4, WARN2=8, STOP3=16, STOP4=32, STOP5=64,
    STOP7=256, STOP8=512, STOP9=1024, STOP10=2048, STOP11=4096.

  Firmware-/IDs
  • FE_MODEL="F2", FE_CAP="EMU_01", FE_SW7="8886612", FE_HW7="0051112".
  • Default-DeviceIDs (32 ASCII) pro Bus; werden bei Writes überschrieben und persistent gesichert.
  • Firmware-Banner zeigt Kennung + Build-Zeit; Startup-Banner zeigt Persistenzstatus [NVS]/[EEP].

  Build-Schalter (Beispiele)
  • #define BUS_COUNT 2
  • #define LED_ENABLE 0                 // falls MEGA zickt
  • #define RELAY_PIN 7                  // anderer MEGA-Pin
  • #define RELAY_ACTIVE_LOW 1           // invertierte Logik
  • #define DBG_SHOW_TX 1 / DBG_SHOW_RX 1 / DBG_SHOW_PROTO 1
  • #define DBG_TS_MODE 1
  • #define LED_SHOW_MIN_GAP_US 15000UL  // noch konservativer auf AVR
  • #define DBG_LOG_DUP_WRITES 0         // Duplicate-Write-Logs unterdrücken

  Bekannte Hinweise
  • MEGA + SK6812: Zu kurze Render-Abstände können UART stören → BCC-Mismatch.
    Abhilfe: LED_ENABLE=0 oder LED_SHOW_MIN_GAP_US erhöhen.

  Autor / Build
  • by IceCube20 – FW-Banner zeigt Kennung und Build-Timestamp.

  ---------------------------------------------------------------------------
  EN / English
  ------------

  Overview
  • Emulates a JBC FAE base unit on up to two independent buses (BUS_COUNT=1|2).
  • Per-bus DeviceID and address (auto-adopted via HS and persisted).
  • Bus count can be changed at runtime via CLI; persist with “save”.
  • Robust P02 parser (length-based) with DLE byte-stuffing and XOR BCC (seed 0x01).
  • Cross-bus relay/after-run logic (WORK/Stand, “continuous suction”, after-run owner).
  • Auto-save of config on writes (debounced via CFG_SAVE_DEBOUNCE_MS to reduce wear).
  • Runs on ESP32 and Arduino Mega 2560.

  Persistence
  • ESP32: NVS (Preferences) – stores per-bus DeviceID/Addr, config blob, bus count.
  • MEGA (AVR): EEPROM – one block per bus (MAGIC, LEN, ADDR, CHKSUM, DEV_ID), plus CFG block and bus count.
    Note: Avoid PSTR()/F() in global initializers on AVR – strings are stored as PROGMEM arrays.

  Pins/Ports & UART
  • 250000 baud, 8E1.
  • ESP32: BUS0=Serial2 (RX=16, TX=17), BUS1=Serial1 (RX=32, TX=33).
  • MEGA : BUS0=Serial1, BUS1=Serial2.

  Relay (output)
  • Defaults: RELAY_ENABLE=1, RELAY_PIN=26 (ESP32) / 7 (MEGA), RELAY_ACTIVE_LOW=0 (HIGH = on).
  • Tracks WORK/after-run/“continuous”. Log shows [RELAY] 🟢ON / ⚪OFF and scheduled after-run seconds.

  LEDs (SK6812 GRBW, Adafruit_NeoPixel)
  • Two LEDs per bus: 0=link, 1=status.
    Link: DOWN=red “breathing”, CONNECTING=yellow blink, UP=green; FW idle loop (no IntakeActivation since LED_FW_IDLE_MS): yellow blink.
    Status: Blue=WORK active (this bus), Magenta=after-run (owner), Red blink=STOP, Yellow blink=WARN, White “breathing”=standby.
  • Defaults: LED_ENABLE=1, LED_PIN=21 (ESP32) / 6 (MEGA), LED_BRIGHTNESS_MAX=40.
  • Timing guard: LED_SHOW_MIN_GAP_US = 10000UL (AVR) / 1500UL (others).
    Tip: On MEGA consider LED_ENABLE=0 or increase GAP to avoid BCC issues.

  CLI (serial @115200)
  • Hotkeys (no Enter, when buffer empty): '?' | ESC | Ctrl+B → banner (persistence, CFG, FW).
  • Commands:
      help
      show cfg
      show buses
      set buses <1|2>
      set tstop_work <s>  | set tstop_stand <s>
      set suctionlevel <0..3>
      set selectflow <0..1000>
      set stand_intakes <0|1>
      set filter_life <0..1000> | set filter_sat <0..1000>
      save | load
      erase [eeprom]      – clear NVS/EEPROM & reload defaults (persist with “save”)
      NOERR
      ERR
      STOPx/WARNx          (e.g. STOP1)
      +STOPx / -STOPx      (set/clear bits)

  Error mnemonics → bit mask
  • STOP1=1, WARN1=2, STOP2=4, WARN2=8, STOP3=16, STOP4=32, STOP5=64,
    STOP7=256, STOP8=512, STOP9=1024, STOP10=2048, STOP11=4096.

  Firmware & IDs
  • FE_MODEL="F2", FE_CAP="EMU_01", FE_SW7="8886612", FE_HW7="0051112".
  • Default 32-ASCII DeviceIDs per bus; overwritten on writes and persisted.
  • Firmware banner prints ID and build timestamp; startup banner shows persistence backend [NVS]/[EEP].

  Build switches (examples)
  • #define BUS_COUNT 2
  • #define LED_ENABLE 0                 // if MEGA is tight
  • #define RELAY_PIN 7                  // custom MEGA pin
  • #define RELAY_ACTIVE_LOW 1           // inverted logic
  • #define DBG_SHOW_TX 1 / DBG_SHOW_RX 1 / DBG_SHOW_PROTO 1
  • #define DBG_TS_MODE 1
  • #define LED_SHOW_MIN_GAP_US 15000UL  // even more conservative on AVR
  • #define DBG_LOG_DUP_WRITES 0         // silence duplicate write logs

  Notes
  • MEGA + SK6812: short render gaps can disturb UART → BCC mismatch.
    Mitigation: disable LEDs or increase LED_SHOW_MIN_GAP_US.

  Author / Build
  • by IceCube20 – firmware banner shows ID and build timestamp.
*/



#include <Arduino.h>
#include <string.h>
#include "jbc_FE_commands_full.h"

#if defined(ARDUINO_ARCH_AVR)
  #include <avr/pgmspace.h>
  #define PROGMEM_STR const char PROGMEM
#else
  #define PROGMEM_STR const char
#endif

// ==== FlashString-Helpers (spart SRAM auf AVR) ==============================
#if defined(ARDUINO_ARCH_AVR)
  typedef const __FlashStringHelper* flashstr_t;
  #define FS_LIT(lit) (reinterpret_cast<flashstr_t>(PSTR(lit)))
#else
  typedef const char* flashstr_t;
  #define FS_LIT(lit) (lit)
#endif

// ===== Debug/Output-Schalter ================================================
#ifndef DBG_SHOW_RX
#define DBG_SHOW_RX 0
#endif
#ifndef DBG_SHOW_TX
#define DBG_SHOW_TX 0
#endif
#ifndef DBG_SHOW_PROTO
#define DBG_SHOW_PROTO 0
#endif
#ifndef DBG_SHOW_ERR
#define DBG_SHOW_ERR 1
#endif
#ifndef DBG_SHOW_STATE
#define DBG_SHOW_STATE 1
#endif
#ifndef DBG_STATE_ON_WRITE_ONLY
#define DBG_STATE_ON_WRITE_ONLY 0
#endif
#ifndef DBG_STATE_PERIOD_MS
#define DBG_STATE_PERIOD_MS 0
#endif
#ifndef DBG_LOG_WRITES
#define DBG_LOG_WRITES 0
#endif
#ifndef DBG_LOG_WRITES_HEX
#define DBG_LOG_WRITES_HEX 0
#endif
#ifndef DBG_LOG_DUP_WRITES
#define DBG_LOG_DUP_WRITES 0   // 0 = keine "duplicate ... ignored" Meldungen
#endif
#ifndef LOG_ICONS
#define LOG_ICONS 1
#endif
// Debug Timestemp
#ifndef DBG_TS_MODE
// 0=aus, 1=ms seit Boot, 2=Sekunden mit 1 Nachkommastelle
#define DBG_TS_MODE 0
#endif
#ifndef BANNER_HOTKEY1
#define BANNER_HOTKEY1 '?'     // Fragezeichen
#endif
#ifndef BANNER_HOTKEY2
#define BANNER_HOTKEY2 0x1B    // ESC
#endif
#ifndef BANNER_HOTKEY3
#define BANNER_HOTKEY3 0x02    // Ctrl+B
#endif

// ===== Dual-Bus Auswahl =====================================================
#ifndef BUS_COUNT
#define BUS_COUNT 2
#endif
#if BUS_COUNT < 1 || BUS_COUNT > 2
#error "Es wird nur BUS_COUNT nur 1 oder 2 unterstützt"
#endif

#define MAX_BUSES 2

static uint8_t g_bus_count = BUS_COUNT;   // 1..2 zur Laufzeit


// ===== Relais (Ausgang) =====================================================
#ifndef RELAY_ENABLE
  #define RELAY_ENABLE 1
#endif

#ifndef RELAY_PIN
  #if defined(ARDUINO_ARCH_AVR)
    #define RELAY_PIN 7        // Mega-Default
  #else
    #define RELAY_PIN 26       // ESP32-Default
  #endif
#endif

#ifndef RELAY_ACTIVE_LOW
  #define RELAY_ACTIVE_LOW 0   // 0: HIGH=an, 1: LOW=an
#endif


static inline void relay_init(){
#if RELAY_ENABLE
  pinMode(RELAY_PIN, OUTPUT);
#endif
  // Relais sicher aus, Logik-Status synchronisieren
  relay_set(false);
}


// ===== SK6812 Status-LEDs (GRBW) ============================================
#ifndef LED_ENABLE
#define LED_ENABLE 1
#endif
#if LED_ENABLE
  #if defined(ARDUINO_ARCH_AVR)
    #ifndef LED_PIN
    #define LED_PIN 6               // Mega-Default
    #endif
  #else
    #ifndef LED_PIN
    #define LED_PIN 21              // ESP32-Default
    #endif
  #endif
  // 2 LEDs pro Bus (Link, Status)
  #ifndef LED_COUNT_MAX
  #define LED_COUNT_MAX (MAX_BUSES*2)     // Puffer für max. 2 Busse
  #endif
  #ifndef LED_BRIGHTNESS_MAX
  #define LED_BRIGHTNESS_MAX 200
  #endif
  #ifndef LED_BREATH_PERIOD_MS
  #define LED_BREATH_PERIOD_MS 5000
  #endif
  #ifndef LED_BLINK_PERIOD_MS
  #define LED_BLINK_PERIOD_MS 1000
  #endif
  #ifndef LED_SHOW_MIN_GAP_US
    #if defined(ARDUINO_ARCH_AVR)
      #define LED_SHOW_MIN_GAP_US 10000UL  // AVR: 1 s
    #else
      #define LED_SHOW_MIN_GAP_US 1500UL     // andere: 1.5 ms
    #endif
  #endif

  #include <Adafruit_NeoPixel.h>
  static Adafruit_NeoPixel pixels(LED_COUNT_MAX, LED_PIN, NEO_GRBW + NEO_KHZ800);
  static uint32_t t_led_last_render = 0;
  static inline uint8_t breath_level(uint32_t period_ms, uint8_t lo, uint8_t hi){
    uint32_t t = millis() % period_ms;
    uint32_t half = period_ms/2;
    if (t <= half){
      return (uint8_t)(lo + (uint32_t)(hi - lo) * t / half);
    }
    uint32_t tt = t - half;
    return (uint8_t)(hi - (uint32_t)(hi - lo) * tt / half);
  }
#endif

// ===== FAE-Konfiguration =====================================================
#define FE_MODEL  "F2" // Wichtig! nur F2 wird von DDE und JTSE angenommen.
#define FE_CAP    "EMU_02" // oder CAP_02
#define FE_SW7    "V1.0.10" // 8886612
#define FE_HW7    "ESP32Devkit1" // 0051112

static PROGMEM_STR FE_FW_STR[] = "02:" FE_MODEL ":" FE_CAP ":" FE_SW7 ":" FE_HW7;
static_assert(sizeof(FE_FW_STR) - 1 <= 64, "FW string too long");

// Default-IDs pro Bus (32 ASCII)
static PROGMEM_STR FE_DEVICE_ID_STR0[] = "849158E467A340159646170D6B1595EF";
static PROGMEM_STR FE_DEVICE_ID_STR1[] = "849158E467A340159646170D6B1596EF"; // zweite DDE Default
static_assert(sizeof(FE_DEVICE_ID_STR0) - 1 == 32, "DeviceID0 must be 32 ASCII");
static_assert(sizeof(FE_DEVICE_ID_STR1) - 1 == 32, "DeviceID1 must be 32 ASCII");

#ifndef FE_SEND_EMPTY_DEVICEID
  #define FE_SEND_EMPTY_DEVICEID 0
#endif

static void log_fw_banner();  // forward declaration
static void print_persist_diag();
static void print_startup_banner();
// Autosave
#ifndef LED_FW_IDLE_MS
#define LED_FW_IDLE_MS 2000  // 2 s ohne IntakeActivation => Gelb-Blinken der Link-LED
#endif
#ifndef MIRROR_WRITES_TO_CFG
#define MIRROR_WRITES_TO_CFG 1      // 1 = Writes in cfg spiegeln
#endif
#ifndef AUTO_SAVE_CFG_ON_JBC_WRITES
#define AUTO_SAVE_CFG_ON_JBC_WRITES 1
#endif
#ifndef CFG_SAVE_DEBOUNCE_MS
#define CFG_SAVE_DEBOUNCE_MS 1500   // NVS/EEPROM-Schonung
#endif

static bool     cfg_dirty = false;
static uint32_t cfg_save_deadline = 0;

static inline void schedule_cfg_save(){
#if AUTO_SAVE_CFG_ON_JBC_WRITES
  cfg_dirty = true;
  cfg_save_deadline = millis() + CFG_SAVE_DEBOUNCE_MS;
#endif
}

// ===== FE-State (gemeinsam für beide Busse) ==================================
#define MAX_PORTS 1
#define NUM_INTAKES 2  // 0=Work, 1=Stand
#define WORK_IDX  0
#define STAND_IDX 1

#define DEF_SUCTIONLEVEL    3
#define DEF_SELECTFLOW_X_M  100
#define DEF_ACTIVATION      0
#ifndef DEF_TSTOP_WORK
#define DEF_TSTOP_WORK 10
#endif
#ifndef DEF_TSTOP_STAND
#define DEF_TSTOP_STAND 0
#endif
#ifndef DEF_FLOW_X_MIL
#define DEF_FLOW_X_MIL 0
#endif
#ifndef DEF_SPEED_RPM
#define DEF_SPEED_RPM 0
#endif
#ifndef DEF_PEDAL_ACT
#define DEF_PEDAL_ACT 0
#endif
#ifndef DEF_PEDAL_MODE
#define DEF_PEDAL_MODE 1
#endif
#ifndef DEF_STAND_INTAKES
#define DEF_STAND_INTAKES 1
#endif
#ifndef DEF_FILTER_LIFE
#define DEF_FILTER_LIFE 0
#endif
#ifndef DEF_FILTER_SAT
#define DEF_FILTER_SAT 0
#endif
#ifndef DEF_STAT_ERROR
#define DEF_STAT_ERROR 0
#endif
// ---- CLI Clamping -----------------------------------------------------------
#define CLAMP(v, lo, hi) (( (v) < (lo) ) ? (lo) : ( ((v) > (hi)) ? (hi) : (v) ))

#define CL_TSTOP_WORK_MIN      0
#define CL_TSTOP_WORK_MAX      300

#define CL_TSTOP_STAND_MIN     0
#define CL_TSTOP_STAND_MAX     999

#define CL_SUCTIONLEVEL_MIN    0
#define CL_SUCTIONLEVEL_MAX    3

// selectFlow/actual_flow sind 0..1000 (x10%-Skala bzw. x1000)
#define CL_SELECTFLOW_MIN      0
#define CL_SELECTFLOW_MAX      1000

#define CL_ACTUAL_FLOW_MIN     0
#define CL_ACTUAL_FLOW_MAX     1000

// speed ist u16 voll
#define CL_SPEED_MIN           0
#define CL_SPEED_MAX           65535

#define CL_PEDAL_ACT_MIN       0
#define CL_PEDAL_ACT_MAX       1

#define CL_PEDAL_MODE_MIN      0
#define CL_PEDAL_MODE_MAX      2

#define CL_STAND_INTAKES_MIN   0
#define CL_STAND_INTAKES_MAX   1

#define CL_FILTER_MIN          0
#define CL_FILTER_MAX          1000

struct {
  uint8_t  suctionLevel = DEF_SUCTIONLEVEL;
  uint16_t flow_x1000   = DEF_FLOW_X_MIL;     // R_FLOW (optional)
  uint16_t speed_rpm    = DEF_SPEED_RPM;      // R_SPEED (optional)
  uint16_t selectFlow   = DEF_SELECTFLOW_X_M; // R/W_SELECTFLOW
  uint8_t  standIntakes = DEF_STAND_INTAKES;  // R/W_STANDINTAKES (global)

  uint8_t  intakeAct[MAX_PORTS][NUM_INTAKES];
  uint16_t delaySec[MAX_PORTS][NUM_INTAKES];
  uint16_t tstopMs [MAX_PORTS][NUM_INTAKES];

  uint8_t  pedalAct[MAX_PORTS];
  uint8_t  pedalMode[MAX_PORTS];

  uint16_t filterLife       = DEF_FILTER_LIFE;
  uint16_t filterSaturation = DEF_FILTER_SAT;
  uint16_t statError        = DEF_STAT_ERROR;  // Bitmaske 0..0xFFFF

  uint8_t  usbConnect = 1;                    // R/W_USB_CONNECTSTATUS
  uint8_t  continuous = 0;                    // R/W_CONTINUOUSSUCTION
  uint8_t  connectedPedal[MAX_PORTS];         // R_CONNECTEDPEDAL (68)
} st;





// ===== Protokoll Konstanten ==================================================
static const uint8_t DLE = 0x10, STX = 0x02, ETX = 0x03;
static const uint8_t FID_HS = 0xFD; //0xFD
static const uint8_t FID_FE = 0xFE; //0xFE
static const uint8_t CTRL_SYN_P02 = 0x16;

// ===== Bus-Kontext ===========================================================
enum LinkState { LINK_DOWN, LINK_CONNECTING, LINK_UP };
enum BaseState { BASE_IDLE, BASE_SEEN_NAK, BASE_SENT_SYN, BASE_GOT_ACK1, BASE_SENT_ACK2, BASE_GOT_SOH, BASE_P02_ACTIVE };
enum RxState   { WAIT_DLE, WAIT_STX, IN_FRAME };






struct BusCtx {
  // UART
  HardwareSerial* ser;
#if defined(ARDUINO_ARCH_ESP32)
  int rxPin, txPin; // nur für ESP32 .begin(...)
#endif
  // Adressen
  uint8_t my_addr  = 0x91;
  uint8_t stn_addr = 0x18;
  bool    addr_locked = false;

  // Link/Parser
  LinkState linkState = LINK_DOWN;
  BaseState baseState = BASE_IDLE;
  RxState   rxState   = WAIT_DLE;

  // Parser-Puffer
  static const size_t MAX_PAY = 512;
  uint8_t payBuf[MAX_PAY]; size_t payLen = 0;
  bool in_escape=false, have_len=false; uint8_t data_len=0;

  // Timings
  uint32_t t_last_rx=0, t_last_syn=0, t_last_hb=0, t_last_rx_us=0;
  uint32_t last_for_us_ms = 0;
  uint32_t t_last_intake_ms = 0;  // Zeitstempel letzte IntakeActivation

  
  // DeviceID (pro Bus)
  uint8_t  dev_id[64];
  uint8_t  dev_id_len = 0;
  uint8_t  persist_devid_status = 0; // 0=none,1=ok
  uint8_t  persist_addr_status  = 0;
};


static BusCtx g_bus[MAX_BUSES];



static inline void intake_touch(uint8_t i){
  g_bus[i].t_last_intake_ms = millis();
}

static inline bool fwloop_active(uint8_t i){
  return (g_bus[i].linkState == LINK_UP) &&
         ((int32_t)(millis() - (int32_t)g_bus[i].t_last_intake_ms) >= (int32_t)LED_FW_IDLE_MS);
}


// ===== UART / Pins je nach Plattform ========================================
#if defined(ARDUINO_ARCH_ESP32)
  // BUS0: Serial2 standard 16/17, BUS1: Serial1 default 32/33 (anpassen falls nötig)
  #ifndef JBC0_RX_PIN
  #define JBC0_RX_PIN 16
  #endif
  #ifndef JBC0_TX_PIN
  #define JBC0_TX_PIN 17
  #endif
  #ifndef JBC1_RX_PIN
  #define JBC1_RX_PIN 32
  #endif
  #ifndef JBC1_TX_PIN
  #define JBC1_TX_PIN 33
  #endif
  static inline void JBC_BEGIN(uint8_t i){
    if(i==0) g_bus[i].ser = &Serial2;
    else     g_bus[i].ser = &Serial1;
    g_bus[i].rxPin = (i==0?JBC0_RX_PIN:JBC1_RX_PIN);
    g_bus[i].txPin = (i==0?JBC0_TX_PIN:JBC1_TX_PIN);
    g_bus[i].ser->begin(250000, SERIAL_8E1, g_bus[i].rxPin, g_bus[i].txPin);
  }
#elif defined(ARDUINO_ARCH_AVR)
  static inline void JBC_BEGIN(uint8_t i){
    if(i==0) g_bus[i].ser = &Serial1; else g_bus[i].ser = &Serial2; // Mega
    g_bus[i].ser->begin(250000, SERIAL_8E1); 
  }
#else
  static inline void JBC_BEGIN(uint8_t i){
    if(i==0) g_bus[i].ser = &Serial1; else g_bus[i].ser = &Serial2;
    g_bus[i].ser->begin(250000, SERIAL_8E1);
  }
#endif

#define DBG Serial
static inline void print_prompt() { DBG.print(F("> ")); }
static void log_fw_banner(){
  dbg_time_prefix();
  DBG.print(F("[FW] "));
#if defined(ARDUINO_ARCH_AVR)
  // FE_FW_STR liegt im PROGMEM → byteweise ausgeben
  for (uint8_t i = 0; i < sizeof(FE_FW_STR) - 1; ++i){
    DBG.write(pgm_read_byte(&FE_FW_STR[i]));
  }
#else
  DBG.print(FE_FW_STR);
#endif
  DBG.print(F(" | by IceCube20 | build "));
  DBG.print(F(__DATE__));
  DBG.print(' ');
  DBG.println(F(__TIME__));
}

// ===== DBG_PRINTF SRAM-schonend =============================================
#if defined(ARDUINO_ARCH_ESP32)
  #define DBG_PRINTF(...) DBG.printf(__VA_ARGS__)
#elif defined(ARDUINO_ARCH_AVR)
  #include <stdarg.h>
  #include <stdio.h>
  static void DBG_VPRINTF_P(const char* fmtP, ...) {
    char buf[192];
    va_list ap; va_start(ap, fmtP);
    vsnprintf_P(buf, sizeof(buf), fmtP, ap);
    va_end(ap);
    DBG.print(buf);
  }
  #define DBG_PRINTF(fmt, ...) DBG_VPRINTF_P(PSTR(fmt), ##__VA_ARGS__)
#else
  #include <stdarg.h>
  #include <stdio.h>
  static void DBG_PRINTF(const char* fmt, ...) {
    char buf[192]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap); DBG.print(buf);
  }
#endif

// ===== Utils / BCC ===========================================================
#ifndef P02_BCC_SEED
#define P02_BCC_SEED 0x01
#endif
#ifndef P02_BCC_INCLUDE_STX
#define P02_BCC_INCLUDE_STX 0
#endif
static inline uint8_t bcc_calc_xor(const uint8_t* p, size_t n){
  uint8_t b = P02_BCC_SEED;
  for(size_t i=0;i<n;i++) b ^= p[i];
#if P02_BCC_INCLUDE_STX
  b ^= 0x02;
#endif
  return b;
}
static size_t stuff_bytes(const uint8_t* in,size_t n,uint8_t* out,size_t outMax){
  size_t o=0;
  for(size_t i=0;i<n && o<outMax;i++){
    uint8_t b=in[i];
    out[o++]=b;
    if(b==DLE && o<outMax) out[o++]=DLE;
  }
  return o;
}

// ===== Logging-Helfer ========================================================
static inline void dbg_time_prefix(){
#if DBG_TS_MODE==0
  (void)0;
#elif DBG_TS_MODE==1
  #if LOG_ICONS
    DBG_PRINTF("⏱%lu ", (unsigned long)millis());
  #else
    DBG_PRINTF("[%lu] ", (unsigned long)millis());
  #endif
#elif DBG_TS_MODE==2
  unsigned long ms = millis();
  unsigned long s  = ms / 1000;
  unsigned long d  = (ms % 1000) / 100; // eine Dezimalstelle
  #if LOG_ICONS
    DBG_PRINTF("⏱%lu.%lus ", s, d);
  #else
    DBG_PRINTF("[%lu.%lus] ", s, d);
  #endif
#endif
}
static inline void print_onoff_icon(bool on){
#if LOG_ICONS
  DBG.print(on ? F("🟢ON") : F("⚪OFF"));
#else
  DBG.print(on ? F("ON") : F("OFF"));
#endif
}
static void print_ascii_quoted(const uint8_t* p, uint8_t n){
  // trailing spaces abschneiden nur fürs Log
  while(n && p[n-1]==0x20) n--;
  DBG.print('"');
  for(uint8_t i=0;i<n;i++){
    char c=(char)p[i];
    if(c>=0x20 && c<=0x7E) DBG.write(c);
    else DBG.write('.');
  }
  DBG.print('"');
}
static inline bool is_intake_activation_ctrl(uint8_t ctrl){
  using namespace jbc_cmd::FE_02;
  return ctrl == M_R_INTAKEACTIVATION || ctrl == M_W_INTAKEACTIVATION;
}

// ===== Relais Helpers + Zustand =============================================
static bool     g_relay_on = false;
static uint32_t g_relay_off_deadline = 0;
static inline void relay_hw_write(bool on){
#if RELAY_ENABLE
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? !on : on);
#else
  (void)on;
#endif
}
static inline void relay_set(bool on){
#if RELAY_ENABLE
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? !on : on);
#else
  (void)on;
#endif

  if (g_relay_on == on){
    // sicherstellen, dass der echte HW-Zustand geschrieben ist
    return;
  }
  g_relay_on = on;

#if DBG_SHOW_STATE
  dbg_time_prefix();
  DBG.print(F("[RELAY] "));
  print_onoff_icon(on);
  if (on) {
    DBG.print(F(" Delay Work "));
    DBG.print(st.delaySec[0][WORK_IDX]); // Nachlaufzeit (Work) anzeigen
    DBG.print(F("s"));
  }
  DBG.println();
#endif
}

// --- Bus-übergreifender WORK-Status als Bitmaske ----------------------------
// Wer "besitzt" den Nachlauf? -1 = keiner, sonst Busindex 0..(MAX_BUSES-1)
static int8_t g_afterrun_owner = -1;
static uint8_t g_work_mask = 0;                 // Bit i: WORK aktiv auf Bus i
// Helper: ist Bus i aktiv?
static inline bool bus_active(uint8_t i){
  return (g_work_mask & (uint8_t)(1u << i)) != 0;
}



static inline void work_set(uint8_t bus, bool on){
  if (on) g_work_mask |=  (uint8_t)(1u << bus);
  else    g_work_mask &= ~(uint8_t)(1u << bus);
}

static inline bool any_work_active(){           // ersetzt die alte Version
  return g_work_mask != 0;
}


// --- Nachlauf-Helper: verbleibende Zeit ---
static inline uint32_t relay_time_left_ms(){
  if (!g_relay_off_deadline) return 0;
  uint32_t now = millis();
  return (g_relay_off_deadline > now) ? (g_relay_off_deadline - now) : 0;
}


// ===== Persistenz ============================================================
struct PersistCfg {
  uint16_t tstop_work, tstop_stand;
  uint8_t  suctionLevel;        
  uint16_t selectFlow;         
  uint8_t  pedal_act, pedal_mode, stand_intakes;
  uint16_t filter_life, filter_sat, stat_error;
};



static PersistCfg cfg;

#if defined(ARDUINO_ARCH_ESP32)
// --------- ESP32 (NVS) ---------
  #include <Preferences.h>
  static Preferences nv;
  static void persist_begin(){ nv.begin("jbcfae", /*readOnly=*/false); }

  static bool persist_load_devid(uint8_t bus){
    char key[8]; snprintf(key,sizeof(key),"devid%u",bus);
    size_t n = nv.getBytesLength(key);
    if (n == 0 || n > sizeof(g_bus[bus].dev_id)) return false;
    g_bus[bus].dev_id_len = nv.getBytes(key, g_bus[bus].dev_id, sizeof(g_bus[bus].dev_id));
    g_bus[bus].persist_devid_status = (g_bus[bus].dev_id_len>0)?1:0; return g_bus[bus].dev_id_len>0;
  }
  static void persist_save_devid(uint8_t bus){
    if(!g_bus[bus].dev_id_len) return;
    char key[8]; snprintf(key,sizeof(key),"devid%u",bus);
    nv.putBytes(key, g_bus[bus].dev_id, g_bus[bus].dev_id_len);
  }
  static bool persist_load_addr(uint8_t bus, uint8_t &out){
    char key[6]; snprintf(key,sizeof(key),"addr%u",bus);
    uint8_t a = nv.getUChar(key, 0x00);
    if(a==0x00||a==0xFF) return false; out=a; return true;
  }
  static void persist_save_addr(uint8_t bus, uint8_t a){
    if(a==0x00) return;
    char key[6]; snprintf(key,sizeof(key),"addr%u",bus);
    nv.putUChar(key, a);
  }
  static bool persist_load_cfg(){
    size_t n = nv.getBytesLength("cfg");
    if(n != sizeof(PersistCfg)) return false;
    size_t got = nv.getBytes("cfg", &cfg, sizeof(PersistCfg));
    return got==sizeof(PersistCfg);
  }
  static void persist_save_cfg(){
    nv.putBytes("cfg", &cfg, sizeof(PersistCfg));
  }
  static uint8_t persist_load_buscount(){
    uint8_t bc = nv.getUChar("buscnt", BUS_COUNT);
    if (bc < 1 || bc > MAX_BUSES) bc = BUS_COUNT;
    return bc;
  }
  static void persist_save_buscount(uint8_t bc){
    if (bc < 1 || bc > MAX_BUSES) return;
    nv.putUChar("buscnt", bc);
  }

#elif defined(ARDUINO_ARCH_AVR)
// --------- AVR (EEPROM) Dual-Bus ---------
  #include <EEPROM.h>
  // Per-Bus Block (ähnlich wie Single-Bus-Version, aber 2x nacheinander)
  // Layout je Bus:
  //   0..3   MAGIC "JBCF" (0x4643424A LE)
  //      4   ver (2)
  //      5   devid_len (1..64)
  //      6   addr (!=0x00)
  //      7   reserved (0)
  //   8..9   checksum (sum of len, addr and devid data, LE)
  //  10..    devid bytes (up to 64)
  // Blockgröße: 10 + 64 = 74 Bytes
  static const uint32_t EEP_MAGIC      = 0x4643424AUL;
  static const uint8_t  EEP_VER        = 2;
  static const int      EEP_BUS_BLOCK  = 74;
  static const int      EEP_BUS0_OFF   = 0;
  static const int      EEP_BUS1_OFF   = EEP_BUS0_OFF + EEP_BUS_BLOCK;

  // Config-Block:
  //   MAGIC "CFG2" (0x32474643 LE), len=sizeof(PersistCfg), checksum (16-bit sum), data...
  static const uint32_t EEP_CFG_MAGIC  = 0x32474643UL; // 'C''F''G''2'
  static const int      EEP_CFG_HDR    = 7;
  static const int      EEP_CFG_OFF    = EEP_BUS1_OFF + EEP_BUS_BLOCK; // hinter Bus1

  static inline uint16_t eep_checksum(const uint8_t* id, uint8_t len, uint8_t addr){
    uint16_t s = (uint16_t)len + (uint16_t)addr;
    for(uint8_t i=0;i<len;i++) s += id[i];
    return s;
  }
  static inline void eep_write_bytes(int off, const uint8_t* src, int n){
    for(int i=0;i<n;i++) EEPROM.update(off+i, src[i]);
  }
  static inline void eep_read_bytes(int off, uint8_t* dst, int n){
    for(int i=0;i<n;i++) dst[i] = EEPROM.read(off+i);
  }
  static int eep_bus_off(uint8_t bus){
    return (bus==0) ? EEP_BUS0_OFF : EEP_BUS1_OFF;
  }

  static void persist_begin(){ /* nothing */ }

  static bool persist_load_devid(uint8_t bus){
    int off = eep_bus_off(bus);
    uint8_t hdr[10]; eep_read_bytes(off, hdr, 10);
    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1]<<8) | ((uint32_t)hdr[2]<<16) | ((uint32_t)hdr[3]<<24);
    if (magic != EEP_MAGIC) { g_bus[bus].persist_devid_status = 0; return false; }
    uint8_t ver = hdr[4];
    uint8_t len = hdr[5];
    uint8_t addr= hdr[6];
    uint16_t csum = (uint16_t)hdr[8] | ((uint16_t)hdr[9]<<8);
    if (ver != EEP_VER) { g_bus[bus].persist_devid_status = 0; return false; }
    if (len == 0 || len > sizeof(g_bus[bus].dev_id)) { g_bus[bus].persist_devid_status = 0; return false; }
    // Read id
    eep_read_bytes(off+10, g_bus[bus].dev_id, len);
    if (eep_checksum(g_bus[bus].dev_id, len, addr) != csum) { g_bus[bus].persist_devid_status = 0; return false; }
    g_bus[bus].dev_id_len = len;
    if (addr != 0x00 && addr != 0xFF) g_bus[bus].my_addr = addr;
    g_bus[bus].persist_devid_status = 1;
    return true;
  }

  static void persist_save_devid(uint8_t bus){
    int off = eep_bus_off(bus);
    // Read existing header to preserve addr if possible
    uint8_t hdr_old[10]; eep_read_bytes(off, hdr_old, 10);
    uint32_t magic_old = (uint32_t)hdr_old[0] | ((uint32_t)hdr_old[1]<<8) | ((uint32_t)hdr_old[2]<<16) | ((uint32_t)hdr_old[3]<<24);
    uint8_t addr = (magic_old == EEP_MAGIC) ? hdr_old[6] : g_bus[bus].my_addr;
    if (addr == 0x00) addr = g_bus[bus].my_addr;

    if (g_bus[bus].dev_id_len == 0 || g_bus[bus].dev_id_len > sizeof(g_bus[bus].dev_id)) return;

    // Avoid wear if same
    bool same = true;
    if (magic_old == EEP_MAGIC && hdr_old[5] == g_bus[bus].dev_id_len){
      for(uint8_t i=0;i<g_bus[bus].dev_id_len;i++){
        if (EEPROM.read(off+10+i) != g_bus[bus].dev_id[i]) { same = false; break; }
      }
      if (same && hdr_old[6] == addr) return;
    }

    uint8_t hdr[10];
    hdr[0]=(uint8_t)(EEP_MAGIC & 0xFF);
    hdr[1]=(uint8_t)((EEP_MAGIC>>8) & 0xFF);
    hdr[2]=(uint8_t)((EEP_MAGIC>>16)& 0xFF);
    hdr[3]=(uint8_t)((EEP_MAGIC>>24)& 0xFF);
    hdr[4]=EEP_VER;
    hdr[5]=g_bus[bus].dev_id_len;
    hdr[6]=addr ? addr : g_bus[bus].my_addr;
    hdr[7]=0;
    uint16_t sum = eep_checksum(g_bus[bus].dev_id, g_bus[bus].dev_id_len, hdr[6]);
    hdr[8]=(uint8_t)(sum & 0xFF);
    hdr[9]=(uint8_t)(sum >> 8);

    eep_write_bytes(off, hdr, 10);
    eep_write_bytes(off+10, g_bus[bus].dev_id, g_bus[bus].dev_id_len);
  }

  static bool persist_load_addr(uint8_t bus, uint8_t &out){
    int off = eep_bus_off(bus);
    uint8_t hdr[10]; eep_read_bytes(off, hdr, 10);
    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1]<<8) | ((uint32_t)hdr[2]<<16) | ((uint32_t)hdr[3]<<24);
    if (magic != EEP_MAGIC) { return false; }
    uint8_t ver = hdr[4];
    uint8_t len = hdr[5];
    uint8_t addr= hdr[6];
    uint16_t csum = (uint16_t)hdr[8] | ((uint16_t)hdr[9]<<8);
    if (ver != EEP_VER) { return false; }
    if (len == 0 || len > sizeof(g_bus[bus].dev_id)) { return false; }
    // Validate checksum with stored id
    uint8_t tmp[64]; eep_read_bytes(off+10, tmp, len);
    if (eep_checksum(tmp, len, addr) != csum) { return false; }
    if (addr == 0x00 || addr == 0xFF) { return false; }
    out = addr;
    return true;
  }

  static void persist_save_addr(uint8_t bus, uint8_t a){
    if (a == 0x00) return;
    int off = eep_bus_off(bus);
    // Load existing id to keep it
    uint8_t hdr_old[10]; eep_read_bytes(off, hdr_old, 10);
    uint32_t magic_old = (uint32_t)hdr_old[0] | ((uint32_t)hdr_old[1]<<8) | ((uint32_t)hdr_old[2]<<16) | ((uint32_t)hdr_old[3]<<24);
    uint8_t len = (magic_old == EEP_MAGIC) ? hdr_old[5] : g_bus[bus].dev_id_len;
    if (len == 0 || len > sizeof(g_bus[bus].dev_id)) len = g_bus[bus].dev_id_len;
    uint8_t buf[64];
    if (len) eep_read_bytes(off+10, buf, len);
    else {
      if (g_bus[bus].dev_id_len) { memcpy(buf, g_bus[bus].dev_id, g_bus[bus].dev_id_len); len = g_bus[bus].dev_id_len; }
    }

    uint8_t hdr[10];
    hdr[0]=(uint8_t)(EEP_MAGIC & 0xFF);
    hdr[1]=(uint8_t)((EEP_MAGIC>>8) & 0xFF);
    hdr[2]=(uint8_t)((EEP_MAGIC>>16)& 0xFF);
    hdr[3]=(uint8_t)((EEP_MAGIC>>24)& 0xFF);
    hdr[4]=EEP_VER;
    hdr[5]=len;
    hdr[6]=a;
    hdr[7]=0;
    uint16_t sum = eep_checksum((len?buf:g_bus[bus].dev_id), len, a);
    hdr[8]=(uint8_t)(sum & 0xFF);
    hdr[9]=(uint8_t)(sum >> 8);

    bool same = (magic_old == EEP_MAGIC && hdr_old[6] == a);
    if (!same){
      eep_write_bytes(off, hdr, 10);
      if (len) eep_write_bytes(off+10, (len?buf:g_bus[bus].dev_id), len);
    }
  }

  static inline uint16_t cfg_checksum(const uint8_t* p, uint8_t n){
    uint16_t s=0;
    for(uint8_t i=0;i<n;i++) s+=p[i];
    return s;
  }
  static bool persist_load_cfg(){
    uint8_t hdr[EEP_CFG_HDR];
    eep_read_bytes(EEP_CFG_OFF, hdr, EEP_CFG_HDR);
    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1]<<8) | ((uint32_t)hdr[2]<<16) | ((uint32_t)hdr[3]<<24);
    if (magic != EEP_CFG_MAGIC) return false;
    uint8_t len = hdr[4];
    uint16_t chk = (uint16_t)hdr[5] | ((uint16_t)hdr[6]<<8);
    if (len != sizeof(PersistCfg)) return false;
    eep_read_bytes(EEP_CFG_OFF+EEP_CFG_HDR, (uint8_t*)&cfg, len);
    if (cfg_checksum((uint8_t*)&cfg, len) != chk) return false;
    return true;
  }
  static void persist_save_cfg(){
    uint8_t hdr[EEP_CFG_HDR];
    hdr[0]=(uint8_t)(EEP_CFG_MAGIC & 0xFF);
    hdr[1]=(uint8_t)((EEP_CFG_MAGIC>>8)&0xFF);
    hdr[2]=(uint8_t)((EEP_CFG_MAGIC>>16)&0xFF);
    hdr[3]=(uint8_t)((EEP_CFG_MAGIC>>24)&0xFF);
    hdr[4]=(uint8_t)sizeof(PersistCfg);
    uint16_t chk = cfg_checksum((uint8_t*)&cfg, sizeof(PersistCfg));
    hdr[5]=(uint8_t)(chk & 0xFF);
    hdr[6]=(uint8_t)(chk >> 8);
    eep_write_bytes(EEP_CFG_OFF, hdr, EEP_CFG_HDR);
    eep_write_bytes(EEP_CFG_OFF+EEP_CFG_HDR, (const uint8_t*)&cfg, sizeof(PersistCfg));
  }
  // Ein Byte hinter dem CFG-Block als Buscount-Ablage
  static const int EEP_BUSCOUNT_OFF = EEP_CFG_OFF + EEP_CFG_HDR + (int)sizeof(PersistCfg);
  
  static uint8_t persist_load_buscount(){
    uint8_t bc = EEPROM.read(EEP_BUSCOUNT_OFF);
    if (bc == 0xFF || bc == 0x00) bc = BUS_COUNT;
    if (bc < 1 || bc > MAX_BUSES) bc = BUS_COUNT;
    return bc;
  }
  static void persist_save_buscount(uint8_t bc){
    if (bc < 1 || bc > MAX_BUSES) return;
    EEPROM.update(EEP_BUSCOUNT_OFF, bc);
  }
  

#else
  // Fallback: keine Persistenz
  static void persist_begin(){}
  static bool persist_load_devid(uint8_t /*bus*/){ return false; }
  static void persist_save_devid(uint8_t /*bus*/){}
  static bool persist_load_addr(uint8_t /*bus*/, uint8_t &/*out*/){ return false; }
  static void persist_save_addr(uint8_t /*bus*/, uint8_t /*a*/){}
  static bool persist_load_cfg(){ return false; }
  static void persist_save_cfg(){}
  static uint8_t persist_load_buscount(){ return BUS_COUNT; }
  static void     persist_save_buscount(uint8_t /*bc*/){ /* noop */ }
#endif

static void deviceid_init_defaults(){
  // BUS0 Default
#if defined(ARDUINO_ARCH_AVR)
  for(uint8_t i=0;i<32;i++) g_bus[0].dev_id[i] = pgm_read_byte(&FE_DEVICE_ID_STR0[i]);
#else
  memcpy(g_bus[0].dev_id, FE_DEVICE_ID_STR0, 32);
#endif
  g_bus[0].dev_id_len = 32;
  // BUS1 Default (falls vorhanden)
#if BUS_COUNT>=2
  #if defined(ARDUINO_ARCH_AVR)
    for(uint8_t i=0;i<32;i++) g_bus[1].dev_id[i] = pgm_read_byte(&FE_DEVICE_ID_STR1[i]);
  #else
    memcpy(g_bus[1].dev_id, FE_DEVICE_ID_STR1, 32);
  #endif
  g_bus[1].dev_id_len = 32;
#endif
}
// --- ERASE: löscht alle Persistenzdaten (NVS/EEPROM) ------------------------
static void persist_erase_all(){
#if defined(ARDUINO_ARCH_ESP32)
  // Löscht alle Keys in der Namespace "jbcfae"
  nv.clear();
#elif defined(ARDUINO_ARCH_AVR)
  // Löscht die von uns belegten EEPROM-Bereiche mit 0xFF
  // Bereich: Bus0-Block + Bus1-Block + CFG-Header+Daten + Buscount-Byte
  for (int i = 0; i <= EEP_BUSCOUNT_OFF; ++i) EEPROM.update(i, 0xFF);
#else
  // keine Persistenz
#endif
}

// ===== Laufzeit-Konfiguration (CLI) =========================================
static bool     g_state_dirty = true;
static uint32_t t_last_state  = 0;
static inline void mark_state_dirty(){ g_state_dirty = true; }
static inline void maybe_dump_state_on_write(){
#if DBG_SHOW_STATE && DBG_STATE_ON_WRITE_ONLY
  if(g_state_dirty){ /* dump_state() unten */ g_state_dirty=false; }
#endif
}



// ===== Fehlerkürzel / Warnungen -> Bitmaske + Beschreibung ==================
// Hinweis: KEIN PSTR/F() in globalen Initialisierern benutzen (AVR)!
struct ErrInfo {
  const char* key;
  uint16_t    mask;
  flashstr_t  desc;   // auf AVR: __FlashStringHelper*, sonst const char*
};

#if defined(ARDUINO_ARCH_AVR)
// Texte im Flash ablegen und als Zeiger verwenden
static const char E_STOP1[]  PROGMEM = "Standzeit Filter ist abgelaufen";
static const char E_STOP1_EN[]  PROGMEM = "Filter lifetime expired";

static const char E_WARN1[]  PROGMEM = "Standzeit Filter endet in X Tagen";
static const char E_WARN1_EN[]  PROGMEM = "Filter lifetime ends in X days";

static const char E_STOP2[]  PROGMEM = "Der Filter ist Verstopft";
static const char E_STOP2_EN[]  PROGMEM = "The filter is clogged";

static const char E_WARN2[]  PROGMEM = "Der Filter ist kurz davor zu Verstopfen";
static const char E_WARN2_EN[]  PROGMEM = "The filter is about to clog";

static const char E_STOP3[]  PROGMEM = "Kein Filter entdeckt!";
static const char E_STOP3_EN[]  PROGMEM = "No filter detected!";

static const char E_STOP4[]  PROGMEM = "Filter Abdeckung offen";
static const char E_STOP4_EN[]  PROGMEM = "Filter cover open";

static const char E_STOP5[]  PROGMEM = "Gebläse beschädigt";
static const char E_STOP5_EN[]  PROGMEM = "Blower damaged";

static const char E_STOP7[]  PROGMEM = "Ventilfehler";
static const char E_STOP7_EN[]  PROGMEM = "Valve error";

static const char E_STOP8[]  PROGMEM = "Überstrom am Hilfsport entdeckt";
static const char E_STOP8_EN[]  PROGMEM = "Overcurrent detected on auxiliary port";

static const char E_STOP9[]  PROGMEM = "Pedalfehler";
static const char E_STOP9_EN[]  PROGMEM = "Pedal error";

static const char E_STOP10[] PROGMEM = "Systemfehler FAE";
static const char E_STOP10_EN[] PROGMEM = "FAE system error";

static const char E_STOP11[] PROGMEM = "Systemfehler FAE";
static const char E_STOP11_EN[] PROGMEM = "FAE system error";

#define DESC(x) (reinterpret_cast<flashstr_t>(x))


// #if defined(ARDUINO_ARCH_AVR)
// // Texte im Flash ablegen und als Zeiger verwenden
// static const char E_STOP1[]  PROGMEM = "Standzeit Filter ist abgelaufen";
// static const char E_WARN1[]  PROGMEM = "Standzeit Filter endet in X Tagen";
// static const char E_STOP2[]  PROGMEM = "Der Filter ist Verstopft";
// static const char E_WARN2[]  PROGMEM = "Der Filter ist kurz davor zu Verstopfen";
// static const char E_STOP3[]  PROGMEM = "Kein Filter entdeckt!";
// static const char E_STOP4[]  PROGMEM = "Filter Abdeckung offen";
// static const char E_STOP5[]  PROGMEM = "Gebläse beschädigt";
// static const char E_STOP7[]  PROGMEM = "Ventilfehler";
// static const char E_STOP8[]  PROGMEM = "Überstrom am Hilfsport entdeckt";
// static const char E_STOP9[]  PROGMEM = "Pedalfehler";
// static const char E_STOP10[] PROGMEM = "Systemfehler FAE";
// static const char E_STOP11[] PROGMEM = "Systemfehler FAE";
// #define DESC(x) (reinterpret_cast<flashstr_t>(x))

static const ErrInfo kErrMap[] = {
  {"STOP1",   1u,    DESC(E_STOP1)},
  {"WARN1",   2u,    DESC(E_WARN1)},
  {"STOP2",   4u,    DESC(E_STOP2)},
  {"WARN2",   8u,    DESC(E_WARN2)},
  {"STOP3",   16u,   DESC(E_STOP3)},
  {"STOP4",   32u,   DESC(E_STOP4)},
  {"STOP5",   64u,   DESC(E_STOP5)},
  {"STOP7",   256u,  DESC(E_STOP7)},
  {"STOP8",   512u,  DESC(E_STOP8)},
  {"STOP9",   1024u, DESC(E_STOP9)},
  {"STOP10",  2048u, DESC(E_STOP10)},
  {"STOP11",  4096u, DESC(E_STOP11)},
};
#else

// Nicht-AVR: normale C-Strings
#define DESC(x) (x)
static const ErrInfo kErrMap[] = {
  {"STOP1",   1u,    "Standzeit Filter ist abgelaufen / Filter lifetime expired"},
  {"WARN1",   2u,    "Standzeit Filter endet in X Tagen / Filter lifetime ends in X days"},
  {"STOP2",   4u,    "Der Filter ist Verstopft / The filter is clogged"},
  {"WARN2",   8u,    "Der Filter ist kurz davor zu Verstopfen / The filter is about to clog"},
  {"STOP3",   16u,   "Kein Filter entdeckt! / No filter detected!"},
  {"STOP4",   32u,   "Filter Abdeckung offen / Filter cover open"},
  {"STOP5",   64u,   "Gebläse beschädigt / Blower damaged"},
  {"STOP7",   256u,  "Ventilfehler / Valve error"},
  {"STOP8",   512u,  "Überstrom am Hilfsport entdeckt / Overcurrent detected on auxiliary port"},
  {"STOP9",   1024u, "Pedalfehler / Pedal error"},
  {"STOP10",  2048u, "Systemfehler FAE / FAE system error"},
  {"STOP11",  4096u, "Systemfehler FAE / FAE system error"},
};
#endif




static const uint16_t ERR_MASK_WARN = (2u | 8u);
static inline bool has_stop_error(uint16_t e){ return (e & ~ERR_MASK_WARN) != 0; }
static inline bool has_warn_error(uint16_t e){ return !has_stop_error(e) && (e & ERR_MASK_WARN); }

static uint16_t lookup_err_mask(const char* s){
  for (size_t i=0;i<sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    if (!strcasecmp(s, kErrMap[i].key)) return kErrMap[i].mask;
  }
  return 0;
}


// ---- Mini-CLI ---------------------------------------------------------------
static char cli_buf[96]; static uint8_t cli_len=0;
static int  cli_token(char* s, char** argv, int maxv){
  int n=0; bool in=false; for(char* p=s; *p && n<maxv; ++p){
    if(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'){ *p=0; in=false; }
    else if(!in){ argv[n++]=p; in=true; }
  }
  return n;
}
static long cli_parse_long(const char* s, bool* ok){
  if(!s){ *ok=false; return 0; }
  char* end=nullptr; long v = strtol(s, &end, 0);
  *ok = (end && *end==0);
  return v;
}
static void apply_buscount(uint8_t newcnt, bool save){
  if (newcnt < 1 || newcnt > MAX_BUSES) {
    DBG.println(F("[BUS] invalid bus count (use 1 or 2)"));
    return;
  }
  if (newcnt == g_bus_count) {
    if (save) persist_save_buscount(g_bus_count);
    return;
  }

  uint8_t old = g_bus_count;
  g_bus_count = newcnt;

  // Neu aktivierte Busse initialisieren
  for (uint8_t i = old; i < g_bus_count; ++i) {
    JBC_BEGIN(i);
    g_bus[i].baseState = BASE_IDLE;
    g_bus[i].linkState = LINK_DOWN;
    g_bus[i].rxState   = WAIT_DLE;
    g_bus[i].addr_locked = false;
  }

  // Deaktivierte Busse beenden
  for (uint8_t i = g_bus_count; i < old; ++i) {
    if (g_bus[i].ser) {
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_AVR)
      g_bus[i].ser->end();
      work_set(i, false);
      if (g_afterrun_owner == (int8_t)i) g_afterrun_owner = -1;

#endif
    }
    g_bus[i].baseState = BASE_IDLE;
    g_bus[i].linkState = LINK_DOWN;
    g_bus[i].rxState   = WAIT_DLE;
    g_bus[i].payLen = 0; g_bus[i].in_escape=false; g_bus[i].have_len=false; g_bus[i].data_len=0;
  }

#if LED_ENABLE
  // LEDs oberhalb der aktiven Busse zwangsweise aus
  for (uint8_t idx = g_bus_count * 2; idx < LED_COUNT_MAX; ++idx) set_led_idx(idx, 0,0,0,0);
  pixels.show();
#endif

  if (save) persist_save_buscount(g_bus_count);
  DBG.print(F("[BUS] active buses = "));
  DBG.println(g_bus_count);
}

static void print_startup_banner(){
  print_persist_diag();
  cli_show_cfg();     // aktuelle Configzeile
  log_fw_banner();    // [FW] … build …
}

static void print_persist_diag(){
  dbg_time_prefix();
#if defined(ARDUINO_ARCH_ESP32)
  DBG.println(F("[NVS]"));
#elif defined(ARDUINO_ARCH_AVR)
  DBG.println(F("[EEP]"));
#else
  DBG.println(F("[PERSIST]"));
#endif

  for (uint8_t i = 0; i < MAX_BUSES; i++) {
    DBG.print(F(" "));
    DBG_PRINTF("bus%u DeviceID len=%u ascii=", i, g_bus[i].dev_id_len);
    print_ascii_quoted(g_bus[i].dev_id, g_bus[i].dev_id_len);
    DBG_PRINTF(" | addr=0x%02X", g_bus[i].my_addr);
    if (i >= g_bus_count) DBG.print(F(" | (inactive)"));
    DBG.println();
  }
  DBG.println(F("[CLI] type 'help' for commands"));
}


static void cli_show_help(){
  DBG.println(F("Commands:"));
  DBG.println(F("  help                     - Show this Help"));
  DBG.println(F("  ?                        - Show DEVID/Addr and other Data) "));
  DBG.println(F("  show cfg                 - Show the actual Configuration"));
  DBG.println(F("  show buses               - Show actual selected Buscount"));
  DBG.println(F("  set buses <1|2>          - select one or two JBC Stations to connect"));
  DBG.println(F("  save / load              - Save or Load configuration Data from EEPROM"));
  DBG.println(F("  erase [eeprom]           - clear NVS/EEPROM and reload defaults (use 'save' to persist) "));
  DBG.println(F("  set tstop_work <sec>     - Select the Relay afterrun time in seconds (for the DDE, you can set it up in the Stations Menu)"));
  DBG.println(F("  set tstop_stand <sec>    - No function! Its for internal testing"));
  DBG.println(F("  set suctionlevel <0..3>  - No function! Its for internal testing"));
  DBG.println(F("  set selectflow <0..1000> - No function! Its for internal testing"));
  //DBG.println(F("  set flow <0..1000>"));
  //DBG.println(F("  set speed <rpm>"));
  //DBG.println(F("  set pedal_act <0|1>"));
  //DBG.println(F("  set pedal_mode <0|1|2>"));
  DBG.println(F("  set stand_intakes <0|1>  - No function! Its for internal testing"));
  DBG.println(F("  set filter_life <u16>    - Set your own Filterlife in % (shown in JBC DDE FAE Menu)"));
  DBG.println(F("  set filter_sat <u16>     - Set your own Filtersaturation in % (shown in JBC DDE FAE Menu)"));
  DBG.println(F("  STOPx/WARNx, NOERR, ERR  - STOP1-11 Set Station errors, WARN1-2 Set Station warnings, NOERR resets Stations errors or warnings and ERR show all Error and Warnings they exist"));
}
static void cli_show_cfg(){
  DBG.print(F("[CFG] buses=")); DBG.print(g_bus_count);
  DBG.print(F(" | tstop(w/s)=")); DBG.print(cfg.tstop_work); DBG.print('/'); DBG.print(cfg.tstop_stand);
  DBG.print(F(" | selectflow=")); DBG.print(st.selectFlow);
  DBG.print(F(" | suctionlevel=")); DBG.print(st.suctionLevel);
  //DBG.print(F(" | speed_rpm=")); DBG.print(st.speed_rpm);
  //DBG.print(F(" | flow_x1000=")); DBG.print(st.flow_x1000);
  //DBG.print(F(" | pedal_act=")); DBG.print(cfg.pedal_act);
  //DBG.print(F(" | pedal_mode=")); DBG.print(cfg.pedal_mode);
  DBG.print(F(" | stand_intakes=")); DBG.print(cfg.stand_intakes);
  DBG.print(F(" | filter(life/sat)=")); DBG.print(st.filterLife); DBG.print('/'); DBG.print(st.filterSaturation);
  DBG.print(F(" | stat_error=")); DBG.print((unsigned)st.statError);
  DBG.print(F(" (0x")); DBG.print(st.statError, HEX); DBG.println(F(")"));
}


static void apply_cfg_to_state(){
  st.delaySec[0][WORK_IDX]=cfg.tstop_work;
  st.delaySec[0][STAND_IDX]=cfg.tstop_stand;
  st.suctionLevel=cfg.suctionLevel;
  st.selectFlow=cfg.selectFlow;
  st.pedalAct[0]=cfg.pedal_act;
  st.pedalMode[0]=cfg.pedal_mode;
  st.standIntakes=cfg.stand_intakes;
  st.filterLife=cfg.filter_life;
  st.filterSaturation=cfg.filter_sat;
  st.statError=cfg.stat_error;
  mark_state_dirty();
  maybe_dump_state_on_write();
}
static void cli_apply_stat(uint16_t v){
  cfg.stat_error=v;
  apply_cfg_to_state();
  DBG.print(F("[CLI] stat_error="));
  DBG.print((unsigned)st.statError);
  DBG.print(F(" (0x")); DBG.print(st.statError, HEX); DBG.println(F(")"));
}
static void cli_show_err_table(){
  DBG.println(F("Fehler/Warnungen (Bitmaske):"));
  for (size_t i=0;i<sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    const ErrInfo &e = kErrMap[i];
    DBG_PRINTF("  %-6s = 0x%04X (%u)  - ", e.key, (unsigned)e.mask, (unsigned)e.mask);
    if (e.desc) DBG.print(e.desc);
    DBG.println();
  }
  DBG.println(F("Hinweis: Es kann nur ein Fehler oder eine Warnung Emuliert werden."));
}
static void cli_show_active_errors(uint16_t e){
  if (e == 0){
    DBG.println(F("  (keine Fehler)"));
    return;
  }
  DBG.println(F("  aktive Flags:"));
  for (size_t i=0;i<sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    const ErrInfo &info = kErrMap[i];
    if (e & info.mask){
      DBG_PRINTF("   + %-6s - ", info.key);
      if (info.desc) DBG.print(info.desc);
      DBG.println();
    }
  }
}

static void cli_handle_line(char* line){
  char* argv[6]; int argc=cli_token(line,argv,6);
  if(argc<=0) return;
  if(!strcasecmp(argv[0],"banner") || !strcasecmp(argv[0],"config")){
    print_startup_banner();
    return;
  }

  if(!strcasecmp(argv[0],"help")){ cli_show_help(); return; }
  if(!strcasecmp(argv[0],"show") && argc>=2 && !strcasecmp(argv[1],"cfg")){ cli_show_cfg(); return; }
  if(!strcasecmp(argv[0],"show") && argc>=2 && !strcasecmp(argv[1],"buses")){
    DBG.print(F("[BUS] active buses = ")); DBG.println(g_bus_count);
    return;
  }
  if(!strcasecmp(argv[0],"save")){
    persist_save_cfg();
    persist_save_buscount(g_bus_count);
    DBG.println(F("[CLI] cfg + buses saved."));
    return;
  }

  if(!strcasecmp(argv[0],"load")){
    if(persist_load_cfg()){ apply_cfg_to_state(); DBG.println(F("[CLI] cfg loaded & applied.")); }
    else { DBG.println(F("[CLI] no cfg stored.")); }
    uint8_t bc = persist_load_buscount();
    apply_buscount(bc, /*save=*/false);
    return;
  }
    if(!strcasecmp(argv[0],"erase") || !strcasecmp(argv[0],"factory") || !strcasecmp(argv[0],"wipe")){
    persist_erase_all();

    // RAM auf Werkseinstellungen zurücksetzen (nicht automatisch gespeichert!)
    // Busanzahl zurück auf Default & UARTs/Puffer neu setzen
    apply_buscount(BUS_COUNT, /*save=*/false);

    // DeviceIDs zurücksetzen
    deviceid_init_defaults();
    for (uint8_t i = 0; i < MAX_BUSES; ++i){
      g_bus[i].persist_devid_status = 0;
      g_bus[i].persist_addr_status  = 0;
      g_bus[i].my_addr = 0x91;           // Defaultadresse
      g_bus[i].addr_locked = false;
    }

    // Konfig auf Defaults
    cfg.tstop_work    = DEF_TSTOP_WORK;
    cfg.tstop_stand   = DEF_TSTOP_STAND;
    cfg.suctionLevel  = DEF_SUCTIONLEVEL;
    cfg.selectFlow    = DEF_SELECTFLOW_X_M;
    cfg.pedal_act     = DEF_PEDAL_ACT;
    cfg.pedal_mode    = DEF_PEDAL_MODE;
    cfg.stand_intakes = DEF_STAND_INTAKES;
    cfg.filter_life   = DEF_FILTER_LIFE;
    cfg.filter_sat    = DEF_FILTER_SAT;
    cfg.stat_error    = DEF_STAT_ERROR;
    apply_cfg_to_state();

    // Laufzeitstatus (Relais/Fehler) auf neutral
    st.statError = 0;
    g_relay_off_deadline = 0;
    g_afterrun_owner = -1;
    g_work_mask = 0;
    relay_set(false);

#if LED_ENABLE
    // LEDs sauber zurücksetzen
    for(uint8_t idx=0; idx<LED_COUNT_MAX; ++idx) set_led_idx(idx,0,0,0,0);
    pixels.show();
#endif

    DBG.println(F("[ERASE] persistence wiped. Defaults reloaded (use 'save' to persist)."));
    return;
  }

  if(!strcasecmp(argv[0],"set") && argc>=3){
    bool ok=false; long v=cli_parse_long(argv[2],&ok);
    if(!ok){ DBG.println(F("[CLI] invalid number")); return; }
    if(!strcasecmp(argv[1],"buses")){
      bool ok=false; long v=cli_parse_long(argv[2], &ok);
      if(!ok || v<1 || v>2){ DBG.println(F("[CLI] invalid buses (use 1 or 2)")); return; }
      apply_buscount((uint8_t)v, /*save=*/false);
      DBG.println(F("[CLI] ok"));
      return;
    }
    if(!strcasecmp(argv[1],"tstop_work"))    { cfg.tstop_work     = (uint16_t)CLAMP(v, CL_TSTOP_WORK_MIN,   CL_TSTOP_WORK_MAX);   apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"tstop_stand"))   { cfg.tstop_stand    = (uint16_t)CLAMP(v, CL_TSTOP_STAND_MIN,  CL_TSTOP_STAND_MAX);  apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }

    if(!strcasecmp(argv[1],"suctionlevel"))  { cfg.suctionLevel   = (uint8_t) CLAMP(v, CL_SUCTIONLEVEL_MIN, CL_SUCTIONLEVEL_MAX); apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"selectflow"))    { cfg.selectFlow     = (uint16_t)CLAMP(v, CL_SELECTFLOW_MIN,   CL_SELECTFLOW_MAX);   apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }

    if(!strcasecmp(argv[1],"actual_flow"))   { st.flow_x1000      = (uint16_t)CLAMP(v, CL_ACTUAL_FLOW_MIN,  CL_ACTUAL_FLOW_MAX);  apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"speed"))         { st.speed_rpm       = (uint16_t)CLAMP(v, CL_SPEED_MIN,        CL_SPEED_MAX);        apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }

    if(!strcasecmp(argv[1],"pedal_act"))     { cfg.pedal_act      = (uint8_t) CLAMP(v, CL_PEDAL_ACT_MIN,    CL_PEDAL_ACT_MAX);    apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"pedal_mode"))    { cfg.pedal_mode     = (uint8_t) CLAMP(v, CL_PEDAL_MODE_MIN,   CL_PEDAL_MODE_MAX);   apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"stand_intakes")) { cfg.stand_intakes  = (uint8_t) CLAMP(v, CL_STAND_INTAKES_MIN,CL_STAND_INTAKES_MAX);apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }

    if(!strcasecmp(argv[1],"filter_life"))   { cfg.filter_life    = (uint16_t)CLAMP(v, CL_FILTER_MIN,       CL_FILTER_MAX);       apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"filter_sat"))    { cfg.filter_sat     = (uint16_t)CLAMP(v, CL_FILTER_MIN,       CL_FILTER_MAX);       apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    DBG.println(F("[CLI] unknown key"));
    return;
  }

  if(!strcasecmp(argv[0],"NOERR")){ cli_apply_stat(0); DBG.println(F("[CLI] ok")); return; }
  if(!strcasecmp(argv[0],"ERR") || !strcasecmp(argv[0],"err")){
    DBG.print(F("[CLI] stat_error=")); 
    DBG.print((unsigned)st.statError); 
    DBG.print(F(" (0x")); DBG.print(st.statError, HEX); DBG.println(F(")"));
    cli_show_active_errors(st.statError);
    cli_show_err_table();
    return;
  }

  const char* tok=argv[0]; char op=0;
  if(tok[0]=='+'||tok[0]=='-'){ op=tok[0]; tok++; }
  uint16_t m=lookup_err_mask(tok);
  if(m){
    if(op=='+')      cli_apply_stat((uint16_t)(st.statError|m));
    else if(op=='-') cli_apply_stat((uint16_t)(st.statError & (uint16_t)~m));
    else             cli_apply_stat(m);
    DBG.println(F("[CLI] ok"));
    return;
  }
  DBG.println(F("[CLI] unknown cmd (help)"));
}
static void cli_poll(){
  while(DBG.available()){
    char c=(char)DBG.read();
    if(c=='\r') continue;
    // --- HOTKEYS: Banner sofort anzeigen, wenn Puffer leer ist ---
    if(cli_len==0 && (c==BANNER_HOTKEY1 || c==BANNER_HOTKEY2 || c==BANNER_HOTKEY3)){
      print_startup_banner();
      // Hotkey NICHT in den Eingabepuffer übernehmen
      continue;
    }
    if(c=='\n'){
      cli_buf[cli_len]=0;
      if(cli_len>0) cli_handle_line(cli_buf);
      cli_len=0;
      DBG.print(F("> "));
    } else {
      if(cli_len+1<sizeof(cli_buf)) cli_buf[cli_len++]=c;
    }
  }
}

// ===== State-Ansicht =========================================================
static void dump_state(){
#if DBG_SHOW_STATE
  DBG.print(F("[STATE] SL=")); DBG.print(st.suctionLevel);
  DBG.print(F(" SF=")); DBG.print(st.selectFlow);
  DBG.print(F(" | Filter=Life ")); DBG.print(st.filterLife);
  DBG.print(F("/Sat ")); DBG.print(st.filterSaturation);
  DBG.print(F(" Err=")); DBG.print(st.statError);
  DBG.print(F(" | Relay=")); print_onoff_icon(g_relay_on);

  if (g_relay_off_deadline){
#if LOG_ICONS
    DBG.print(F(" ⏳"));
#else
    DBG.print(F(" rem="));
#endif
    uint32_t rem = relay_time_left_ms();
    DBG.print(rem/1000);
    DBG.print(F("s"));
  }
  DBG.println();
#endif
}



// ---- CTRL Namen (nur Log) --------------------------------------------------
static flashstr_t fe02_ctrl_name(uint8_t c){
  using namespace jbc_cmd::FE_02;
  switch(c){
    case M_HS: return FS_LIT("M_HS"); case M_EOT: return FS_LIT("M_EOT");
    case M_ACK: return FS_LIT("M_ACK"); case M_NACK: return FS_LIT("M_NACK");
    case M_SYN: return FS_LIT("M_SYN");
    case M_R_DEVICEIDORIGINAL: return FS_LIT("M_R_DEVICEIDORIGINAL");
    case M_R_DISCOVER: return FS_LIT("M_R_DISCOVER");
    case M_R_DEVICEID: return FS_LIT("M_R_DEVICEID");
    case M_W_DEVICEID: return FS_LIT("M_W_DEVICEID");
    case M_RESET: return FS_LIT("M_RESET");
    case M_FIRMWARE: return FS_LIT("M_FIRMWARE");
    case M_CLEARMEMFLASH: return FS_LIT("M_CLEARMEMFLASH");
    case M_SENDMEMADDRESS: return FS_LIT("M_SENDMEMADDRESS");
    case M_SENDMEMDATA: return FS_LIT("M_SENDMEMDATA");
    case M_ENDPROGR: return FS_LIT("M_ENDPROGR");
    case M_ENDUPD: return FS_LIT("M_ENDUPD");
    case M_CONTINUEUPD: return FS_LIT("M_CONTINUEUPD");
    case M_CLEARING: return FS_LIT("M_CLEARING");
    case M_FORCEUPDATE: return FS_LIT("M_FORCEUPDATE");
    case M_R_SUCTIONLEVEL: return FS_LIT("M_R_SUCTIONLEVEL");
    case M_W_SUCTIONLEVEL: return FS_LIT("M_W_SUCTIONLEVEL");
    case M_R_FLOW: return FS_LIT("M_R_FLOW");
    case M_R_SPEED: return FS_LIT("M_R_SPEED");
    case M_R_SELECTFLOW: return FS_LIT("M_R_SELECTFLOW");
    case M_W_SELECTFLOW: return FS_LIT("M_W_SELECTFLOW");
    case M_R_STANDINTAKES: return FS_LIT("M_R_STANDINTAKES");
    case M_W_STANDINTAKES: return FS_LIT("M_W_STANDINTAKES");
    case M_R_INTAKEACTIVATION: return FS_LIT("M_R_INTAKEACTIVATION");
    case M_W_INTAKEACTIVATION: return FS_LIT("M_W_INTAKEACTIVATION");
    case M_R_SUCTIONDELAY: return FS_LIT("M_R_SUCTIONDELAY");
    case M_W_SUCTIONDELAY: return FS_LIT("M_W_SUCTIONDELAY");
    case M_R_DELAYTIME: return FS_LIT("M_R_DELAYTIME");
    case M_R_ACTIVATIONPEDAL: return FS_LIT("M_R_ACTIVATIONPEDAL");
    case M_W_ACTIVATIONPEDAL: return FS_LIT("M_W_ACTIVATIONPEDAL");
    case M_R_PEDALMODE: return FS_LIT("M_R_PEDALMODE");
    case M_W_PEDALMODE: return FS_LIT("M_W_PEDALMODE");
    case M_R_FILTERSTATUS: return FS_LIT("M_R_FILTERSTATUS");
    case M_R_CONNECTEDPEDAL: return FS_LIT("M_R_CONNECTEDPEDAL");
    case M_R_CONTINUOUSSUCTION: return FS_LIT("M_R_CONTINUOUSSUCTION");
    case M_W_CONTINUOUSSUCTION: return FS_LIT("M_W_CONTINUOUSSUCTION");
    case M_R_USB_CONNECTSTATUS: return FS_LIT("M_R_USB_CONNECTSTATUS");
    case M_W_USB_CONNECTSTATUS: return FS_LIT("M_W_USB_CONNECTSTATUS");
    case M_R_FILTERSAT: return FS_LIT("M_R_FILTERSAT");
    case M_R_STATERROR: return FS_LIT("M_R_STATERROR");
    default: return nullptr;
  }
}
static inline void print_ctrl_name(uint8_t /*fid*/, uint8_t ctrl){
#if DBG_SHOW_RX || DBG_SHOW_TX
  flashstr_t n = fe02_ctrl_name(ctrl);
  if(n){ DBG.print(F(" (")); DBG.print(n); DBG.print(')'); }
#endif
}

// ====== TX Helpers (pro Bus) ================================================
static inline void tx_base(uint8_t i, uint8_t b){ g_bus[i].ser->write(&b,1); }
static inline void send_base_syn(uint8_t i){
  const uint32_t now=millis();
  if(now - g_bus[i].t_last_syn < 1500) return;
  tx_base(i, jbc_cmd::BASE::M_SYN);
  g_bus[i].t_last_syn=now;
#if DBG_SHOW_PROTO
  DBG_PRINTF("[BASE b%u] TX SYN\n", i);
#endif
}
static inline void send_base_ack(uint8_t i, const __FlashStringHelper* why){
  tx_base(i, jbc_cmd::BASE::M_ACK);
#if DBG_SHOW_PROTO
  DBG_PRINTF("[BASE b%u] TX ACK ", i); DBG.println(why);
#endif
}

static void send_p02(uint8_t i, const uint8_t* payload, size_t n){
  // --- Rahmen bauen + senden -------------------------------------------------
  uint8_t bcc = bcc_calc_xor(payload, n);
  static uint8_t txBuf[2 + BusCtx::MAX_PAY*2 + 3];
  size_t o = 0;
  txBuf[o++] = DLE; txBuf[o++] = STX;
  o += stuff_bytes(payload, n, txBuf + o, sizeof(txBuf) - o - 3);
  txBuf[o++] = bcc; txBuf[o++] = DLE; txBuf[o++] = ETX;
  g_bus[i].ser->write(txBuf, o);

  // --- TX-LOG (sichtbar wenn DBG_SHOW_TX == 1) -------------------------------
#if DBG_SHOW_TX
  if (n >= 5) {
    const uint8_t src  = payload[0];
    const uint8_t dst  = payload[1];
    const uint8_t fid  = payload[2];
    const uint8_t ctrl = payload[3];
    const uint8_t len  = payload[4];

    // Kopfzeile
    DBG_PRINTF("[TX b%u] src=0x%02X dst=0x%02X fid=%u ctrl=0x%02X len=%u",
               i, src, dst, fid, ctrl, len);
    print_ctrl_name(fid, ctrl);
    DBG.println();

    // Zweitzeile(n) mit den Werten – nur für READ-ähnliche Frames
    if (len && n >= (size_t)(5 + len)) {
      const uint8_t* d = payload + 5;

      switch (ctrl) {
        // --- FE 0xFE READs ---------------------------------------------------
        case jbc_cmd::FE_02::M_R_SUCTIONLEVEL:
          if (len >= 1) DBG_PRINTF("        ↳ suctionLevel=%u\n", d[0]);
          break;

        case jbc_cmd::FE_02::M_R_SELECTFLOW:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ selectFlow=%u\n", v); }
          break;

        case jbc_cmd::FE_02::M_R_FLOW:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ flow_x1000=%u\n", v); }
          break;

        case jbc_cmd::FE_02::M_R_SPEED:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ speed_rpm=%u\n", v); }
          break;

        case jbc_cmd::FE_02::M_R_INTAKEACTIVATION:
          if (len == 3) {
            DBG_PRINTF("        ↳ intakeAct=%u port=%u intake=%u\n", d[0], d[1], d[2]);
          }
          break;

        case jbc_cmd::FE_02::M_R_SUCTIONDELAY:
          if (len == 4) {
            uint16_t secs = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ delaySec=%u port=%u intake=%u\n", secs, d[2], d[3]);
          }
          break;

        case jbc_cmd::FE_02::M_R_DELAYTIME:
          if (len == 4) {
            uint16_t ms = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ tstopMs=%u port=%u intake=%u\n", ms, d[2], d[3]);
          }
          break;

        case jbc_cmd::FE_02::M_R_ACTIVATIONPEDAL:
          if (len == 2) {
            DBG_PRINTF("        ↳ pedalAct=%u port=%u\n", d[0], d[1]);
          }
          break;

        case jbc_cmd::FE_02::M_R_PEDALMODE:
          if (len == 2) {
            DBG_PRINTF("        ↳ pedalMode=%u port=%u\n", d[0], d[1]);
          }
          break;

        case jbc_cmd::FE_02::M_R_FILTERSTATUS:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ filterLife=%u\n", v); }
          break;

        case jbc_cmd::FE_02::M_R_FILTERSAT:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ filterSaturation=%u\n", v); }
          break;

        case jbc_cmd::FE_02::M_R_CONNECTEDPEDAL:
          if (len == 2) {
            DBG_PRINTF("        ↳ connectedPedal=%u port=%u\n", d[0], d[1]);
          }
          break;

        case jbc_cmd::FE_02::M_R_CONTINUOUSSUCTION:
          if (len >= 1) DBG_PRINTF("        ↳ continuous=%u\n", d[0]);
          break;

        case jbc_cmd::FE_02::M_R_USB_CONNECTSTATUS:
          if (len >= 1) DBG_PRINTF("        ↳ usbConnect=%u\n", d[0]);
          break;

        case jbc_cmd::FE_02::M_R_STATERROR:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8));
            DBG_PRINTF("        ↳ statError=%u\n", v); }
          break;

        // Optional: Firmware/DeviceID-Reads sichtbar machen
        case jbc_cmd::FE_02::M_FIRMWARE:
          if (len >= 1) { DBG.print(F("        ↳ fw=")); print_ascii_quoted(d, len); DBG.println(); }
          break;

        case jbc_cmd::FE_02::M_R_DEVICEID:
          if (len >= 1) { DBG.print(F("        ↳ deviceId=")); print_ascii_quoted(d, len); DBG.println(); }
          break;

        default:
          // andere CTRLs (ACK/NACK/SYN etc.) ohne Detail-Zeile
          break;
      }
    }
  }
#endif
}


static void send_hs_ack_p02(uint8_t i, uint8_t dst_node){
  uint8_t pl[] = { g_bus[i].my_addr, dst_node, FID_HS, jbc_cmd::FE_02::M_HS, 0x01, 0x06 };
  send_p02(i, pl, sizeof(pl));
}
static void send_syn_p02(uint8_t i, uint8_t dst_node, uint8_t fid){
  uint8_t pl[] = { g_bus[i].my_addr, dst_node, fid, CTRL_SYN_P02, 0x01, 0x06 };
  send_p02(i, pl, sizeof(pl));
}

static void send_firmware_fe_as(uint8_t i, uint8_t src_node, uint8_t dst_node, uint8_t mirror_fid /*=0xFE*/){
  const uint8_t len=(uint8_t)(sizeof(FE_FW_STR)-1);
  uint8_t pl[5+64]; size_t o=0;
  pl[o++]=src_node; pl[o++]=dst_node; pl[o++]=mirror_fid; pl[o++]=jbc_cmd::FE_02::M_FIRMWARE; pl[o++]=len;
#if defined(ARDUINO_ARCH_AVR)
  memcpy_P(&pl[o], FE_FW_STR, len);
#else
  memcpy(&pl[o], FE_FW_STR, len);
#endif
  o+=len;
  send_p02(i, pl,o);
}

static void send_deviceid_fe_as(uint8_t i, uint8_t src_node, uint8_t dst_node, uint8_t mirror_fid /*=0xFE*/){
#if FE_SEND_EMPTY_DEVICEID
  uint8_t pl[] = { src_node, dst_node, mirror_fid, jbc_cmd::FE_02::M_R_DEVICEID, 0x00 };
  send_p02(i, pl, sizeof(pl));
#else
  uint8_t len = g_bus[i].dev_id_len; if(len > sizeof(g_bus[i].dev_id)) len = sizeof(g_bus[i].dev_id);
  uint8_t pl[5+64]; size_t o=0;
  pl[o++]=src_node; pl[o++]=dst_node; pl[o++]=mirror_fid; pl[o++]=jbc_cmd::FE_02::M_R_DEVICEID; pl[o++]=len;
  if(len){ memcpy(&pl[o], g_bus[i].dev_id, len); o+=len; }
  send_p02(i, pl, o);
#endif
}

static inline bool is_for_us(uint8_t i, uint8_t dst){
  return (dst==0x00) || (dst==g_bus[i].my_addr);
}
static inline uint8_t reply_src_for(uint8_t i, uint8_t /*req_dst*/){
  return g_bus[i].my_addr;
}

// ===== FE-like READ/WRITE Handler ===========================================
static void send_fe_read_reply_anyfid_as(uint8_t i, uint8_t src_node, uint8_t dst_node, uint8_t fid, uint8_t ctrl, const uint8_t* d, uint8_t n){
  uint8_t pl[5+16]; size_t o=0;
  pl[o++]=src_node; pl[o++]=dst_node; pl[o++]=fid; pl[o++]=ctrl; pl[o++]=n;
  if(n){ memcpy(pl+o,d,n); o+=n; }
  send_p02(i, pl,o);
}

static void handle_fe_like_read(uint8_t i, uint8_t req_dst, uint8_t reply_to, uint8_t fid, uint8_t ctrl, const uint8_t* data, uint8_t len_field){
  using namespace jbc_cmd::FE_02;
  uint8_t out[8]; uint8_t olen=0;
  switch(ctrl){
    case M_R_SUCTIONLEVEL:    { out[0]=st.suctionLevel; olen=1; } break;
    case M_R_FLOW:            { uint16_t v=st.flow_x1000; out[0]=v&0xFF; out[1]=v>>8; olen=2; } break;
    case M_R_SPEED:           { uint16_t v=st.speed_rpm;  out[0]=v&0xFF; out[1]=v>>8; olen=2; } break;
    case M_R_SELECTFLOW:      { uint16_t v=st.selectFlow; out[0]=v&0xFF; out[1]=v>>8; olen=2; } break;
    case M_R_STANDINTAKES:    { out[0]=st.standIntakes; olen=1; } break;
    case M_R_INTAKEACTIVATION:{ uint8_t port=(len_field>=1&&data)?data[0]:0; uint8_t in=(len_field>=2&&data)?data[1]:0; if(port>=MAX_PORTS) port=0; if(in>=NUM_INTAKES) in=0; out[0]=st.intakeAct[port][in]; out[1]=port; out[2]=in; olen=3; } break;
    case M_R_SUCTIONDELAY:    { uint8_t port=(len_field>=1&&data)?data[0]:0; uint8_t in=(len_field>=2&&data)?data[1]:0; if(port>=MAX_PORTS) port=0; if(in>=NUM_INTAKES) in=0; uint16_t v=st.delaySec[port][in]; out[0]=v&0xFF; out[1]=v>>8; out[2]=port; out[3]=in; olen=4; } break;
    case M_R_DELAYTIME:       { uint8_t port=(len_field>=1&&data)?data[0]:0; uint8_t in=(len_field>=2&&data)?data[1]:0; if(port>=MAX_PORTS) port=0; if(in>=NUM_INTAKES) in=0; uint16_t v=st.tstopMs[port][in]; out[0]=v&0xFF; out[1]=v>>8; out[2]=port; out[3]=in; olen=4; } break;
    case M_R_ACTIVATIONPEDAL: { uint8_t port=(len_field>=1&&data)?data[0]:0; if(port>=MAX_PORTS) port=0; out[0]=st.pedalAct[port]; out[1]=port; olen=2; } break;
    case M_R_PEDALMODE:       { uint8_t port=(len_field>=1&&data)?data[0]:0; if(port>=MAX_PORTS) port=0; out[0]=st.pedalMode[port]; out[1]=port; olen=2; } break;
    case M_R_FILTERSTATUS:    { uint16_t v=st.filterLife; out[0]=v&0xFF; out[1]=v>>8; olen=2; } break;
    case M_R_FILTERSAT:       { uint16_t v=st.filterSaturation; out[0]=v&0xFF; out[1]=v>>8; olen=2; } break;
    case M_R_CONNECTEDPEDAL:  { uint8_t port=(len_field>=1&&data)?data[0]:0; if(port>=MAX_PORTS) port=0; out[0]= st.connectedPedal[port]?1:0; out[1]=port; olen=2; } break;
    case M_R_CONTINUOUSSUCTION:{ out[0]= st.continuous?1:0; olen=1; } break;
    case M_R_USB_CONNECTSTATUS:{ out[0]= st.usbConnect?1:0; olen=1; } break;
    case M_R_STATERROR:       { uint16_t v=st.statError; out[0]=v&0xFF; out[1]=v>>8; olen=2; } break;
    default: break;
  }
  if(olen){
    uint8_t src_use=reply_src_for(i, req_dst);
    send_fe_read_reply_anyfid_as(i, src_use, reply_to, fid, ctrl, out, olen);
  }
}

static inline void fe_send_ack_as(uint8_t i, uint8_t dst, uint8_t fid=FID_FE, uint8_t code=0x06){
  uint8_t pl[] = { g_bus[i].my_addr, dst, fid, jbc_cmd::FE_02::M_ACK, 0x01, code };
  send_p02(i, pl, sizeof(pl));
}

static bool handle_fe_like_write(uint8_t i, uint8_t req_dst, uint8_t reply_to, uint8_t fid, uint8_t ctrl, const uint8_t* data, uint8_t len_field){
  using namespace jbc_cmd::FE_02;
  uint8_t src_use=reply_src_for(i, req_dst);
  (void)src_use;

  switch (ctrl) {
    case M_W_SUCTIONLEVEL:
      if (len_field>=1 && data){
        uint8_t v = data[0];
        if (v > 3) v = 3;
        cfg.suctionLevel = v;
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_SELECTFLOW:
      if (len_field>=2 && data){
        uint16_t v = (uint16_t)(data[0] | (data[1]<<8));
        if (v > 1000) v = 1000;
        cfg.selectFlow = v;
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_STANDINTAKES:
      if (len_field>=1 && data){
        uint8_t v = data[0] ? 1 : 0;
        cfg.stand_intakes = v;
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_INTAKEACTIVATION:
      if (len_field>=2 && data){
        uint8_t val  = data[0];
        uint8_t port = data[1];
        uint8_t in   = (len_field>=3 ? data[2] : WORK_IDX);
        if (port<MAX_PORTS && in<NUM_INTAKES){
          uint8_t prev = st.intakeAct[port][in];
          fe_send_ack_as(i, reply_to, fid);

          if (in == WORK_IDX) {
            uint8_t before = g_work_mask;
            if (val) {
              work_set(i, true);
              g_afterrun_owner = -1;
              g_relay_off_deadline = 0;
              relay_set(true);
            } else {
              work_set(i, false);
              if (before && g_work_mask == 0 && !g_relay_off_deadline) {
                g_afterrun_owner = i;
                uint16_t d = st.delaySec[port][WORK_IDX];
                g_relay_off_deadline = millis() + (uint32_t)d * 1000UL;
  #if DBG_SHOW_STATE
                dbg_time_prefix();
                DBG_PRINTF("[RELAY b%u] schedule OFF in %us\n", i, d);
  #endif
              }
            }
          }
          if (prev != val){
            st.intakeAct[port][in] = val;
            mark_state_dirty();
            maybe_dump_state_on_write();
          }
          else {
  #if DBG_LOG_DUP_WRITES
            dbg_time_prefix();
            DBG_PRINTF("[RELAY b%u] duplicate M_W_INTAKEACTIVATION ignored (val=%u port=%u intake=%u)\n",
                      i, val, port, in);
  #endif
          }
          return true;
        }
      }
      break;

    case M_W_SUCTIONDELAY:
      if (len_field>=4 && data){
        uint16_t v = (uint16_t)(data[0] | (data[1]<<8));
        uint8_t  in = data[3];
        if (in == WORK_IDX)      cfg.tstop_work  = v;
        else if (in == STAND_IDX)cfg.tstop_stand = v;
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_ACTIVATIONPEDAL:
      if (len_field>=2 && data){
        uint8_t val = data[0];
        // port ignorieren bei MAX_PORTS=1
        cfg.pedal_act = val;
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_PEDALMODE:
      if (len_field>=2 && data){
        uint8_t mode = data[0];
        cfg.pedal_mode = mode;
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_CONTINUOUSSUCTION:
      if (len_field>=1 && data){
        st.continuous = data[0]?1:0;
        fe_send_ack_as(i, reply_to, fid);
        if (st.continuous){ g_relay_off_deadline=0; relay_set(true); }
        mark_state_dirty(); maybe_dump_state_on_write();
        return true;
      }
      break;

    case M_W_USB_CONNECTSTATUS:
      if (len_field>=1 && data){
        st.usbConnect = data[0]?1:0;
        fe_send_ack_as(i, reply_to, fid);
        mark_state_dirty(); maybe_dump_state_on_write();
        return true;
      }
      break;

    case M_W_DEVICEID:
      if (len_field>=1 && data){
        uint8_t len = len_field;
        if (len>sizeof(g_bus[i].dev_id)) len=sizeof(g_bus[i].dev_id);
        memcpy(g_bus[i].dev_id, data, len);
        g_bus[i].dev_id_len = len;
        persist_save_devid(i);
        fe_send_ack_as(i, reply_to, fid);
  #if (DBG_SHOW_TX || DBG_SHOW_RX || DBG_SHOW_STATE)
        dbg_time_prefix();
        DBG_PRINTF("[ID bus%u] updated len=%u ascii=", i, g_bus[i].dev_id_len);
        print_ascii_quoted(g_bus[i].dev_id, g_bus[i].dev_id_len);
        DBG.println();
  #endif
        return true;
      }
      break;

    default:
      break;
  }
  return false;
}

// ===== Frame-Handler =========================================================
static void handle_frame(uint8_t i, const uint8_t* f, size_t n){
  if(n<4) return;
  const uint8_t src=f[0];
  const uint8_t dst=f[1];
  const uint8_t fid=f[2];
  const uint8_t ctrl=f[3];
  const uint8_t len_field=(n>=5)?f[4]:0;
  const uint8_t* data=(n>=5 && len_field)?(f+5):nullptr;
  const uint8_t reply_to=(src==0x00)?0x00:src;
  bool for_us=is_for_us(i,dst);
  const uint32_t now_ms=millis();

  if(for_us){
  bool was_up = (g_bus[i].linkState == LINK_UP);
    g_bus[i].linkState = LINK_UP;
    g_bus[i].last_for_us_ms = now_ms;
    if(!was_up){
      g_bus[i].t_last_intake_ms = now_ms;  // Timer mit Link-Start setzen
    }
  }


#if DBG_SHOW_RX
  DBG_PRINTF("[RX b%u] src=0x%02X dst=0x%02X fid=%u ctrl=0x%02X", i, src,dst,fid,ctrl); print_ctrl_name(fid,ctrl); DBG.println();
#endif

  // --- neu ---
  if (fid == FID_HS) {
    if(!g_bus[i].addr_locked && dst!=0x00){
      g_bus[i].my_addr = dst;
      persist_save_addr(i, g_bus[i].my_addr);
      g_bus[i].addr_locked=true;
  #if DBG_SHOW_PROTO
      DBG_PRINTF("[ADDR b%u] adopt 0x%02X (locked)\n", i, g_bus[i].my_addr);
  #endif
    }
    if (src != 0x00) g_bus[i].stn_addr = src;
    send_hs_ack_p02(i, reply_to);

    // WICHTIG:
    // - Broadcast-HS (dst==0x00) verändert linkState NICHT.
    // - Nur wenn wir wirklich "down" waren, auf CONNECTING gehen.
    // - Wenn bereits UP, UP beibehalten -> kein LED-Ausblinken.
    if (dst != 0x00) {
      if (g_bus[i].linkState == LINK_DOWN) {
        g_bus[i].linkState = LINK_CONNECTING;
      } else {
        g_bus[i].linkState = LINK_UP;
      }
    }
    return;
  }

  // FE Frames (0xFE) → Firmware/DeviceID handled direkt
  if (fid == FID_FE) {
    if (!for_us) return;
    if (src != 0x00) g_bus[i].stn_addr = src;

    // FW-Loop erkennt *nur* (R/W) IntakeActivation
    if (is_intake_activation_ctrl(ctrl)) {
        intake_touch(i);
    }



    switch (ctrl){
      case jbc_cmd::FE_02::M_FIRMWARE: {
        uint8_t src_use = reply_src_for(i, dst);
        send_firmware_fe_as(i, src_use, reply_to, fid);
        g_bus[i].linkState = LINK_UP;
        return;
      }
      case jbc_cmd::FE_02::M_R_DEVICEID: {
        uint8_t src_use = reply_src_for(i, dst);
        send_deviceid_fe_as(i, src_use, reply_to, fid);
        g_bus[i].linkState = LINK_UP;
        return;
      }
      default:
        break;
    }

    if (handle_fe_like_write(i, dst, reply_to, fid, ctrl, data, len_field)) return;
    handle_fe_like_read(i, dst, reply_to, fid, ctrl, data, len_field);
    return;
  }


  if (for_us && fid != FID_FE) {
    if (is_intake_activation_ctrl(ctrl)) {
      intake_touch(i);
    }
    if (handle_fe_like_write(i, dst, reply_to, fid, ctrl, data, len_field)) return;
    handle_fe_like_read(i, dst, reply_to, fid, ctrl, data, len_field);
    return;
  }
}

// ===== RX: P02-Parser (LEN-basiert, robust) pro Bus ==========================
static void poll_rx_p02(uint8_t i){
  auto &b = g_bus[i];
  while (b.ser->available()){
    uint8_t by = (uint8_t)b.ser->read();
    b.t_last_rx=millis();
    b.t_last_rx_us=micros();

    switch(b.rxState){
      case WAIT_DLE:
        if(by==DLE) b.rxState=WAIT_STX;
        break;
      case WAIT_STX:
        if(by==STX){
          b.rxState=IN_FRAME; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
        } else if(by!=DLE){
          b.rxState=WAIT_DLE;
        }
        break;
      case IN_FRAME:
        if(!b.in_escape){
          if(by==DLE){
            b.in_escape=true;
          } else {
            if(b.payLen < BusCtx::MAX_PAY){
              b.payBuf[b.payLen++]=by;
              if(!b.have_len && b.payLen==5){
                b.data_len=b.payBuf[4]; b.have_len=true;
              }
              if(b.have_len && b.payLen == (size_t)(5 + b.data_len + 1)){
                uint8_t bcc_rx=b.payBuf[b.payLen-1];
                size_t n_no_bcc=5+b.data_len;
                uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
                if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
                else {
#if DBG_SHOW_ERR
                  DBG_PRINTF("[ERR b%u] BCC mismatch rx=%02X calc=%02X\n", i, bcc_rx, bcc_cmp);
#endif
                }
                b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
              }
            } else {
#if DBG_SHOW_ERR
              DBG_PRINTF("[ERR b%u] payload overflow\n", i);
#endif
              b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
            }
          }
        } else { // in_escape
          if(by==DLE){
            if(b.payLen < BusCtx::MAX_PAY){
              b.payBuf[b.payLen++]=DLE;
              if(!b.have_len && b.payLen==5){
                b.data_len=b.payBuf[4]; b.have_len=true;
              }
              if(b.have_len && b.payLen==(size_t)(5+b.data_len+1)){
                uint8_t bcc_rx=b.payBuf[b.payLen-1];
                size_t n_no_bcc=5+b.data_len;
                uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
                if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
                else {
#if DBG_SHOW_ERR
                  DBG_PRINTF("[ERR b%u] BCC mismatch rx=%02X calc=%02X\n", i, bcc_rx, bcc_cmp);
#endif
                }
                b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
              }
            } else {
#if DBG_SHOW_ERR
              DBG_PRINTF("[ERR b%u] payload overflow\n", i);
#endif
              b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
            }
            b.in_escape=false;
          }
          else if(by==ETX){
            if(b.have_len && b.payLen>=(size_t)(5+b.data_len+1)){
              // already complete
            } else if(b.payLen>=6){
              uint8_t bcc_rx=b.payBuf[b.payLen-1];
              size_t n_no_bcc=b.payLen-1;
              uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
              if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
            }
            b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
          }
          else if(by==STX){
            if(!(b.have_len && b.payLen==(size_t)(5+b.data_len+1))){
              if(b.payLen>=6){
                uint8_t bcc_rx=b.payBuf[b.payLen-1];
                size_t n_no_bcc=b.payLen-1;
                uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
                if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
              }
            }
            b.rxState=IN_FRAME; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
          }
          else {
#if DBG_SHOW_ERR
            DBG_PRINTF("[ERR b%u] invalid DLE-sequence byte=0x%02X\n", i, by);
#endif
            b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
          }
        }
        break;
    }
  }
}

// ===== RX: BASE-Layer (vor P02) pro Bus =====================================
static void poll_rx_base(uint8_t i){
  auto &b=g_bus[i];
  while(b.ser->available()){
    uint8_t by=(uint8_t)b.ser->read();
    b.t_last_rx=millis(); b.t_last_rx_us=micros();
    if(by==DLE){
      b.baseState=BASE_P02_ACTIVE; b.rxState=WAIT_STX;
#if DBG_SHOW_PROTO
      DBG_PRINTF("[BASE b%u] DLE -> P02\n", i);
#endif
      return;
    }
    switch(b.baseState){
      case BASE_IDLE:
        if(by==jbc_cmd::BASE::M_NACK){ b.baseState=BASE_SEEN_NAK;
#if DBG_SHOW_PROTO
          DBG_PRINTF("[BASE b%u] RX NAK\n", i);
#endif
        }
        break;
      case BASE_SEEN_NAK:
        if(by==jbc_cmd::BASE::M_NACK){ /* still base */ }
        send_base_syn(i); b.baseState=BASE_SENT_SYN;
        break;
      case BASE_SENT_SYN:
        if(by==jbc_cmd::BASE::M_ACK){ b.baseState=BASE_GOT_ACK1;
#if DBG_SHOW_PROTO
          DBG_PRINTF("[BASE b%u] RX ACK1\n", i);
#endif
        }
        break;
      case BASE_GOT_ACK1:
        send_base_ack(i, F("(after ACK1)"));
        b.baseState=BASE_SENT_ACK2;
        break;
      case BASE_SENT_ACK2:
        if(by==0x01 /*SOH*/){ b.baseState=BASE_GOT_SOH;
#if DBG_SHOW_PROTO
          DBG_PRINTF("[BASE b%u] RX SOH\n", i);
#endif
        }
        break;
      case BASE_GOT_SOH:
        send_base_ack(i, F("(after SOH)"));
        b.baseState=BASE_P02_ACTIVE; b.rxState=WAIT_DLE; b.linkState=LINK_CONNECTING;
#if DBG_SHOW_PROTO
        DBG_PRINTF("[BASE b%u] -> P02 mode\n", i);
#endif
        return;
      case BASE_P02_ACTIVE:  return;
    }
  }
}

// ===== LEDs (pro Bus 2 Stück) ===============================================
#if LED_ENABLE
static inline bool led_safe_to_show(){
  uint32_t now_us = micros();

  // nur die aktuell aktiven Busse prüfen
  for (uint8_t i = 0; i < g_bus_count; i++){
    // Wenn gerade UART-Daten pending sind → nicht rendern
    if (g_bus[i].ser && g_bus[i].ser->available() > 0) return false;
    // Nach letztem RX noch Mindestabstand einhalten
    if ((uint32_t)(now_us - g_bus[i].t_last_rx_us) < LED_SHOW_MIN_GAP_US) return false;
  }
  return true;
}

static inline void leds_begin(){
  pixels.begin(); pixels.clear(); pixels.show();
}
static inline void set_led_idx(uint8_t idx, uint8_t r,uint8_t g,uint8_t b,uint8_t w){
  if(idx<LED_COUNT_MAX) pixels.setPixelColor(idx, pixels.Color(r,g,b,w));
}


static void leds_render_bus(uint8_t i){
  const uint32_t now=millis();
  uint8_t linkIdx = i*2; uint8_t statIdx = i*2+1;
  // Link LED
  if(fwloop_active(i)){
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2)==0;
    if(on) set_led_idx(linkIdx, LED_BRIGHTNESS_MAX, LED_BRIGHTNESS_MAX, 0, 0);
    else set_led_idx(linkIdx,0,0,0,0);
  }
  else {
    switch(g_bus[i].linkState){
      case LINK_DOWN:{
        uint8_t lv=breath_level(LED_BREATH_PERIOD_MS,2,LED_BRIGHTNESS_MAX);
        set_led_idx(linkIdx, lv,0,0,0);
      } break;
      case LINK_CONNECTING:{
        bool on=((now/(LED_BLINK_PERIOD_MS/2))%2)==0;
        if(on) set_led_idx(linkIdx, LED_BRIGHTNESS_MAX, LED_BRIGHTNESS_MAX,0,0);
        else set_led_idx(linkIdx,0,0,0,0);
      } break;
      case LINK_UP:{
        set_led_idx(linkIdx,0,LED_BRIGHTNESS_MAX,0,0);
      } break;
    }
  }
  // Bei Disconnect/Connecting: Status-LED aus
  if (g_bus[i].linkState != LINK_UP){
    set_led_idx(statIdx, 0, 0, 0, 0);
    return;
  }


  // NEUE Priorität: 1) Relais durch DIESEN Bus aktiv (blau)
  //                 2) Nachlauf dieses Busses (türkis)
  //                 3) STOP/WARN (rot/gelb blinkend)
  //                 4) Standby (weiß "atmen")
  const bool active      = bus_active(i);
  const bool afterrun_me = (g_relay_off_deadline != 0) && (g_afterrun_owner == (int8_t)i);

  uint8_t lv = breath_level(LED_BREATH_PERIOD_MS, 2, LED_BRIGHTNESS_MAX);

  // 1) Relais aktiv (dieser Bus hat WORK an)
  if (active) {
    set_led_idx(statIdx, 0, 0, lv, 0);  // Blau
    return;
  }

  // 2) Nachlauf (nur Owner zeigt ihn an) — blinkend lila, in der dunklen Phase weißes "breathing"
  if (afterrun_me) {
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2) == 0;
    uint8_t v = on ? LED_BRIGHTNESS_MAX : 0;
    if (on) {
      // Lila hell
      set_led_idx(statIdx, v, 0, v, 0);
    } else {
      // In der "dunklen" Phase Weiß atmen (lv ist breath_level)
      set_led_idx(statIdx, 0, 0, 0, 0);
    }
    return;
  }
  // 3) Fehler nur anzeigen, wenn nicht aktiv / kein Nachlauf
  const bool stopErr = has_stop_error(st.statError);
  const bool warnErr = has_warn_error(st.statError);
  if (stopErr) {
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2) == 0;
    set_led_idx(statIdx, on ? LED_BRIGHTNESS_MAX : 0, 0, 0, 0); // Rot blinkend
    return;
  }
  if (warnErr) {
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2) == 0;
    uint8_t v = on ? LED_BRIGHTNESS_MAX : 0;
    set_led_idx(statIdx, v, v, 0, 0); // Gelb blinkend
    return;
  }

  // 4) Standby
  set_led_idx(statIdx, 0, 0, 0, lv);   // Weiß "breathing"
}
static inline void leds_task(){
  uint32_t now=millis();
  if(now - t_led_last_render < 20) return; // ~50 Hz
  t_led_last_render=now;
  for(uint8_t i=0;i<g_bus_count;i++) leds_render_bus(i);
  // LEDs hinter den aktiven Bussen zuverlässig aus
  for(uint8_t idx = g_bus_count*2; idx < LED_COUNT_MAX; ++idx) set_led_idx(idx,0,0,0,0);
  if(led_safe_to_show()) pixels.show();
}
#endif

// ===== Setup / Loop ==========================================================
static void dump_config_defaults(){
#if DBG_SHOW_STATE
  DBG.print(F("[CFG] defaults: "));
  DBG.print(F("buses=")); DBG.print(g_bus_count);
  DBG.print(F(" | tstop(w/s)=")); DBG.print(DEF_TSTOP_WORK); DBG.print('/'); DBG.print(DEF_TSTOP_STAND);
  DBG.print(F(" | flow_x1000=")); DBG.print(DEF_FLOW_X_MIL);
  DBG.print(F(" | speed_rpm="));  DBG.print(DEF_SPEED_RPM);
  DBG.print(F(" | pedal_act="));  DBG.print(DEF_PEDAL_ACT);
  DBG.print(F(" | pedal_mode=")); DBG.print(DEF_PEDAL_MODE);
  DBG.print(F(" | stand_intakes=")); DBG.print(DEF_STAND_INTAKES);
  DBG.print(F(" | filter(life/sat)=")); DBG.print(DEF_FILTER_LIFE); DBG.print('/'); DBG.print(DEF_FILTER_SAT);
  DBG.print(F(" | stat_error=")); DBG.print(DEF_STAT_ERROR);
  DBG.println();
#endif
}


void setup(){
  for(uint8_t p=0;p<MAX_PORTS;p++){
    st.pedalAct[p]=DEF_PEDAL_ACT; st.pedalMode[p]=DEF_PEDAL_MODE; st.connectedPedal[p]=1;
    for(uint8_t i=0;i<NUM_INTAKES;i++){
      st.intakeAct[p][i]=DEF_ACTIVATION;
      st.delaySec[p][i]=(i==WORK_IDX)?DEF_TSTOP_WORK:DEF_TSTOP_STAND;
      st.tstopMs[p][i]=0;
    }
  }
  DBG.begin(115200);

  #ifndef SERIAL_STARTUP_WAIT_MS
  #define SERIAL_STARTUP_WAIT_MS 1200  // ~1.2 s reichen i.d.R.
  #endif

  #if defined(ARDUINO_USB_CDC_ON_BOOT)
  // Bei ESP32-S2/S3 (native USB): kurz auf CDC warten (aber nicht ewig blocken)
  uint32_t t0 = millis();
  while (!DBG && (millis() - t0) < SERIAL_STARTUP_WAIT_MS) { delay(10); }
  #else
  // Klassischer ESP32 mit USB-UART: feste kurze Pause
  delay(SERIAL_STARTUP_WAIT_MS);
  #endif


  persist_begin();
  deviceid_init_defaults();
  g_bus_count = persist_load_buscount();
  if (g_bus_count < 1 || g_bus_count > MAX_BUSES) g_bus_count = BUS_COUNT;

  // Versuche, pro Bus aus Persistenz zu laden
  for(uint8_t i=0;i<MAX_BUSES;i++){
    (void)persist_load_devid(i);
    uint8_t a=0; if(persist_load_addr(i,a)) g_bus[i].my_addr=a;
  }

  print_startup_banner();     // zeigt [NVS]/[EEP], Busse, [CLI], [CFG], [FW]
  //print_prompt();  
 


  // Runtime-Config Defaults
  cfg.tstop_work    = DEF_TSTOP_WORK;
  cfg.tstop_stand   = DEF_TSTOP_STAND;
  cfg.suctionLevel  = DEF_SUCTIONLEVEL;
  cfg.selectFlow    = DEF_SELECTFLOW_X_M;
  cfg.pedal_act     = DEF_PEDAL_ACT;
  cfg.pedal_mode    = DEF_PEDAL_MODE;
  cfg.stand_intakes = DEF_STAND_INTAKES;
  cfg.filter_life   = DEF_FILTER_LIFE;
  cfg.filter_sat    = DEF_FILTER_SAT;
  cfg.stat_error    = DEF_STAT_ERROR;
  if (persist_load_cfg()) apply_cfg_to_state(); else apply_cfg_to_state();

 
  relay_init();
#if LED_ENABLE
  leds_begin();
#endif
#if LED_ENABLE
for(uint8_t i=0;i<g_bus_count;i++){
  g_bus[i].t_last_intake_ms = millis();
}

for(uint8_t idx=g_bus_count*2; idx<LED_COUNT_MAX; ++idx) set_led_idx(idx,0,0,0,0);
pixels.show();
#endif
  // UARTs starten
  for(uint8_t i=0;i<g_bus_count;i++) JBC_BEGIN(i);
#if DBG_SHOW_PROTO
  DBG.println(F("\n[JBC FAE Emulator – Base-Link + P02 (SRC,DST), Dual-Bus]"));
  for(uint8_t i=0;i<BUS_COUNT;i++) DBG_PRINTF("[UART bus%u] @ 250000, 8E1\n", i);
#endif
}

void loop(){
  for(uint8_t i=0;i<g_bus_count;i++){
    if(g_bus[i].baseState!=BASE_P02_ACTIVE) poll_rx_base(i); else poll_rx_p02(i);
  }


#if DBG_SHOW_STATE && !DBG_STATE_ON_WRITE_ONLY
  if(DBG_STATE_PERIOD_MS > 0){
    const uint32_t now = millis();
    if(now - t_last_state >= DBG_STATE_PERIOD_MS){ dump_state(); t_last_state = now; }
  }
#endif

  cli_poll();

  // ---- Relais-Laufzeitlogik -------------------------------------------------
  if (st.continuous) {
    if (!g_relay_on) relay_set(true);
    g_afterrun_owner = -1;            // NEU
    g_relay_off_deadline = 0;
  } else {
    if (any_work_active()) {
      if (!g_relay_on) relay_set(true);
      if (g_relay_off_deadline) g_relay_off_deadline=0;
      g_afterrun_owner = -1;          // NEU: kein Nachlauf, weil wieder aktiv
    } else {
      if (g_relay_off_deadline) {
        if ((int32_t)(millis() - g_relay_off_deadline) >= 0) {
          relay_set(false);
          #if DBG_SHOW_STATE
            dbg_time_prefix(); DBG.println(F("[RELAY] afterrun expired"));
          #endif
          g_relay_off_deadline=0;
          g_afterrun_owner = -1;      // NEU: Nachlauf ist vorbei
        }
      } else {
        if (g_relay_on) relay_set(false);
      }
    }
  }


#if LED_ENABLE
  leds_task();
#endif
#if AUTO_SAVE_CFG_ON_JBC_WRITES
  if (cfg_dirty && (int32_t)(millis() - (int32_t)cfg_save_deadline) >= 0) {
    persist_save_cfg();
    cfg_dirty = false;
  #if DBG_SHOW_STATE
    dbg_time_prefix(); DBG.println(F("[CFG] auto-saved"));
  #endif
  }
#endif

  // ---- Link-Timeout je Bus --------------------------------------------------
  const uint32_t now=millis();
  for(uint8_t i=0;i<g_bus_count;i++){
    if(now - g_bus[i].t_last_rx > 1000){
#if DBG_SHOW_PROTO
      if(g_bus[i].baseState==BASE_P02_ACTIVE) DBG_PRINTF("[LINK b%u] timeout → BASE reset\n", i);
      

#endif
      work_set(i, false);
      if (g_afterrun_owner == (int8_t)i) g_afterrun_owner = -1;
      g_bus[i].baseState=BASE_IDLE; g_bus[i].linkState=LINK_DOWN; g_bus[i].rxState=WAIT_DLE; g_bus[i].payLen=0; g_bus[i].t_last_rx=now; g_bus[i].t_last_rx_us=micros(); g_bus[i].addr_locked=false;
    }
  }
}
