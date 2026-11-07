// SPDX-License-Identifier: MIT OR GPL-2.0-only
/*
  JBC FAE Emulator – Dual-Bus P02 / Base-Link Emulator with ESP32 Web Portal
  JBC FAE Emulator – Dual-Bus P02 / Base-Link Emulator mit ESP32-Webportal

  Features / Funktionen:
  - Emulation of a JBC FAE on 1 or 2 separate JBC buses
  - Emulation eines JBC FAE auf 1 oder 2 getrennten JBC-Bussen

  - Base-Link handling (NAK / SYN / ACK / SOH)
  - Base-Link-Verarbeitung (NAK / SYN / ACK / SOH)

  - P02 frame handling with mirrored FID routing
  - P02-Frame-Verarbeitung mit gespiegeltem FID-Routing

  - Shared logical FAE state for both buses
  - Gemeinsamer logischer FAE-Zustand für beide Busse

  - ESP32 web configuration portal (AP mode if no WiFi is configured)
  - ESP32-Web-Konfigurationsportal (AP-Modus, wenn kein WLAN konfiguriert ist)

  - Optional STA mode, web status page and MQTT support
  - Optionaler STA-Modus, Web-Statusseite und MQTT-Unterstützung

  Model / Modell:
  - 2 separate transport/address endpoints
  - 2 getrennte Transport-/Adress-Endpunkte

  - 1 shared logical FAE device state
  - 1 gemeinsamer logischer FAE-Gerätezustand

  Main functions in this version / Hauptfunktionen in dieser Version:
  - automatic address adoption via CTRL M_HS
  - automatische Adressübernahme über CTRL M_HS

  - DeviceID persistence per bus
  - DeviceID-Persistenz pro Bus

  - configuration persistence in NVS / EEPROM
  - Konfigurationsspeicherung in NVS / EEPROM

  - relay control with afterrun timer
  - Relaissteuerung mit Nachlaufzeit

  - LED status indication per bus
  - LED-Statusanzeige pro Bus

  - web-based network setup directly on the ESP32
  - webbasierte Netzwerkkonfiguration direkt auf dem ESP32

  - optional MQTT state publishing and Home Assistant discovery
  - optionale MQTT-Statusveröffentlichung und Home-Assistant-Discovery

  Routing concept / Routing-Konzept:
  - no hard routing via fixed FID_HS / FID_FE
  - kein fest verdrahtetes Routing über feste FID_HS / FID_FE

  - special handling is based on CTRL commands
  - Spezialbehandlung basiert auf CTRL-Befehlen

  - incoming FIDs are mirrored in replies
  - eingehende FIDs werden in Antworten gespiegelt

  - address / device logging shows source and destination names directly
  - Adress-/Gerätelogging zeigt Quell- und Zielnamen direkt an

  Notes / Hinweise:
  - if no valid WiFi configuration exists, the ESP starts its own setup AP
  - wenn keine gültige WLAN-Konfiguration vorhanden ist, startet der ESP einen eigenen Setup-AP

  - the web portal can be used to configure WiFi, IP and MQTT settings
  - über das Webportal können WLAN-, IP- und MQTT-Einstellungen konfiguriert werden
*/

#include <Arduino.h>
#include <string.h>
#include "jbc_FE_commands_full.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_mac.h>
#include <Update.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <esp_timer.h>
#include <esp_freertos_hooks.h>

#define PROGMEM_STR const char

typedef const char* flashstr_t;
#define FS_LIT(lit) (lit)
#ifndef USER_FW_VERSION
#define USER_FW_VERSION "V2.3.67RC"
#endif


#ifndef WIFI_ENABLE
#define WIFI_ENABLE 1
#endif
#ifndef HTTP_ENABLE
#define HTTP_ENABLE 1
#endif
#ifndef MQTT_ENABLE
#define MQTT_ENABLE 1
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME_PREFIX "JBC-FAE-Emulator-esp32"
#endif
#ifndef HTTP_PORT
#define HTTP_PORT 80
#endif
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "jbc-fae-emulator"
#endif
#ifndef MQTT_DISCOVERY_PREFIX
#define MQTT_DISCOVERY_PREFIX "homeassistant"
#endif
#ifndef MQTT_BASE_TOPIC
#define MQTT_BASE_TOPIC "jbc_fae"
#endif
#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "JBC FAE Emulator"
#endif
#ifndef MQTT_PUBLISH_INTERVAL_MS
#define MQTT_PUBLISH_INTERVAL_MS 5000UL
#endif
#ifndef WIFI_RETRY_MS
#define WIFI_RETRY_MS 10000UL
#endif
#ifndef WIFI_PORTAL_FALLBACK_MS
#define WIFI_PORTAL_FALLBACK_MS 15000UL
#endif
#ifndef WIFI_CONFIG_AP_SSID_PREFIX
#define WIFI_CONFIG_AP_SSID_PREFIX "JBC-FAE-Setup"
#endif
#ifndef WIFI_CONFIG_AP_PASSWORD
#define WIFI_CONFIG_AP_PASSWORD "jbcfae123"
#endif
#ifndef WIFI_CONFIG_AP_CHANNEL
#define WIFI_CONFIG_AP_CHANNEL 6
#endif
#ifndef MQTT_RETRY_MS
#define MQTT_RETRY_MS 5000UL
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
#ifndef DBG_LOG_DUP_WRITES
#define DBG_LOG_DUP_WRITES 0   // 0 = keine "duplicate ... ignored" Meldungen
#endif
#ifndef LOG_ICONS
#define LOG_ICONS 1
#endif
#ifndef DBG_TS_MODE
// 0=aus, 1=ms seit Boot, 2=Sekunden mit 1 Nachkommastelle
#define DBG_TS_MODE 0
#endif
#ifndef DBG_RXTX_MIN_INTERVAL_MS
// Mindestabstand zwischen RX/TX-Logzeilen pro Bus und Richtung; 0 = aus
#define DBG_RXTX_MIN_INTERVAL_MS 0
#endif
#ifndef DBG_CTRL_REPEAT_SLOTS
// Anzahl unterschiedlicher Control-Signaturen (ctrl+payload) pro Bus/Richtung, die parallel auf Wiederholungen geprüft werden.
#define DBG_CTRL_REPEAT_SLOTS 40
#endif
#ifndef BANNER_HOTKEY1
#define BANNER_HOTKEY1 '?'
#endif
#ifndef BANNER_HOTKEY2
#define BANNER_HOTKEY2 0x1B
#endif
#ifndef BANNER_HOTKEY3
#define BANNER_HOTKEY3 0x02
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

struct DbgRateState {
  uint32_t last_ms = 0;
  uint32_t suppressed = 0;
};

struct DbgCtrlRepeatState {
  bool valid = false;
  uint8_t src = 0;
  uint8_t dst = 0;
  uint8_t ctrl = 0;
  uint8_t len = 0;
  uint8_t last_fid = 0;
  uint32_t key_hash = 0;
  uint32_t last_payload_hash = 0;
  uint32_t suppressed = 0;
  uint32_t last_seen_ms = 0;
};

static inline uint32_t dbg_payload_hash(uint8_t len_field, const uint8_t* data){
  uint32_t h = 2166136261u; // FNV-1a
  h ^= len_field; h *= 16777619u;
  for (uint8_t i = 0; i < len_field; i++) {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}
static inline uint32_t dbg_key_hash_bytes(const uint8_t* data, uint8_t len_field){
  return dbg_payload_hash(len_field, data);
}

static inline uint32_t dbg_ctrl_repeat_key_hash(bool is_tx, uint8_t ctrl, uint8_t len_field, const uint8_t* data){
  using namespace jbc_cmd::FE_02;

  if (!is_tx) {
    return dbg_key_hash_bytes(data, len_field);
  }

  switch (ctrl) {
    case M_R_SUCTIONDELAY:
    case M_R_DELAYTIME:
      if (data && len_field >= 4) return dbg_key_hash_bytes(data + 2, 2); // port/intake selector
      break;

    case M_R_INTAKEACTIVATION:
      if (data && len_field >= 3) return dbg_key_hash_bytes(data + 1, 2); // port/intake selector
      break;

    default:
      break;
  }

  // Für die meisten TX-Antworten gibt es genau einen letzten Zustand pro Command.
  return 0;
}

static DbgRateState g_dbg_rx_rate[MAX_BUSES];
static DbgRateState g_dbg_tx_rate[MAX_BUSES];
static DbgCtrlRepeatState g_dbg_rx_repeat[MAX_BUSES][DBG_CTRL_REPEAT_SLOTS];
static DbgCtrlRepeatState g_dbg_tx_repeat[MAX_BUSES][DBG_CTRL_REPEAT_SLOTS];

static inline void dbg_reset_runtime_filters(){
  memset(g_dbg_rx_rate, 0, sizeof(g_dbg_rx_rate));
  memset(g_dbg_tx_rate, 0, sizeof(g_dbg_tx_rate));
  memset(g_dbg_rx_repeat, 0, sizeof(g_dbg_rx_repeat));
  memset(g_dbg_tx_repeat, 0, sizeof(g_dbg_tx_repeat));
}

// ===== Runtime-Debugflags (per CLI schaltbar) ================================
static bool     dbg_show_rx             = (DBG_SHOW_RX != 0);
static bool     dbg_show_tx             = (DBG_SHOW_TX != 0);
static bool     dbg_show_proto          = (DBG_SHOW_PROTO != 0);
static bool     dbg_show_err            = (DBG_SHOW_ERR != 0);
static bool     dbg_show_state          = (DBG_SHOW_STATE != 0);
static bool     dbg_show_intake_poll    = true;
static bool     dbg_state_on_write_only = (DBG_STATE_ON_WRITE_ONLY != 0);
static uint32_t dbg_state_period_ms     = DBG_STATE_PERIOD_MS;
static uint8_t  dbg_ts_mode             = DBG_TS_MODE;
static uint32_t dbg_rxtx_min_interval_ms = DBG_RXTX_MIN_INTERVAL_MS;


// ===== Relais (Ausgang) =====================================================
#ifndef RELAY_ENABLE
  #define RELAY_ENABLE 1
#endif

#ifndef RELAY_PIN
  #define RELAY_PIN 26       // ESP32-Default
#endif

#ifndef RELAY_ACTIVE_LOW
  #define RELAY_ACTIVE_LOW 0   // 0: HIGH=an, 1: LOW=an
#endif

// ===== SK6812 Status-LEDs (GRBW) ============================================
#ifndef LED_ENABLE
#define LED_ENABLE 1
#endif
#ifndef LED_BRIGHTNESS_MAX
#define LED_BRIGHTNESS_MAX 200
#endif

#if LED_ENABLE
  #ifndef LED_PIN
  #define LED_PIN 21              // ESP32-Default
  #endif
  #ifndef LED_COUNT_MAX
  #define LED_COUNT_MAX (MAX_BUSES*2)
  #endif
  #ifndef LED_BREATH_PERIOD_MS
  #define LED_BREATH_PERIOD_MS 5000
  #endif
  #ifndef LED_BLINK_PERIOD_MS
  #define LED_BLINK_PERIOD_MS 1000
  #endif
  #ifndef LED_SHOW_MIN_GAP_US
    #define LED_SHOW_MIN_GAP_US 1500UL
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
// Wire order: protocol:family:software:hardware:detailed model.
// FAE1 firmware 8886792 also uses the shared F2 protocol family.
#define FE_FAMILY "F2"
#define FE_MODEL  "F1_DIS_02"
#define FE_SW7    "8886792"
#define FE_HW7    "0023330"

static PROGMEM_STR FE_FW_STR[] = "02:" FE_FAMILY ":" FE_SW7 ":" FE_HW7 ":" FE_MODEL;
static_assert(sizeof(FE_SW7) - 1 == 7, "Software ID must be 7 ASCII characters");
static_assert(sizeof(FE_HW7) - 1 == 7, "Hardware ID must be 7 ASCII characters");
static_assert(sizeof(FE_FW_STR) - 1 <= 64, "FW string too long");

static PROGMEM_STR FE_DEVICE_ID_STR0[] = "849158E467A340159646170D6B1595EF";
static PROGMEM_STR FE_DEVICE_ID_STR1[] = "849158E467A340159646170D6B1596EF";
static_assert(sizeof(FE_DEVICE_ID_STR0) - 1 == 32, "DeviceID0 must be 32 ASCII");
static_assert(sizeof(FE_DEVICE_ID_STR1) - 1 == 32, "DeviceID1 must be 32 ASCII");

#ifndef FE_SEND_EMPTY_DEVICEID
  #define FE_SEND_EMPTY_DEVICEID 0
#endif

#ifndef LED_FW_IDLE_MS
#define LED_FW_IDLE_MS 2000
#endif
#ifndef MIRROR_WRITES_TO_CFG
#define MIRROR_WRITES_TO_CFG 1
#endif
#ifndef AUTO_SAVE_CFG_ON_JBC_WRITES
#define AUTO_SAVE_CFG_ON_JBC_WRITES 1
#endif
#ifndef CFG_SAVE_DEBOUNCE_MS
#define CFG_SAVE_DEBOUNCE_MS 1500
#endif

// ===== FE-State (gemeinsam für beide Busse) ==================================
#define MAX_PORTS 1
#define NUM_INTAKES 2
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

#define CLAMP(v, lo, hi) (( (v) < (lo) ) ? (lo) : ( ((v) > (hi)) ? (hi) : (v) ))

#define CL_TSTOP_WORK_MIN      0
#define CL_TSTOP_WORK_MAX      300
#define CL_TSTOP_STAND_MIN     0
#define CL_TSTOP_STAND_MAX     999
#define CL_SUCTIONLEVEL_MIN    0
#define CL_SUCTIONLEVEL_MAX    3
#define CL_SELECTFLOW_MIN      0
#define CL_SELECTFLOW_MAX      1000
#define CL_ACTUAL_FLOW_MIN     0
#define CL_ACTUAL_FLOW_MAX     1000
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

static inline uint16_t filter_pct_to_raw(uint16_t pct){
  if (pct > 100) pct = 100;
  return (uint16_t)(pct * 10U);
}
static inline uint16_t filter_raw_to_pct(uint16_t raw){
  if (raw > CL_FILTER_MAX) raw = CL_FILTER_MAX;
  return (uint16_t)((raw + 5U) / 10U);
}

static inline uint16_t selectflow_pct_to_raw(uint16_t pct){
  if (pct > 100) pct = 100;
  return (uint16_t)(pct * 10U);
}
static inline uint16_t selectflow_raw_to_pct(uint16_t raw){
  if (raw > CL_SELECTFLOW_MAX) raw = CL_SELECTFLOW_MAX;
  return (uint16_t)((raw + 5U) / 10U);
}

static inline uint16_t actualflow_pct_to_raw(uint16_t pct){
  if (pct > 100) pct = 100;
  return (uint16_t)(pct * 10U);
}
static inline uint16_t actualflow_raw_to_pct(uint16_t raw){
  if (raw > CL_ACTUAL_FLOW_MAX) raw = CL_ACTUAL_FLOW_MAX;
  return (uint16_t)((raw + 5U) / 10U);
}

static inline uint8_t led_brightness_pct_to_raw(uint16_t pct){
  if (pct < 10U) pct = 10U;
  if (pct > 100U) pct = 100U;
  return (uint8_t)(50U + (((pct - 10U) * 205U) + 45U) / 90U);
}
static inline uint8_t led_brightness_raw_to_pct(uint16_t raw){
  if (raw < 50U) raw = 50U;
  if (raw > 255U) raw = 255U;
  return (uint8_t)(10U + (((raw - 50U) * 90U) + 102U) / 205U);
}

struct {
  uint8_t  suctionLevel = DEF_SUCTIONLEVEL;
  uint16_t flow_x1000   = DEF_FLOW_X_MIL;
  uint16_t speed_rpm    = DEF_SPEED_RPM;
  uint16_t selectFlow   = DEF_SELECTFLOW_X_M;
  uint8_t  standIntakes = DEF_STAND_INTAKES;

  uint8_t  intakeAct[MAX_PORTS][NUM_INTAKES];
  uint16_t delaySec[MAX_PORTS][NUM_INTAKES];
  uint16_t tstopMs [MAX_PORTS][NUM_INTAKES];

  uint8_t  pedalAct[MAX_PORTS];
  uint8_t  pedalMode[MAX_PORTS];

  uint16_t filterLife       = DEF_FILTER_LIFE;
  uint16_t filterSaturation = DEF_FILTER_SAT;
  uint16_t statError        = DEF_STAT_ERROR;

  uint8_t  usbConnect = 1;
  uint8_t  continuous = 0;
  uint8_t  connectedPedal[MAX_PORTS];
} st;

// Bus-lokaler IntakeActivation-Zustand.
// Wichtig: Die Stationssicht auf M_R/M_W_INTAKEACTIVATION ist pro Bus getrennt,
// damit sich DDE und JTSE nicht gegenseitig ihren Sollzustand überschreiben.
// Das Relais bleibt trotzdem global und wird weiterhin über g_work_mask / Owner
// / Afterrun arbitriert.
static uint8_t g_bus_intakeAct[MAX_BUSES][MAX_PORTS][NUM_INTAKES];

// ===== Protokoll Konstanten ==================================================
static const uint8_t DLE = 0x10, STX = 0x02, ETX = 0x03;
static const uint8_t CTRL_SYN_P02 = 0x16;

// ===== Bus-Kontext ===========================================================
enum LinkState { LINK_DOWN, LINK_CONNECTING, LINK_UP };
enum BaseState { BASE_IDLE, BASE_SEEN_NAK, BASE_SENT_SYN, BASE_GOT_ACK1, BASE_SENT_ACK2, BASE_GOT_SOH, BASE_P02_ACTIVE };
enum RxState   { WAIT_DLE, WAIT_STX, IN_FRAME };

struct BusCtx {
  HardwareSerial* ser = nullptr;

  int rxPin = -1, txPin = -1;
  uint8_t my_addr  = 0x91;
  uint8_t stn_addr = 0x00;
  bool    addr_locked = false;

  LinkState linkState = LINK_DOWN;
  BaseState baseState = BASE_IDLE;
  RxState   rxState   = WAIT_DLE;

  // 5 header + 255 payload + 1 BCC = 261 max unstuffed bytes
  static const size_t MAX_PAY = 261;
  uint8_t payBuf[MAX_PAY];
  size_t payLen = 0;
  bool in_escape = false, have_len = false;
  uint8_t data_len = 0;

  uint32_t t_last_rx = 0, t_last_syn = 0, t_last_hb = 0, t_last_rx_us = 0;
  uint32_t last_for_us_ms = 0;
  uint32_t t_last_intake_ms = 0;

  uint8_t  dev_id[64];
  uint8_t  dev_id_len = 0;
  uint8_t  persist_devid_status = 0;
  uint8_t  persist_addr_status  = 0;
};

static BusCtx g_bus[MAX_BUSES];

struct NetCfg {
  char wifi_ssid[33];
  char wifi_password[65];
  char ap_password[65];
  char wifi_hostname[33];
  char mqtt_host[65];
  uint16_t mqtt_port;
  char mqtt_username[33];
  char mqtt_password[65];
  uint8_t mqtt_enable;
  uint8_t mqtt_tls_enable;
  uint8_t mqtt_tls_insecure;
  char mqtt_ca_cert[3072];
  char web_username[33];
  char web_password[65];
  uint8_t use_dhcp;
  char static_ip[16];
  char static_gw[16];
  char static_mask[16];
  char static_dns1[16];
  char static_dns2[16];
};
static NetCfg   g_netcfg;
static WiFiClient g_wifi_client;
static WiFiClientSecure g_wifi_client_tls;
#if MQTT_ENABLE
static PubSubClient g_mqtt(g_wifi_client);
static bool     g_mqtt_tls_ready = false;
static uint32_t g_mqtt_last_cfg_warn_ms = 0;
static bool     g_mqtt_discovery_sent = false;
static uint32_t g_mqtt_last_attempt_ms = 0;
static uint32_t g_mqtt_last_publish_ms = 0;
static uint32_t g_mqtt_last_state_hash = 0;
static bool     g_mqtt_force_publish = true;
static char     g_mqtt_device_id[32] = {0};
static char     g_mqtt_base_topic[96] = {0};
#endif
#if HTTP_ENABLE
static WebServer g_http(HTTP_PORT);
static bool      g_http_started = false;
static DNSServer g_dns;
static bool      g_dns_started = false;
#endif
static bool      g_cfg_portal_active = false;
static bool      g_wifi_sta_connecting = false;
static uint32_t  g_wifi_connect_started_ms = 0;
static uint32_t  g_wifi_last_attempt_ms = 0;
static uint32_t  g_net_restart_deadline_ms = 0;
static char      g_cfg_ap_ssid[40] = {0};
static char      g_cfg_wifi_hostname_default[33] = {0};
static volatile uint32_t g_cpu_idle_counter[2] = {0, 0};
static uint32_t g_cpu_idle_last_sample[2] = {0, 0};
static uint32_t g_cpu_idle_baseline_rate[2] = {0, 0};
static uint32_t g_cpu_load_core_pct[2] = {0, 0};
static uint32_t g_cpu_load_pct = 0;
static uint32_t g_cpu_load_last_ms = 0;
#ifndef CPU_LOAD_UPDATE_MS
#define CPU_LOAD_UPDATE_MS 1000UL
#endif
#ifndef CPU_LOAD_BASELINE_MAX_ELAPSED_MS
#define CPU_LOAD_BASELINE_MAX_ELAPSED_MS (CPU_LOAD_UPDATE_MS * 3UL)
#endif
#if HTTP_ENABLE
struct WifiScanEntry {
  char ssid[33];
  int32_t rssi;
  uint8_t auth;
};
#ifndef WIFI_SCAN_MAX_RESULTS
#define WIFI_SCAN_MAX_RESULTS 12
#endif
static WifiScanEntry g_wifi_scan[WIFI_SCAN_MAX_RESULTS];
static uint8_t  g_wifi_scan_count = 0;
static uint32_t g_wifi_scan_last_ms = 0;
static bool     g_wifi_scan_valid = false;
static bool     g_wifi_scan_running = false;
static uint32_t g_wifi_scan_started_ms = 0;
static wifi_mode_t g_wifi_scan_restore_mode = WIFI_MODE_NULL;
static bool     g_wifi_scan_restore_ap_only = false;
static bool     g_ota_upload_success = false;
static bool     g_ota_upload_active = false;
static char     g_ota_status_msg[128] = {0};
#endif

struct ErrInfo;

// ===== Forwards ============================================================== 
static void dump_state();
static void cli_show_cfg();
static void print_persist_diag();
static void print_startup_banner();
static void log_fw_banner();
static inline void relay_set(bool on);
static void apply_cfg_to_state();
static void deviceid_init_defaults();
static void cli_poll();
static inline void dbg_print_bus_header_signed(const __FlashStringHelper* tag, int8_t bus);
static inline void relay_log_prefix();
static void poll_rx_base(uint8_t i);
static void poll_rx_p02(uint8_t i);
static void apply_buscount(uint8_t newcnt, bool save);
static void reset_bus_runtime(uint8_t i);
static void net_task();
static void net_publish_state(bool force=false);
static void mqtt_publish_discovery();
static void mqtt_publish_availability(bool online);
static void mqtt_configure_transport();
static void web_handle_root();
static void web_handle_state();
static void web_handle_config();
static void web_handle_save();
static void web_handle_reboot();
static String web_html_control(const String& msg = String());
static String web_html_cli(const String& msg = String(), const String& output = String(), const String& command = String());
static String web_html_update(const String& msg = String(), bool success=false);
static void web_handle_control();
static void web_handle_apply();
static void web_handle_cli();
static void web_handle_cli_exec();
static void web_handle_cli_run();
static void web_handle_cli_clearlog();
static void web_handle_update();
static void web_handle_update_post();
static void web_handle_update_upload();
static void runtime_set_continuous(bool on);
static void runtime_set_usb_connect(bool on);
static void runtime_set_relay_inverted(bool inverted, bool persist);
static bool mqtt_apply_command(const char* topic_suffix, const char* payload);
static bool mqtt_parse_bool_text(const char* s, bool* out);
static bool mqtt_parse_u16_text(const char* s, uint16_t minv, uint16_t maxv, uint16_t* out);
static void web_apply_emulator_values(bool persist);
static const ErrInfo* find_errinfo_by_mask(uint16_t mask);
static const ErrInfo* find_errinfo_by_key(const String& key);
static const char* error_level_name(uint16_t mask);
static String current_error_label();
static void cli_show_net();
static void netcfg_set_defaults();
static void netcfg_load();
static void netcfg_save();
static const char* netcfg_ap_password();
static void net_start_config_portal(bool keep_sta);
static void net_stop_config_portal();
static void net_start_sta();
static void net_refresh_wifi_scan(bool force=false);
static void service_bus_io_once();
static void cooperative_wait_ms(uint32_t ms);
static const char* wifi_auth_label(uint8_t auth);
static String html_escape(const String& in);
static void cpu_load_init();
static void cpu_load_task();
static uint32_t cpu_load_pct();
#if LED_ENABLE
static inline void set_led_idx(uint8_t idx, uint8_t r,uint8_t g,uint8_t b,uint8_t w);
static inline bool led_safe_to_show();
static inline void safe_pixels_show();
#endif

static bool     cfg_dirty = false;
static bool     led_brightness_dirty = false;
static uint32_t cfg_save_deadline = 0;
static uint8_t  g_led_brightness = LED_BRIGHTNESS_MAX;
static inline void schedule_cfg_save(){
#if AUTO_SAVE_CFG_ON_JBC_WRITES
  cfg_dirty = true;
  cfg_save_deadline = millis() + CFG_SAVE_DEBOUNCE_MS;
#endif
}
static inline void schedule_led_brightness_save(){
#if AUTO_SAVE_CFG_ON_JBC_WRITES
  led_brightness_dirty = true;
  cfg_save_deadline = millis() + CFG_SAVE_DEBOUNCE_MS;
#endif
}

#ifndef INTAKE_SENSOR_HOLD_MS
#define INTAKE_SENSOR_HOLD_MS 1500UL
#endif
static uint32_t g_intake_sensor_until[MAX_BUSES][NUM_INTAKES];

static inline void intake_sensor_note(uint8_t bus, uint8_t intake, bool active){
  if (bus >= MAX_BUSES || intake >= NUM_INTAKES) return;
  g_intake_sensor_until[bus][intake] = active ? (millis() + INTAKE_SENSOR_HOLD_MS) : 0UL;
}



static inline void intake_sensor_refresh(uint8_t bus, uint8_t intake){
  intake_sensor_note(bus, intake, true);
}

static inline void intake_sensor_clear_bus(uint8_t bus){
  if (bus >= MAX_BUSES) return;
  for (uint8_t intake = 0; intake < NUM_INTAKES; ++intake) g_intake_sensor_until[bus][intake] = 0UL;
}

static inline void intake_sensor_clear_all(){
  memset(g_intake_sensor_until, 0, sizeof(g_intake_sensor_until));
}

static inline void intake_touch(uint8_t i){
  g_bus[i].t_last_intake_ms = millis();
}

static inline bool fwloop_active(uint8_t i){
  return (g_bus[i].linkState == LINK_UP) &&
         ((int32_t)(millis() - (int32_t)g_bus[i].t_last_intake_ms) >= (int32_t)LED_FW_IDLE_MS);
}

// ===== UART / Pins ========================================
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

#ifndef DBG_CAPTURE_BUF_SIZE
#define DBG_CAPTURE_BUF_SIZE 4096
#endif
#ifndef DBG_RECENT_LOG_SIZE
#define DBG_RECENT_LOG_SIZE 12288
#endif

class DebugMuxPrint : public Print {
public:
  DebugMuxPrint() : _mirror(&Serial), _capture(false), _cap_len(0), _log_head(0), _log_len(0), _log_seq(0), _log_base_seq(0) { _cap[0] = 0; }

  void setMirror(Print* mirror){ _mirror = mirror ? mirror : &Serial; }
  void clearCapture(){ _cap_len = 0; _cap[0] = 0; }
  void beginCapture(){ clearCapture(); _capture = true; }
  void endCapture(){ _capture = false; }
  String captureString() const { return String(_cap); }
  String recentLogString() const {
    String out;
    appendLogSlice(out, 0, _log_len);
    return out;
  }
  uint32_t recentLogSeq() const { return _log_seq; }
  uint32_t recentLogBaseSeq() const { return _log_base_seq; }
  String recentLogChunk(uint32_t from_seq, bool* reset = nullptr, uint32_t* next_seq = nullptr) const {
    const bool need_reset = (from_seq < _log_base_seq) || (from_seq > _log_seq);
    if (reset) *reset = need_reset;
    if (next_seq) *next_seq = _log_seq;
    if (need_reset) return recentLogString();
    const size_t start = (size_t)(from_seq - _log_base_seq);
    if (start >= _log_len) return String("");
    String out;
    appendLogSlice(out, start, _log_len - start);
    return out;
  }
  void clearRecentLog(){ _log_head = 0; _log_len = 0; _log_base_seq = _log_seq; }

  virtual size_t write(uint8_t c) override {
    if (_mirror) _mirror->write(c);
    if (_capture && _cap_len + 1 < sizeof(_cap)){
      _cap[_cap_len++] = (char)c;
      _cap[_cap_len] = 0;
    }
    appendRecent((char)c);
    return 1;
  }

  virtual size_t write(const uint8_t *buffer, size_t size) override {
    size_t written = 0;
    for(size_t i = 0; i < size; ++i) written += write(buffer[i]);
    return written;
  }

private:
  void appendLogSlice(String& out, size_t start, size_t count) const {
    if (!count) return;
    out.reserve(out.length() + count);
    const size_t cap = sizeof(_log);
    const size_t logical_start = (_log_head + start) % cap;
    for(size_t i = 0; i < count; ++i){
      out += _log[(logical_start + i) % cap];
    }
  }

  void appendRecent(char c){
    const size_t cap = sizeof(_log);
    if (!cap) return;

    if (_log_len < cap){
      const size_t idx = (_log_head + _log_len) % cap;
      _log[idx] = c;
      ++_log_len;
    } else {
      _log[_log_head] = c;
      _log_head = (_log_head + 1) % cap;
      ++_log_base_seq;
    }
    ++_log_seq;
  }

  Print* _mirror;
  bool _capture;
  size_t _cap_len;
  char _cap[DBG_CAPTURE_BUF_SIZE];
  size_t _log_head;
  size_t _log_len;
  char _log[DBG_RECENT_LOG_SIZE];
  uint32_t _log_seq;
  uint32_t _log_base_seq;
};

static DebugMuxPrint g_dbg;
#define DBG g_dbg

// ===== DBG_PRINTF =============================================
#define DBG_PRINTF(...) DBG.printf(__VA_ARGS__)
// Netsetup
static void net_setup();
static void net_start_http_if_needed(){
#if HTTP_ENABLE
  if (!g_http_started){
    g_http.begin();
    g_http_started = true;
    DBG.println(F("[HTTP] started"));
  }
#endif
}


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
  switch (dbg_ts_mode){
    case 0:
      break;
    case 1:
#if LOG_ICONS
      DBG_PRINTF("⏱%lu ", (unsigned long)millis());
#else
      DBG_PRINTF("[%lu] ", (unsigned long)millis());
#endif
      break;
    case 2: {
      unsigned long ms = millis();
      unsigned long s  = ms / 1000;
      unsigned long d  = (ms % 1000) / 100;
#if LOG_ICONS
      DBG_PRINTF("⏱%lu.%lus ", s, d);
#else
      DBG_PRINTF("[%lu.%lus] ", s, d);
#endif
      break;
    }
    default:
      break;
  }
}
static inline void print_onoff_icon(bool on){
#if LOG_ICONS
  DBG.print(on ? F("🟢ON") : F("⚪OFF"));
#else
  DBG.print(on ? F("ON") : F("OFF"));
#endif
}
static void print_ascii_quoted(const uint8_t* p, uint8_t n){
  while(n && p[n-1]==0x20) n--;
  DBG.print('"');
  for(uint8_t i=0;i<n;i++){
    char c=(char)p[i];
    if(c>=0x20 && c<=0x7E) DBG.write(c);
    else DBG.write('.');
  }
  DBG.print('"');
}

static inline bool station_addr_is_known(uint8_t addr){
  return addr == 0x18 || addr == 0x12 || addr == 0x10;
}
static flashstr_t station_name_from_addr(uint8_t addr){
  switch(addr){
    case 0x00: return FS_LIT("BCAST");
    case 0x18: return FS_LIT("DDE");
    case 0x12: return FS_LIT("JTSE");
    case 0x10: return FS_LIT("CD");
    default:   return FS_LIT("UNKNOWN");
  }
}
static inline void print_station_name_from_addr(uint8_t addr){
  DBG.print(station_name_from_addr(addr));
}
static inline void print_addr_with_name(uint8_t addr){
  DBG.print(F("0x"));
  if (addr < 0x10) DBG.print('0');
  DBG.print(addr, HEX);
  DBG.print(F(" ("));
  print_station_name_from_addr(addr);
  DBG.print(F(")"));
}
static inline uint8_t bus_label_addr(uint8_t i){
  if (station_addr_is_known(g_bus[i].my_addr))  return g_bus[i].my_addr;
  if (station_addr_is_known(g_bus[i].stn_addr)) return g_bus[i].stn_addr;
  if (g_bus[i].my_addr  != 0x00) return g_bus[i].my_addr;
  return g_bus[i].stn_addr;
}
static inline void dbg_print_bus_label(uint8_t i){
  DBG.print(F("b"));
  DBG.print(i);
  DBG.print(F("/"));
  print_station_name_from_addr(bus_label_addr(i));
}
static bool dbg_group_bus_visual = true;
static int8_t dbg_last_group_bus = -1;
static inline void dbg_maybe_print_bus_group(uint8_t i){
  if (!dbg_group_bus_visual || g_bus_count < 2) return;
  if (dbg_last_group_bus == (int8_t)i) return;
  if (dbg_last_group_bus >= 0) DBG.println();
  DBG.print(F("---- BUS "));
  DBG.print(i);
  DBG.print(F(" / "));
  print_station_name_from_addr(bus_label_addr(i));
  DBG.println(F(" ----"));
  dbg_last_group_bus = (int8_t)i;
}
static inline void dbg_print_bus_header(const __FlashStringHelper* tag, uint8_t i){
  dbg_maybe_print_bus_group(i);
  DBG.print(F("["));
  DBG.print(tag);
  DBG.print(F(" "));
  dbg_print_bus_label(i);
  DBG.print(F("] "));
}
static inline void dbg_print_bus_header_signed(const __FlashStringHelper* tag, int8_t bus){
  if (bus >= 0 && bus < (int8_t)g_bus_count) {
    dbg_print_bus_header(tag, (uint8_t)bus);
    return;
  }
  DBG.print(F("["));
  DBG.print(tag);
  DBG.print(F("] "));
}
static inline void update_station_addr(uint8_t i, uint8_t src){
  if (src == 0x00) return;
  if (g_bus[i].stn_addr == src) return;
  g_bus[i].stn_addr = src;
  if (dbg_show_proto){
    dbg_time_prefix();
    dbg_print_bus_header(F("STN"), i);
    DBG.print(F("src="));
    print_addr_with_name(src);
    DBG.println();
  }
}
static inline bool is_intake_activation_ctrl(uint8_t ctrl){
  using namespace jbc_cmd::FE_02;
  return ctrl == M_R_INTAKEACTIVATION || ctrl == M_W_INTAKEACTIVATION;
}
enum : uint8_t {
  DBG_PAYLOADFMT_HEX = 0,
  DBG_PAYLOADFMT_DEC = 1,
  DBG_PAYLOADFMT_ASCII = 2,
  DBG_PAYLOADFMT_AUTO = 3
};

static uint8_t dbg_payload_fmt = DBG_PAYLOADFMT_AUTO;

static inline bool dbg_ctrl_payload_prefers_ascii(uint8_t ctrl){
  using namespace jbc_cmd::FE_02;
  switch (ctrl) {
    case M_FIRMWARE:
    case M_R_DEVICEID:
      return true;
    default:
      return false;
  }
}

static inline bool dbg_payload_is_mostly_printable(const uint8_t* data, uint8_t len){
  if (!data || len == 0) return false;
  uint8_t printable = 0;
  for (uint8_t i = 0; i < len; ++i) {
    const uint8_t c = data[i];
    if (c >= 0x20 && c <= 0x7E) printable++;
  }
  return (len >= 4) && ((uint16_t)printable * 100u >= (uint16_t)len * 85u);
}

static inline const __FlashStringHelper* dbg_payload_fmt_name(uint8_t fmt){
  switch (fmt) {
    case DBG_PAYLOADFMT_HEX:   return F("hex");
    case DBG_PAYLOADFMT_DEC:   return F("dec");
    case DBG_PAYLOADFMT_ASCII: return F("ascii");
    case DBG_PAYLOADFMT_AUTO:  return F("auto");
    default:                   return F("?");
  }
}

static inline bool dbg_payload_fmt_from_text(const char* s, uint8_t* out){
  if (!s || !out) return false;
  if (!strcasecmp(s, "hex"))   { *out = DBG_PAYLOADFMT_HEX;   return true; }
  if (!strcasecmp(s, "dec"))   { *out = DBG_PAYLOADFMT_DEC;   return true; }
  if (!strcasecmp(s, "ascii")) { *out = DBG_PAYLOADFMT_ASCII; return true; }
  if (!strcasecmp(s, "auto"))  { *out = DBG_PAYLOADFMT_AUTO;  return true; }
  return false;
}

static inline void dbg_print_payload_inline(const uint8_t* data, uint8_t len, uint8_t ctrl){
  DBG.print(F(" payload="));
  if (!len || !data) {
    DBG.print('-');
    return;
  }

  uint8_t fmt = dbg_payload_fmt;
  if (fmt == DBG_PAYLOADFMT_AUTO) {
    fmt = (dbg_ctrl_payload_prefers_ascii(ctrl) || dbg_payload_is_mostly_printable(data, len))
            ? DBG_PAYLOADFMT_ASCII
            : DBG_PAYLOADFMT_HEX;
  }

  switch (fmt) {
    case DBG_PAYLOADFMT_ASCII:
      print_ascii_quoted(data, len);
      return;

    case DBG_PAYLOADFMT_DEC:
      for (uint8_t idx = 0; idx < len; ++idx) {
        if (idx) DBG.print(' ');
        DBG.print((unsigned)data[idx]);
      }
      return;

    case DBG_PAYLOADFMT_HEX:
    default:
      for (uint8_t idx = 0; idx < len; ++idx) {
        if (idx) DBG.print(' ');
        if (data[idx] < 0x10) DBG.print('0');
        DBG.print(data[idx], HEX);
      }
      return;
  }
}

// ===== Relais Helpers + Zustand =============================================
static bool     g_relay_on = false;
static uint8_t  g_relay_inverted = (RELAY_ACTIVE_LOW ? 1 : 0);
static uint32_t g_relay_off_deadline = 0;
static inline void relay_hw_write(bool on){
#if RELAY_ENABLE
  digitalWrite(RELAY_PIN, g_relay_inverted ? !on : on);
#else
  (void)on;
#endif
}
static inline void relay_set(bool on){
  relay_hw_write(on);
  if (g_relay_on == on) return;
  g_relay_on = on;
  if (dbg_show_state) {
    dbg_time_prefix();
    relay_log_prefix();
    print_onoff_icon(on);
    if (on) {
      if (st.continuous) {
        DBG.print(F(" Continuous"));
      } else {
        DBG.print(F(" Delay Work "));
        DBG.print(st.delaySec[0][WORK_IDX]);
        DBG.print(F("s"));
      }
    }
    DBG.println();
  }
}
static inline void relay_init(){
#if RELAY_ENABLE
  pinMode(RELAY_PIN, OUTPUT);
#endif
  relay_set(false);
}

static int8_t g_afterrun_owner = -1;
static int8_t g_relay_owner = -1;              // Bus, der zuletzt M_W_INTAKEACTIVATION(WORK=1) schrieb
static uint8_t g_work_mask = 0;
static inline bool bus_active(uint8_t i){
  return (g_work_mask & (uint8_t)(1u << i)) != 0;
}
static inline void work_set(uint8_t bus, bool on){
  if (on) g_work_mask |=  (uint8_t)(1u << bus);
  else    g_work_mask &= ~(uint8_t)(1u << bus);
}
static inline bool any_work_active(){
  return g_work_mask != 0;
}
static inline bool bus_afterrun(uint8_t i){
  return (g_relay_off_deadline != 0) && (g_afterrun_owner == (int8_t)i);
}
static inline bool bus_intake_latched(uint8_t bus, uint8_t intake){
  if (bus >= MAX_BUSES || intake >= NUM_INTAKES) return false;
  return g_bus_intakeAct[bus][0][intake] != 0;
}
static inline bool any_intake_latched(uint8_t intake){
  if (intake >= NUM_INTAKES) return false;
  for (uint8_t b = 0; b < g_bus_count; ++b){
    if (bus_intake_latched(b, intake)) return true;
  }
  return false;
}
static inline int8_t pick_active_bus_owner(){
  for (uint8_t b = 0; b < g_bus_count; ++b){
    if (bus_active(b)) return (int8_t)b;
  }
  return -1;
}
static inline int8_t relay_log_bus(){
  if (g_relay_owner >= 0 && g_relay_owner < (int8_t)g_bus_count) return g_relay_owner;
  if (g_afterrun_owner >= 0 && g_afterrun_owner < (int8_t)g_bus_count) return g_afterrun_owner;
  if (g_bus_count == 1) return 0;
  return -1;
}
static inline void relay_log_prefix(){
  dbg_print_bus_header_signed(F("RELAY"), relay_log_bus());
}
static inline uint32_t relay_time_left_ms(){
  if (!g_relay_off_deadline) return 0;
  const int32_t remaining = (int32_t)(g_relay_off_deadline - millis());
  return (remaining > 0) ? (uint32_t)remaining : 0;
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

// Preferences persistence
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
static void persist_save_cfg(){ nv.putBytes("cfg", &cfg, sizeof(PersistCfg)); }
static uint8_t persist_load_buscount(){
  uint8_t bc = nv.getUChar("buscnt", BUS_COUNT);
  if (bc < 1 || bc > MAX_BUSES) bc = BUS_COUNT;
  return bc;
}
static void persist_save_buscount(uint8_t bc){ if (bc >= 1 && bc <= MAX_BUSES) nv.putUChar("buscnt", bc); }
static uint8_t persist_load_relay_invert(){ return nv.getUChar("r_inv", (uint8_t)(RELAY_ACTIVE_LOW ? 1 : 0)) ? 1 : 0; }
static void persist_save_relay_invert(uint8_t inv){ nv.putUChar("r_inv", inv ? 1 : 0); }
static uint8_t persist_load_led_brightness(){ return nv.getUChar("led_br", LED_BRIGHTNESS_MAX); }
static void persist_save_led_brightness(uint8_t v){ nv.putUChar("led_br", v); }

static void init_cfg_defaults(){
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
}

static void init_runtime_state(){
  memset(g_bus_intakeAct, DEF_ACTIVATION, sizeof(g_bus_intakeAct));
  for(uint8_t p=0;p<MAX_PORTS;p++){
    st.pedalAct[p]=DEF_PEDAL_ACT;
    st.pedalMode[p]=DEF_PEDAL_MODE;
    st.connectedPedal[p]=1;
    for(uint8_t i=0;i<NUM_INTAKES;i++){
      st.intakeAct[p][i]=DEF_ACTIVATION;
      st.delaySec[p][i]=(i==WORK_IDX)?DEF_TSTOP_WORK:DEF_TSTOP_STAND;
      st.tstopMs[p][i]=0;
    }
  }
}

static void deviceid_init_defaults(){
  memcpy(g_bus[0].dev_id, FE_DEVICE_ID_STR0, 32);
  memcpy(g_bus[1].dev_id, FE_DEVICE_ID_STR1, 32);
  g_bus[0].dev_id_len = 32;
  g_bus[1].dev_id_len = 32;
}

static void persist_erase_all(){
  nv.clear();
}

// ===== Laufzeit-Konfiguration (CLI) =========================================
static bool     g_state_dirty = true;
static uint32_t t_last_state  = 0;
static inline void mark_state_dirty(){ g_state_dirty = true; }
static inline void maybe_dump_state_on_write(){
  if(dbg_show_state && dbg_state_on_write_only && g_state_dirty){
    dump_state();
    g_state_dirty=false;
  }
}

// ===== Fehlerkürzel / Warnungen -> Bitmaske + Beschreibung ==================
struct ErrInfo {
  const char* key;
  uint16_t    mask;
  flashstr_t  desc;
};

#define DESC(x) (reinterpret_cast<flashstr_t>(x))

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

static const uint16_t ERR_MASK_WARN = (2u | 8u);
static inline bool has_stop_error(uint16_t e){ return (e & ~ERR_MASK_WARN) != 0; }
static inline bool has_warn_error(uint16_t e){ return !has_stop_error(e) && (e & ERR_MASK_WARN); }

static uint16_t lookup_err_mask(const char* s){
  for (size_t i=0;i<sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    if (!strcasecmp(s, kErrMap[i].key)) return kErrMap[i].mask;
  }
  return 0;
}

static const ErrInfo* find_errinfo_by_mask(uint16_t mask){
  for (size_t i=0;i<sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    if (kErrMap[i].mask == mask) return &kErrMap[i];
  }
  return nullptr;
}
static const ErrInfo* find_errinfo_by_key(const String& key){
  for (size_t i=0;i<sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    if (key.equalsIgnoreCase(kErrMap[i].key)) return &kErrMap[i];
  }
  return nullptr;
}
static const char* error_level_name(uint16_t mask){
  if (mask == 0) return "none";
  if (has_stop_error(mask)) return "stop";
  if (has_warn_error(mask)) return "warn";
  return "custom";
}
static String current_error_label(){
  if (st.statError == 0) return String("No error");
  const ErrInfo* e = find_errinfo_by_mask(st.statError);
  if (!e) return String("Custom / unknown mask");
  String s = e->key;
  s += F(" - ");
  s += (const char*)e->desc;
  return s;
}
static String error_option_label_by_mask(uint16_t mask){
  if (mask == 0) return String("NOERR - No error");
  const ErrInfo* e = find_errinfo_by_mask(mask);
  if (!e) return String("CUSTOM - Unknown");
  String s = e->key;
  s += F(" - ");
  s += (const char*)e->desc;
  return s;
}
static const char* relay_mode_key(){
  return g_relay_inverted ? "nc" : "no";
}
static const char* relay_mode_label(){
  return g_relay_inverted ? "Öffner (NC)" : "Schließer (NO)";
}

static uint8_t suction_level_clamp(uint8_t v){
  return (v > 3U) ? 3U : v;
}
static const char* suction_level_key(uint8_t v){
  switch (suction_level_clamp(v)){
    case 0: return "high";
    case 1: return "mid";
    case 2: return "low";
    default: return "custom";
  }
}
static const char* suction_level_label(uint8_t v){
  switch (suction_level_clamp(v)){
    case 0: return "High";
    case 1: return "Mid";
    case 2: return "Low";
    default: return "Custom";
  }
}
static bool suction_level_from_text(const char* s, uint8_t* out){
  if (!s) return false;
  bool ok = false;
  long v = strtol(s, nullptr, 0);
  if (strcmp(s, "0") == 0 || strcmp(s, "1") == 0 || strcmp(s, "2") == 0 || strcmp(s, "3") == 0){
    if (out) *out = suction_level_clamp((uint8_t)v);
    return true;
  }
  if (!strcasecmp(s, "high"))   { if (out) *out = 0; return true; }
  if (!strcasecmp(s, "mid"))    { if (out) *out = 1; return true; }
  if (!strcasecmp(s, "low"))    { if (out) *out = 2; return true; }
  if (!strcasecmp(s, "custom")) { if (out) *out = 3; return true; }
  return false;
}
static String bus_device_id_string(uint8_t bus){
  String out;
  if (bus >= MAX_BUSES) return out;
  out.reserve(g_bus[bus].dev_id_len + 4);
  int16_t last = (int16_t)g_bus[bus].dev_id_len - 1;
  while (last >= 0){
    char c = (char)g_bus[bus].dev_id[last];
    if (c == ' ' || c == '\t' || c == '\0') --last;
    else break;
  }
  for (int16_t i = 0; i <= last; ++i){
    char c = (char)g_bus[bus].dev_id[i];
    out += (c >= 0x20 && c <= 0x7E) ? c : '.';
  }
  return out;
}
static const char* bus_connection_label(uint8_t bus){
  if (bus >= g_bus_count) return "inactive";
  switch (g_bus[bus].linkState){
    case LINK_UP: return "connected";
    case LINK_CONNECTING: return "connecting";
    case LINK_DOWN:
    default: return "disconnected";
  }
}
static String tool_sensor_html_from_state(bool active, bool show_stand, bool inactive_bus=false, bool is_work=false){
  const char* color = active ? "#3bd16f" : (inactive_bus ? "#777777" : "#f2f2f2");
  const char* glow  = active ? "0 0 12px rgba(59,209,111,.26)" : (inactive_bus ? "none" : "0 0 7px rgba(255,255,255,.08)");
  const char* stateLabel = inactive_bus ? "inactive" : (active ? "active" : "inactive");
  const char* stateCls   = inactive_bus ? "state-chip state-inactive tool-state-chip" : (active ? "state-chip tool-state-active tool-state-chip" : "state-chip state-idle tool-state-chip");
  String out;
  out.reserve(1180);
  out += F("<div class='tool-status-wrap'><span class='tool-icon-box");
  if (show_stand) out += F(" with-stand");
  if (is_work) out += F(" is-work");
  else if (show_stand) out += F(" stand-only");
  out += F("' style='--tool-color:");
  out += color;
  out += F(";--tool-glow:");
  out += glow;
  out += F("'>");
  if (is_work){
    out += F("<span class='tool-iron'></span>");
  } else if (show_stand){
    out += F("<span class='tool-stand-base'></span><span class='tool-stand-post'></span><span class='tool-stand-cradle'></span><span class='tool-stand-hook'></span>");
  }
  out += F("</span><span class='");
  out += stateCls;
  out += F("'>");
  out += stateLabel;
  out += F("</span></div>");
  return out;
}
static String tool_sensor_html(uint8_t bus, uint8_t intake){
  const bool inactive_bus = (bus >= g_bus_count);
  const bool active = (!inactive_bus) && bus_intake_latched(bus, intake);
  const bool is_work = (intake == WORK_IDX);
  const bool show_stand = (intake == STAND_IDX);
  return tool_sensor_html_from_state(active, show_stand, inactive_bus, is_work);
}
static const char* bus_relay_state_label(uint8_t bus){
  if (bus >= g_bus_count) return "inactive";
  if (st.continuous && g_relay_on) return "continuous";
  if (bus_active(bus) && g_relay_on && (g_relay_off_deadline == 0)) return "work";
  if (bus_afterrun(bus)) return "afterrun";
  return "idle";
}
static String relay_state_chip_html(uint8_t bus){
  const char* label = bus_relay_state_label(bus);
  const char* cls = "state-chip state-idle";
  if (!strcmp(label, "work")) cls = "state-chip state-active";
  else if (!strcmp(label, "afterrun")) cls = "state-chip state-afterrun";
  else if (!strcmp(label, "continuous")) cls = "state-chip state-continuous";
  else if (!strcmp(label, "inactive")) cls = "state-chip state-inactive";
  String out;
  out.reserve(96);
  out += F("<span class='"); out += cls; out += F("'>"); out += label; out += F("</span>");
  return out;
}
static const char* led_link_mode_key(uint8_t bus){
  if (bus >= g_bus_count) return "off";
  if (fwloop_active(bus)) return "blink-yellow";
  switch (g_bus[bus].linkState){
    case LINK_DOWN:       return "breathe-red";
    case LINK_CONNECTING: return "blink-yellow";
    case LINK_UP:         return "solid-green";
    default:              return "off";
  }
}
static const char* led_event_mode_key(uint8_t bus){
  if (bus >= g_bus_count) return "off";
  if (st.continuous && g_relay_on) return "blink-blue";
  if (has_stop_error(st.statError)) return "blink-red";
  if (has_warn_error(st.statError)) return "blink-yellow";
  if (g_bus[bus].linkState != LINK_UP) return "off";
  if (bus_active(bus) && g_relay_on && (g_relay_off_deadline == 0)) return "breathe-blue";
  if (bus_afterrun(bus)) return "blink-magenta";
  return "breathe-white";
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

static bool cli_parse_onoff(const char* s, bool* ok){
  if(!s){ *ok=false; return false; }
  if(!strcasecmp(s,"1") || !strcasecmp(s,"on") || !strcasecmp(s,"true") || !strcasecmp(s,"yes")){ *ok=true; return true; }
  if(!strcasecmp(s,"0") || !strcasecmp(s,"off") || !strcasecmp(s,"false") || !strcasecmp(s,"no")){ *ok=true; return false; }
  *ok=false; return false;
}
static inline void dbg_set_all(bool en){
  dbg_show_rx    = en;
  dbg_show_tx    = en;
  dbg_show_proto = en;
  dbg_show_err   = en;
  dbg_show_state = en;
}
static void cli_show_dbg(){
  DBG.print(F("[DBG] rx=")); DBG.print(dbg_show_rx ? F("on") : F("off"));
  DBG.print(F(" tx=")); DBG.print(dbg_show_tx ? F("on") : F("off"));
  DBG.print(F(" proto=")); DBG.print(dbg_show_proto ? F("on") : F("off"));
  DBG.print(F(" err=")); DBG.print(dbg_show_err ? F("on") : F("off"));
  DBG.print(F(" state=")); DBG.print(dbg_show_state ? F("on") : F("off"));
  DBG.print(F(" state_mode=")); DBG.print(dbg_state_on_write_only ? F("write") : F("periodic"));
  DBG.print(F(" state_period_ms=")); DBG.print(dbg_state_period_ms);
  DBG.print(F(" rxtx_rate_ms=")); DBG.print(dbg_rxtx_min_interval_ms);
  DBG.print(F(" payloadfmt=")); DBG.print(dbg_payload_fmt_name(dbg_payload_fmt));
  DBG.print(F(" ts=")); DBG.println(dbg_ts_mode);
}

static void reset_bus_runtime(uint8_t i){
  g_bus[i].baseState = BASE_IDLE;
  g_bus[i].linkState = LINK_DOWN;
  g_bus[i].rxState   = WAIT_DLE;
  g_bus[i].payLen = 0;
  g_bus[i].in_escape = false;
  g_bus[i].have_len = false;
  g_bus[i].data_len = 0;
  g_bus[i].addr_locked = false;
  g_bus[i].stn_addr = 0x00;
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

  for (uint8_t i = old; i < g_bus_count; ++i) {
    JBC_BEGIN(i);
    reset_bus_runtime(i);
  }

  for (uint8_t i = g_bus_count; i < old; ++i) {
    if (g_bus[i].ser) {
      g_bus[i].ser->end();
      work_set(i, false);
      memset(g_bus_intakeAct[i], DEF_ACTIVATION, sizeof(g_bus_intakeAct[i]));
      intake_sensor_clear_bus(i);
      if (g_afterrun_owner == (int8_t)i) g_afterrun_owner = -1;
      if (g_relay_owner == (int8_t)i) g_relay_owner = pick_active_bus_owner();
    }
    reset_bus_runtime(i);
  }

#if LED_ENABLE
  for (uint8_t idx = g_bus_count * 2; idx < LED_COUNT_MAX; ++idx) set_led_idx(idx, 0,0,0,0);
  safe_pixels_show();
#endif

  if (save) persist_save_buscount(g_bus_count);
  DBG.print(F("[BUS] active buses = "));
  DBG.println(g_bus_count);
}

static void log_fw_banner(){
  dbg_time_prefix();
  DBG.print(F("[FW] "));
  DBG.print(FE_FW_STR);
  DBG.print(F(" | FW "));
  DBG.print(F(USER_FW_VERSION));
  DBG.print(F(" by IceCube20 | build "));
  DBG.print(F(__DATE__));
  DBG.print(' ');
  DBG.println(F(__TIME__));
}

static void print_persist_diag(){
  dbg_time_prefix();
  DBG.println(F("[NVS]"));

  for (uint8_t i = 0; i < MAX_BUSES; i++) {
    DBG.print(F(" "));
    DBG_PRINTF("bus%u DeviceID len=%u ascii=", i, g_bus[i].dev_id_len);
    print_ascii_quoted(g_bus[i].dev_id, g_bus[i].dev_id_len);
    DBG.print(F(" | addr="));
    print_addr_with_name(g_bus[i].my_addr);
    DBG.print(F(" | stn="));
    print_addr_with_name(g_bus[i].stn_addr);
    if (i >= g_bus_count) DBG.print(F(" | (inactive)"));
    DBG.println();
  }
  DBG.println(F("[CLI] type 'help' for commands"));
}

static void print_startup_banner(){
  print_persist_diag();
  cli_show_cfg();
  log_fw_banner();
}

static void cli_show_help(){
  DBG.println(F("Commands:"));
  DBG.println(F("  help                     - Show this help"));
  DBG.println(F("  ?                        - Show device, address and startup information"));
  DBG.println(F("  show cfg                 - Show current emulator configuration"));
  DBG.println(F("  show buses               - Show current bus count"));
  DBG.println(F("  show dbg                 - Show current debug settings"));
  DBG.println(F("  show net                 - Show WiFi, AP, web and MQTT status"));
  DBG.println(F("  set buses <1|2>          - Set the number of connected JBC stations"));
  DBG.println(F("  save / load              - Save or load configuration data"));
  DBG.println(F("  clearweb                 - Clear web username/password and save immediately"));
  DBG.println(F("  erase [eeprom]           - Clear NVS/EEPROM and reload defaults"));
  DBG.println(F("  set tstop_work <sec>     - Set afterrun time for WORK mode"));
  DBG.println(F("  set tstop_stand <sec>    - Set afterrun time for STAND mode"));
  DBG.println(F("  set suctionlevel <0..3>  - Set suction level (0=High, 1=Mid, 2=Low, 3=Custom)"));
  DBG.println(F("  set selectflow <0..100>  - Set selected flow in percent"));
  DBG.println(F("  set stand_intakes <0|1>  - Set stand intakes off or on"));
  DBG.println(F("  set filter_life <0..100> - Set filter life in percent"));
  DBG.println(F("  set filter_sat <0..100>  - Set filter saturation in percent"));
  DBG.println(F("  set continuous <0|1>     - Set continuous suction off or on"));
  DBG.println(F("  set relay_mode <no|nc>   - Set relay contact mode"));
  DBG.println(F("  set led_brightness <10..100> - Set LED brightness in percent"));
  DBG.println(F("  set actual_flow <0..100>  - Set simulated actual flow in percent (runtime only)"));
  DBG.println(F("  set speed <0..65535>     - Set simulated blower speed in RPM (runtime only)"));
  DBG.println(F("  set pedal_act <0|1>      - Set pedal activation flag"));
  DBG.println(F("  set pedal_mode <0..2>    - Set pedal mode"));
  DBG.println(F("  dbg rx|tx|proto|err|state <on|off>"));
  DBG.println(F("  dbg all <on|off>         - Set all debug outputs on or off"));
  DBG.println(F("  dbg none / dbg quiet     - Alias for dbg all off"));
  DBG.println(F("  dbg state_mode <write|periodic>"));
  DBG.println(F("  dbg state_period <ms>    - Set periodic state dump interval"));
  DBG.println(F("  dbg rate <ms>            - Minimum interval for RX/TX log lines (0=off)"));
  DBG.println(F("  dbg payloadfmt <hex|dec|ascii|auto> - Set payload display format"));
  DBG.println(F("  dbg ts <0|1|2>           - Set timestamp mode (0=off, 1=ms, 2=s.x)"));
  DBG.println(F("  STOPx/WARNx, NOERR, ERR  - Set or clear error and warning presets"));
}

static void cli_show_cfg(){
  DBG.print(F("[CFG] buses=")); DBG.print(g_bus_count);
  DBG.print(F(" | tstop(w/s)=")); DBG.print(cfg.tstop_work); DBG.print('/'); DBG.print(cfg.tstop_stand);
  DBG.print(F(" | suctionlevel=")); DBG.print(st.suctionLevel);
  DBG.print(F(" | selectflow=")); DBG.print(selectflow_raw_to_pct(st.selectFlow)); DBG.print(F(" %"));
  DBG.print(F(" | stand_intakes=")); DBG.print(cfg.stand_intakes);
  DBG.print(F(" | continuous=")); DBG.print(st.continuous ? F("on") : F("off"));
  DBG.print(F(" | relay_mode=")); DBG.print(relay_mode_key());
  DBG.print(F(" | filter(life/sat)=")); DBG.print(filter_raw_to_pct(st.filterLife)); DBG.print('/'); DBG.print(filter_raw_to_pct(st.filterSaturation)); DBG.print(F(" %"));
  DBG.print(F(" | actual_flow=")); DBG.print(actualflow_raw_to_pct(st.flow_x1000)); DBG.print(F(" %"));
  DBG.print(F(" | speed=")); DBG.print(st.speed_rpm);
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

static void runtime_set_continuous(bool on){
  st.continuous = on ? 1 : 0;
  if (st.continuous){
    g_relay_off_deadline = 0;
    relay_set(true);
  }
  mark_state_dirty();
  maybe_dump_state_on_write();
}

static void runtime_set_usb_connect(bool on){
  st.usbConnect = on ? 1 : 0;
  mark_state_dirty();
  maybe_dump_state_on_write();
}

static void runtime_set_relay_inverted(bool inverted, bool persist){
  const uint8_t new_value = inverted ? 1 : 0;
  if (g_relay_inverted != new_value){
    g_relay_inverted = new_value;
    relay_hw_write(g_relay_on);
    mark_state_dirty();
    maybe_dump_state_on_write();
#if MQTT_ENABLE
    g_mqtt_force_publish = true;
#endif
  }
  if (persist) persist_save_relay_invert(g_relay_inverted);
}

static bool mqtt_parse_bool_text(const char* s, bool* out){
  bool ok = false;
  bool v = cli_parse_onoff(s, &ok);
  if (ok){
    if (out) *out = v;
    return true;
  }
  if (!strcasecmp(s, "toggle")){
    return false;
  }
  return false;
}

static bool mqtt_parse_u16_text(const char* s, uint16_t minv, uint16_t maxv, uint16_t* out){
  bool ok = false;
  long v = cli_parse_long(s, &ok);
  if (!ok) return false;
  if (v < (long)minv) v = minv;
  if (v > (long)maxv) v = maxv;
  if (out) *out = (uint16_t)v;
  return true;
}

static bool mqtt_apply_command(const char* topic_suffix, const char* payload){
  if (!topic_suffix || !payload) return false;

  bool changed = false;
  bool persist_cfg = false;
  bool publish_now = false;
  bool b = false;
  uint16_t u16 = 0;

  if (!strcasecmp(topic_suffix, "continuous")){
    if (!mqtt_parse_bool_text(payload, &b)) return false;
    runtime_set_continuous(b);
    return true;
  }
  if (!strcasecmp(topic_suffix, "work_active") || !strcasecmp(topic_suffix, "stand_active")){
    return false;
  }
  if (!strcasecmp(topic_suffix, "suction_level")){
    uint8_t su = 0;
    if (!suction_level_from_text(payload, &su)) return false;
    cfg.suctionLevel = su;
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "select_flow")){
    if (!mqtt_parse_u16_text(payload, 0, 100, &u16)) return false;
    cfg.selectFlow = selectflow_pct_to_raw(u16);
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "tstop_work")){
    if (!mqtt_parse_u16_text(payload, CL_TSTOP_WORK_MIN, CL_TSTOP_WORK_MAX, &u16)) return false;
    cfg.tstop_work = u16;
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "tstop_stand")){
    if (!mqtt_parse_u16_text(payload, CL_TSTOP_STAND_MIN, CL_TSTOP_STAND_MAX, &u16)) return false;
    cfg.tstop_stand = u16;
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "stand_intakes")){
    if (!mqtt_parse_bool_text(payload, &b)) return false;
    cfg.stand_intakes = b ? 1 : 0;
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "filter_life")){
    if (!mqtt_parse_u16_text(payload, 0, 100, &u16)) return false;
    cfg.filter_life = filter_pct_to_raw(u16);
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "filter_sat")){
    if (!mqtt_parse_u16_text(payload, 0, 100, &u16)) return false;
    cfg.filter_sat = filter_pct_to_raw(u16);
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "actual_flow")){
    return false;
  } else if (!strcasecmp(topic_suffix, "speed")){
    if (!mqtt_parse_u16_text(payload, CL_SPEED_MIN, CL_SPEED_MAX, &u16)) return false;
    st.speed_rpm = u16;
    mark_state_dirty();
    maybe_dump_state_on_write();
#if MQTT_ENABLE
    g_mqtt_force_publish = true;
#endif
    return true;
  } else if (!strcasecmp(topic_suffix, "bus_count")){
    if (!mqtt_parse_u16_text(payload, 1, MAX_BUSES, &u16)) return false;
    apply_buscount((uint8_t)u16, true);
#if MQTT_ENABLE
    g_mqtt_force_publish = true;
#endif
    return true;
  } else if (!strcasecmp(topic_suffix, "relay_mode")){
    if (!strcasecmp(payload, "no") || !strcasecmp(payload, "schliesser") || !strcasecmp(payload, "schließer")) {
      runtime_set_relay_inverted(false, true);
      return true;
    }
    if (!strcasecmp(payload, "nc") || !strcasecmp(payload, "oeffner") || !strcasecmp(payload, "öffner")) {
      runtime_set_relay_inverted(true, true);
      return true;
    }
    if (mqtt_parse_bool_text(payload, &b)){
      runtime_set_relay_inverted(b, true);
      return true;
    }
    return false;
  } else if (!strcasecmp(topic_suffix, "error_code")){
    String err_payload = String(payload);
    err_payload.trim();
    if (err_payload.startsWith("NOERR") || err_payload.equalsIgnoreCase("No error")) cfg.stat_error = 0;
    else {
      int sep = err_payload.indexOf(" - ");
      String err_key = (sep > 0) ? err_payload.substring(0, sep) : err_payload;
      err_key.trim();
      const ErrInfo* e = find_errinfo_by_key(err_key);
      if (!e) return false;
      cfg.stat_error = e->mask;
    }
    changed = true; persist_cfg = true;
  } else if (!strcasecmp(topic_suffix, "stat_error")){
    if (!mqtt_parse_u16_text(payload, 0, 65535, &u16)) return false;
    cfg.stat_error = u16;
    changed = true; persist_cfg = true;
  } else {
    return false;
  }

  if (changed){
    apply_cfg_to_state();
    if (persist_cfg) schedule_cfg_save();
#if MQTT_ENABLE
    g_mqtt_force_publish = true;
#endif
    return true;
  }

  (void)publish_now;
  return false;
}

static void web_apply_emulator_values(bool persist){
  auto argOrEmpty = [](const char* name) -> String {
    return g_http.hasArg(name) ? g_http.arg(name) : String("");
  };

  auto argU16 = [&](const char* name, uint16_t defv, uint16_t minv, uint16_t maxv) -> uint16_t {
    if (!g_http.hasArg(name)) return defv;
    long v = argOrEmpty(name).toInt();
    return (uint16_t)CLAMP(v, minv, maxv);
  };

  auto argU8 = [&](const char* name, uint8_t defv, uint8_t minv, uint8_t maxv) -> uint8_t {
    if (!g_http.hasArg(name)) return defv;
    long v = argOrEmpty(name).toInt();
    return (uint8_t)CLAMP(v, minv, maxv);
  };

  cfg.suctionLevel  = argU8 ("suctionLevel",  cfg.suctionLevel,  CL_SUCTIONLEVEL_MIN,  CL_SUCTIONLEVEL_MAX);
  cfg.selectFlow    = selectflow_pct_to_raw(argU16("selectFlow", selectflow_raw_to_pct(cfg.selectFlow), 0, 100));
  cfg.tstop_work    = argU16("tstop_work",    cfg.tstop_work,    CL_TSTOP_WORK_MIN,    CL_TSTOP_WORK_MAX);
  cfg.tstop_stand   = argU16("tstop_stand",   cfg.tstop_stand,   CL_TSTOP_STAND_MIN,   CL_TSTOP_STAND_MAX);
  cfg.stand_intakes = g_http.hasArg("stand_intakes") ? 1 : 0;
  cfg.filter_life   = filter_pct_to_raw(argU16("filter_life", filter_raw_to_pct(cfg.filter_life), 0, 100));
  cfg.filter_sat    = filter_pct_to_raw(argU16("filter_sat",  filter_raw_to_pct(cfg.filter_sat),  0, 100));

  if (g_http.hasArg("error_code")) {
    String err = g_http.arg("error_code");
    if (err.equalsIgnoreCase("NOERR")) cfg.stat_error = 0;
    else {
      const ErrInfo* e = find_errinfo_by_key(err);
      cfg.stat_error = e ? e->mask : 0;
    }
  } else {
    cfg.stat_error = argU16("stat_error", cfg.stat_error, 0, 65535);
  }

  st.flow_x1000     = actualflow_pct_to_raw(argU16("actual_flow", actualflow_raw_to_pct(st.flow_x1000), 0, 100));
  st.speed_rpm      = argU16("speed",         st.speed_rpm,      CL_SPEED_MIN,         CL_SPEED_MAX);
  g_led_brightness  = led_brightness_pct_to_raw(argU16("led_brightness", led_brightness_raw_to_pct(g_led_brightness), 10, 100));

  uint8_t new_relay_inverted = argU8("relay_inverted", g_relay_inverted, 0, 1);
  runtime_set_relay_inverted(new_relay_inverted != 0, persist);

  apply_cfg_to_state();
  if (persist) { schedule_cfg_save(); schedule_led_brightness_save(); }

  uint8_t new_bus_count = argU8("bus_count", g_bus_count, 1, MAX_BUSES);
  apply_buscount(new_bus_count, persist);

  runtime_set_continuous(g_http.hasArg("continuous"));
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
  if (e == 0){ DBG.println(F("  (keine Fehler)")); return; }
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

  if(argc == 1 && argv[0][0] == '?' && argv[0][1] == '\0'){ print_startup_banner(); return; }
  if(!strcasecmp(argv[0],"help")){ cli_show_help(); return; }
  if(!strcasecmp(argv[0],"show") && argc>=2 && !strcasecmp(argv[1],"cfg")){ cli_show_cfg(); return; }
  if(!strcasecmp(argv[0],"show") && argc>=2 && !strcasecmp(argv[1],"buses")){
    DBG.print(F("[BUS] active buses = ")); DBG.println(g_bus_count);
    return;
  }
  if(!strcasecmp(argv[0],"show") && argc>=2 && !strcasecmp(argv[1],"dbg")){
    cli_show_dbg();
    return;
  }
  if(!strcasecmp(argv[0],"show") && argc>=2 && !strcasecmp(argv[1],"net")){
    cli_show_net();
    return;
  }
  if((argc == 1 && (!strcasecmp(argv[0],"quiet") || !strcasecmp(argv[0],"silence"))) ||
     (argc == 2 && !strcasecmp(argv[0],"dbg") && (!strcasecmp(argv[1],"none") || !strcasecmp(argv[1],"quiet"))) ||
     (argc == 3 && !strcasecmp(argv[0],"dbg") && !strcasecmp(argv[1],"all") &&
      (!strcasecmp(argv[2],"off") || !strcasecmp(argv[2],"0") || !strcasecmp(argv[2],"false") || !strcasecmp(argv[2],"no")))){
    dbg_set_all(false);
    dbg_state_period_ms = 0;
    dbg_reset_runtime_filters();
    DBG.println(F("[CLI] dbg all off"));
    return;
  }
  if(!strcasecmp(argv[0],"dbg")){
    if(argc == 1){ cli_show_dbg(); return; }

    if(!strcasecmp(argv[1],"state_mode")){
      if(argc < 3){ DBG.println(F("[CLI] usage: dbg state_mode <write|periodic>")); return; }
      if(!strcasecmp(argv[2], "write")){ dbg_state_on_write_only = true; DBG.println(F("[CLI] ok")); return; }
      if(!strcasecmp(argv[2], "periodic")){ dbg_state_on_write_only = false; DBG.println(F("[CLI] ok")); return; }
      DBG.println(F("[CLI] invalid state_mode (write|periodic)"));
      return;
    }

    if(!strcasecmp(argv[1],"state_period")){
      if(argc < 3){ DBG.println(F("[CLI] usage: dbg state_period <ms>")); return; }
      bool ok=false; long v=cli_parse_long(argv[2], &ok);
      if(!ok || v < 0){ DBG.println(F("[CLI] invalid state_period")); return; }
      dbg_state_period_ms = (uint32_t)v;
      DBG.println(F("[CLI] ok"));
      return;
    }

    if(!strcasecmp(argv[1],"rate")){
      if(argc < 3){ DBG.println(F("[CLI] usage: dbg rate <ms>")); return; }
      bool ok=false; long v=cli_parse_long(argv[2], &ok);
      if(!ok || v < 0){ DBG.println(F("[CLI] invalid rate")); return; }
      dbg_rxtx_min_interval_ms = (uint32_t)v;
      DBG.println(F("[CLI] ok"));
      return;
    }

    if(!strcasecmp(argv[1],"payloadfmt")){
      if(argc < 3){ DBG.println(F("[CLI] usage: dbg payloadfmt <hex|dec|ascii|auto>")); return; }
      uint8_t fmt = dbg_payload_fmt;
      if(!dbg_payload_fmt_from_text(argv[2], &fmt)){
        DBG.println(F("[CLI] invalid payloadfmt (hex|dec|ascii|auto)"));
        return;
      }
      dbg_payload_fmt = fmt;
      DBG.print(F("[CLI] payloadfmt="));
      DBG.println(dbg_payload_fmt_name(dbg_payload_fmt));
      return;
    }

    if(!strcasecmp(argv[1],"ts")){
      if(argc < 3){ DBG.println(F("[CLI] usage: dbg ts <0|1|2>")); return; }
      bool ok=false; long v=cli_parse_long(argv[2], &ok);
      if(!ok || v < 0 || v > 2){ DBG.println(F("[CLI] invalid ts mode (0|1|2)")); return; }
      dbg_ts_mode = (uint8_t)v;
      DBG.println(F("[CLI] ok"));
      return;
    }

    if(!strcasecmp(argv[1], "none") || !strcasecmp(argv[1], "quiet")) {
      dbg_set_all(false);
      dbg_reset_runtime_filters();
      DBG.println(F("[CLI] dbg all off"));
      return;
    }

    if(argc < 3){ DBG.println(F("[CLI] usage: dbg <rx|tx|proto|err|state|all> <on|off>")); return; }
    bool ok=false; bool en=cli_parse_onoff(argv[2], &ok);
    if(!ok){ DBG.println(F("[CLI] invalid value (use on/off or 1/0)")); return; }

    if(!strcasecmp(argv[1], "rx"))    { dbg_show_rx = en;    DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1], "tx"))    { dbg_show_tx = en;    DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1], "proto")) { dbg_show_proto = en; DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1], "err"))   { dbg_show_err = en;   DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1], "state")) { dbg_show_state = en; DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1], "all")) {
      dbg_set_all(en);
      if (!en) dbg_state_period_ms = 0;
      dbg_reset_runtime_filters();
      DBG.print(F("[CLI] dbg all "));
      DBG.println(en ? F("on") : F("off"));
      return;
    }

    DBG.println(F("[CLI] unknown dbg key"));
    return;
  }
  if(!strcasecmp(argv[0],"clearweb") || !strcasecmp(argv[0],"webclear") || !strcasecmp(argv[0],"clearhttp")){
    g_netcfg.web_username[0] = 0;
    g_netcfg.web_password[0] = 0;
    netcfg_save();
    DBG.println(F("[CLI] Web username/password cleared and saved."));
    return;
  }

  if(!strcasecmp(argv[0],"save")){
    persist_save_cfg();
    persist_save_buscount(g_bus_count);
    persist_save_led_brightness(g_led_brightness);
    DBG.println(F("[CLI] cfg + buses + LED brightness saved."));
    return;
  }

  if(!strcasecmp(argv[0],"load")){
    if(persist_load_cfg()){ apply_cfg_to_state(); DBG.println(F("[CLI] cfg loaded & applied.")); }
    else { DBG.println(F("[CLI] no cfg stored.")); }
    g_led_brightness = persist_load_led_brightness();
    uint8_t bc = persist_load_buscount();
    apply_buscount(bc, /*save=*/false);
    return;
  }

  if(!strcasecmp(argv[0],"erase") || !strcasecmp(argv[0],"factory") || !strcasecmp(argv[0],"wipe")){
    persist_erase_all();
    apply_buscount(BUS_COUNT, /*save=*/false);

    deviceid_init_defaults();
    for (uint8_t i = 0; i < MAX_BUSES; ++i){
      g_bus[i].persist_devid_status = 0;
      g_bus[i].persist_addr_status  = 0;
      g_bus[i].my_addr = 0x91;
      g_bus[i].stn_addr = 0x00;
      g_bus[i].addr_locked = false;
    }

    init_cfg_defaults();
    apply_cfg_to_state();
    g_led_brightness = LED_BRIGHTNESS_MAX;

    st.statError = 0;
    g_relay_inverted = (RELAY_ACTIVE_LOW ? 1 : 0);
    relay_hw_write(g_relay_on);
    g_relay_off_deadline = 0;
    intake_sensor_clear_all();
    memset(g_bus_intakeAct, DEF_ACTIVATION, sizeof(g_bus_intakeAct));
    g_afterrun_owner = -1;
    g_relay_owner = -1;
    g_work_mask = 0;
    relay_set(false);

#if LED_ENABLE
    for(uint8_t idx=0; idx<LED_COUNT_MAX; ++idx) set_led_idx(idx,0,0,0,0);
    safe_pixels_show();
#endif

    DBG.println(F("[ERASE] persistence wiped. Defaults reloaded (use 'save' to persist)."));
    return;
  }

  if(!strcasecmp(argv[0],"set") && argc>=3){
    if(!strcasecmp(argv[1],"continuous")){
      bool ok=false; bool en=cli_parse_onoff(argv[2], &ok);
      if(!ok){ DBG.println(F("[CLI] invalid value (use on/off or 1/0)")); return; }
      runtime_set_continuous(en);
      DBG.println(F("[CLI] ok"));
      return;
    }
    if(!strcasecmp(argv[1],"relay_mode")){
      if(!strcasecmp(argv[2],"no") || !strcasecmp(argv[2],"0")){
        runtime_set_relay_inverted(false, true);
        DBG.println(F("[CLI] ok"));
        return;
      }
      if(!strcasecmp(argv[2],"nc") || !strcasecmp(argv[2],"1")){
        runtime_set_relay_inverted(true, true);
        DBG.println(F("[CLI] ok"));
        return;
      }
      DBG.println(F("[CLI] invalid relay_mode (use no|nc)"));
      return;
    }

    bool ok=false; long v=cli_parse_long(argv[2],&ok);
    if(!ok){ DBG.println(F("[CLI] invalid number")); return; }
    if(!strcasecmp(argv[1],"buses")){
      if(v<1 || v>2){ DBG.println(F("[CLI] invalid buses (use 1 or 2)")); return; }
      apply_buscount((uint8_t)v, /*save=*/false);
      DBG.println(F("[CLI] ok"));
      return;
    }
    if(!strcasecmp(argv[1],"tstop_work"))    { cfg.tstop_work     = (uint16_t)CLAMP(v, CL_TSTOP_WORK_MIN,   CL_TSTOP_WORK_MAX);   apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"tstop_stand"))   { cfg.tstop_stand    = (uint16_t)CLAMP(v, CL_TSTOP_STAND_MIN,  CL_TSTOP_STAND_MAX);  apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"suctionlevel"))  { cfg.suctionLevel   = (uint8_t) CLAMP(v, CL_SUCTIONLEVEL_MIN, CL_SUCTIONLEVEL_MAX); apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"selectflow"))    { cfg.selectFlow     = selectflow_pct_to_raw((uint16_t)CLAMP(v, 0, 100)); apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"actual_flow"))   { st.flow_x1000      = actualflow_pct_to_raw((uint16_t)CLAMP(v, 0, 100)); mark_state_dirty(); maybe_dump_state_on_write(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"speed"))         { st.speed_rpm       = (uint16_t)CLAMP(v, CL_SPEED_MIN,        CL_SPEED_MAX);        mark_state_dirty(); maybe_dump_state_on_write(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"pedal_act"))     { cfg.pedal_act      = (uint8_t) CLAMP(v, CL_PEDAL_ACT_MIN,    CL_PEDAL_ACT_MAX);    apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"pedal_mode"))    { cfg.pedal_mode     = (uint8_t) CLAMP(v, CL_PEDAL_MODE_MIN,   CL_PEDAL_MODE_MAX);   apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"stand_intakes")) { cfg.stand_intakes  = (uint8_t) CLAMP(v, CL_STAND_INTAKES_MIN,CL_STAND_INTAKES_MAX);apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"filter_life"))   { cfg.filter_life    = filter_pct_to_raw((uint16_t)CLAMP(v, 0, 100)); apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"filter_sat"))    { cfg.filter_sat     = filter_pct_to_raw((uint16_t)CLAMP(v, 0, 100)); apply_cfg_to_state(); DBG.println(F("[CLI] ok")); return; }
    if(!strcasecmp(argv[1],"led_brightness")) { g_led_brightness    = led_brightness_pct_to_raw((uint16_t)CLAMP(v, 10, 100)); DBG.println(F("[CLI] ok")); return; }
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
  static bool last_was_cr = false;
  while(Serial.available()){
    char c=(char)Serial.read();

    if(c=='\r' || c=='\n'){
      if(c=='\n' && last_was_cr){
        last_was_cr = false;
        continue;
      }
      last_was_cr = (c=='\r');
      cli_buf[cli_len]=0;
      if(cli_len>0) cli_handle_line(cli_buf);
      cli_len=0;
      DBG.print(F("> "));
      continue;
    }
    last_was_cr = false;

    if(cli_len==0 && (c==BANNER_HOTKEY1 || c==BANNER_HOTKEY2 || c==BANNER_HOTKEY3)){
      print_startup_banner();
      continue;
    }
    if(cli_len+1<sizeof(cli_buf)) cli_buf[cli_len++]=c;
  }
}

// ===== State-Ansicht =========================================================
static void dump_state(){
  if(!dbg_show_state) return;
  DBG.print(F("[STATE] SL=")); DBG.print(st.suctionLevel);
  DBG.print(F(" SF=")); DBG.print(st.selectFlow);
  DBG.print(F(" | Filter=Life ")); DBG.print(filter_raw_to_pct(st.filterLife)); DBG.print(F("%"));
  DBG.print(F("/Sat ")); DBG.print(filter_raw_to_pct(st.filterSaturation)); DBG.print(F("%"));
  DBG.print(F(" Err=")); DBG.print(st.statError);
  DBG.print(F(" | Relay=")); print_onoff_icon(g_relay_on);
  DBG.print(F(" | bus0 act/af=")); DBG.print(bus_active(0) ? 1 : 0); DBG.print('/'); DBG.print(bus_afterrun(0) ? 1 : 0);
  DBG.print(F(" bus1 act/af=")); DBG.print(bus_active(1) ? 1 : 0); DBG.print('/'); DBG.print(bus_afterrun(1) ? 1 : 0);

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
    case M_R_FILTERSTATUS: return FS_LIT("M_R_FILTERLIFE");
    case M_R_CONNECTEDPEDAL: return FS_LIT("M_R_CONNECTEDPEDAL");
    case M_R_CONTINUOUSSUCTION: return FS_LIT("M_R_CONTINUOUSSUCTION");
    case M_W_CONTINUOUSSUCTION: return FS_LIT("M_W_CONTINUOUSSUCTION");
    case M_R_USB_CONNECTSTATUS: return FS_LIT("M_R_USB_CONNECTSTATUS");
    case M_W_USB_CONNECTSTATUS: return FS_LIT("M_W_USB_CONNECTSTATUS");
    case M_R_FILTERSAT: return FS_LIT("M_R_FILTERSATURATION");
    case M_R_STATERROR: return FS_LIT("M_R_STATIONERROR");
    default: return nullptr;
  }
}
static inline void print_ctrl_name(uint8_t /*fid*/, uint8_t ctrl){
  if(!(dbg_show_rx || dbg_show_tx)) return;
  flashstr_t n = fe02_ctrl_name(ctrl);
  if(n){ DBG.print(F(" (")); DBG.print(n); DBG.print(')'); }
}

static inline DbgRateState& dbg_rate_state(uint8_t i, bool is_tx){
  return is_tx ? g_dbg_tx_rate[i] : g_dbg_rx_rate[i];
}

static inline bool dbg_rxtx_should_log(uint8_t i, bool is_tx){
  if (dbg_rxtx_min_interval_ms == 0) return true;
  DbgRateState &st = dbg_rate_state(i, is_tx);
  const uint32_t now = millis();
  if (st.last_ms != 0 && (uint32_t)(now - st.last_ms) < dbg_rxtx_min_interval_ms) {
    st.suppressed++;
    return false;
  }
  st.last_ms = now;
  return true;
}

static inline uint32_t dbg_rxtx_take_suppressed(uint8_t i, bool is_tx){
  DbgRateState &st = dbg_rate_state(i, is_tx);
  const uint32_t suppressed = st.suppressed;
  st.suppressed = 0;
  return suppressed;
}

static inline DbgCtrlRepeatState* dbg_ctrl_repeat_table(uint8_t i, bool is_tx){
  return is_tx ? g_dbg_tx_repeat[i] : g_dbg_rx_repeat[i];
}

static inline DbgCtrlRepeatState& dbg_ctrl_repeat_state(uint8_t i, bool is_tx, uint8_t src, uint8_t dst, uint8_t ctrl, uint8_t len_field, uint32_t key_hash){
  DbgCtrlRepeatState* table = dbg_ctrl_repeat_table(i, is_tx);
  DbgCtrlRepeatState* empty = nullptr;
  DbgCtrlRepeatState* oldest = &table[0];

  for (uint8_t s = 0; s < DBG_CTRL_REPEAT_SLOTS; s++) {
    DbgCtrlRepeatState* st = &table[s];
    if (!st->valid) {
      if (!empty) empty = st;
      continue;
    }

    if (st->src == src && st->dst == dst && st->ctrl == ctrl && st->len == len_field && st->key_hash == key_hash) {
      return *st;
    }

    if (!oldest->valid || st->last_seen_ms < oldest->last_seen_ms) oldest = st;
  }

  if (empty) return *empty;
  return *oldest;
}

static inline bool dbg_should_hide_intake_poll(uint8_t ctrl){
  return !dbg_show_intake_poll && ctrl == jbc_cmd::FE_02::M_R_INTAKEACTIVATION;
}

static inline bool dbg_should_repeat_suppress_ctrl(uint8_t ctrl){
  using namespace jbc_cmd::FE_02;
  // Nur echte zyklische Read-/Poll-Frames unterdrücken.
  // Write-/Set-Kommandos sollen immer sichtbar bleiben, auch wenn ein Wert
  // später wieder auf einen früheren Zustand zurückfällt.
  switch (ctrl) {
    case M_SYN:
    case M_FIRMWARE:
    case M_R_DEVICEID:
    case M_R_DEVICEIDORIGINAL:
    case M_R_DISCOVER:
    case M_R_SUCTIONLEVEL:
    case M_R_FLOW:
    case M_R_SPEED:
    case M_R_SELECTFLOW:
    case M_R_STANDINTAKES:
    case M_R_INTAKEACTIVATION:
    case M_R_SUCTIONDELAY:
    case M_R_DELAYTIME:
    case M_R_ACTIVATIONPEDAL:
    case M_R_PEDALMODE:
    case M_R_FILTERSTATUS:
    case M_R_CONNECTEDPEDAL:
    case M_R_CONTINUOUSSUCTION:
    case M_R_USB_CONNECTSTATUS:
    case M_R_FILTERSAT:
    case M_R_STATERROR:
      return true;

    case M_HS:
    case M_EOT:
    case M_ACK:
    case M_NACK:
    case M_W_DEVICEID:
    case M_RESET:
    case M_CLEARMEMFLASH:
    case M_SENDMEMADDRESS:
    case M_SENDMEMDATA:
    case M_ENDPROGR:
    case M_ENDUPD:
    case M_CONTINUEUPD:
    case M_CLEARING:
    case M_FORCEUPDATE:
    case M_W_SUCTIONLEVEL:
    case M_W_SELECTFLOW:
    case M_W_STANDINTAKES:
    case M_W_INTAKEACTIVATION:
    case M_W_SUCTIONDELAY:
    case M_W_ACTIVATIONPEDAL:
    case M_W_PEDALMODE:
    case M_W_CONTINUOUSSUCTION:
    case M_W_USB_CONNECTSTATUS:
      return false;

    default:
      return false;
  }
}

static inline uint32_t dbg_ctrl_repeat_before_log(uint8_t i, bool is_tx, uint8_t src, uint8_t dst, uint8_t fid, uint8_t ctrl, uint8_t len_field, const uint8_t* data){
  if (!dbg_should_repeat_suppress_ctrl(ctrl)) {
    return 0;
  }

  const uint32_t now = millis();
  const uint32_t payload_hash = dbg_payload_hash(len_field, data);
  const uint32_t key_hash = dbg_ctrl_repeat_key_hash(is_tx, ctrl, len_field, data);
  DbgCtrlRepeatState &st = dbg_ctrl_repeat_state(i, is_tx, src, dst, ctrl, len_field, key_hash);

  const bool same = st.valid &&
                    st.src == src &&
                    st.dst == dst &&
                    st.ctrl == ctrl &&
                    st.len == len_field &&
                    st.key_hash == key_hash &&
                    st.last_payload_hash == payload_hash;

  if (same) {
    st.suppressed++;
    st.last_fid = fid;
    st.last_seen_ms = now;
    return UINT32_MAX;
  }

  const uint32_t suppressed_before = same ? st.suppressed : 0;
  st.valid = true;
  st.src = src;
  st.dst = dst;
  st.ctrl = ctrl;
  st.len = len_field;
  st.last_fid = fid;
  st.key_hash = key_hash;
  st.last_payload_hash = payload_hash;
  st.suppressed = 0;
  st.last_seen_ms = now;
  return suppressed_before;
}

// ====== TX Helpers (pro Bus) ================================================
static inline void tx_base(uint8_t i, uint8_t b){ g_bus[i].ser->write(&b,1); }

static void service_bus_io_once(){
  for(uint8_t i=0;i<g_bus_count;i++){
    if (!g_bus[i].ser) continue;
    if(g_bus[i].baseState!=BASE_P02_ACTIVE) poll_rx_base(i);
    else                                   poll_rx_p02(i);
  }
}

static void cooperative_wait_ms(uint32_t ms){
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < ms){
    service_bus_io_once();
    delay(1);
  }
}

static inline bool send_base_syn(uint8_t i){
  const uint32_t now=millis();
  if(now - g_bus[i].t_last_syn < 1500) return false;
  tx_base(i, jbc_cmd::BASE::M_SYN);
  g_bus[i].t_last_syn=now;
  if (dbg_show_proto) DBG_PRINTF("[BASE b%u] TX SYN\n", i);
  return true;
}

static inline void send_base_ack(uint8_t i, const __FlashStringHelper* why){
  tx_base(i, jbc_cmd::BASE::M_ACK);
  if (dbg_show_proto) { DBG_PRINTF("[BASE b%u] TX ACK ", i); DBG.println(why); }
}

static void send_p02(uint8_t i, const uint8_t* payload, size_t n){
  uint8_t bcc = bcc_calc_xor(payload, n);
  static uint8_t txBuf[2 + BusCtx::MAX_PAY*2 + 4];
  size_t o = 0;
  txBuf[o++] = DLE; txBuf[o++] = STX;
  o += stuff_bytes(payload, n, txBuf + o, sizeof(txBuf) - o - 4);
  txBuf[o++] = bcc;
  if (bcc == DLE) txBuf[o++] = DLE;
  txBuf[o++] = DLE; txBuf[o++] = ETX;
  g_bus[i].ser->write(txBuf, o);

  if (dbg_show_tx && n >= 5) {
    const uint8_t src  = payload[0];
    const uint8_t dst  = payload[1];
    const uint8_t fid  = payload[2];
    const uint8_t ctrl = payload[3];
    const uint8_t len  = payload[4];
    const uint8_t* d   = (len && n >= (size_t)(5 + len)) ? (payload + 5) : nullptr;

    if (dbg_should_hide_intake_poll(ctrl)) return;
    const uint32_t repeat_suppressed_before = dbg_ctrl_repeat_before_log(i, true, src, dst, fid, ctrl, len, d);
    if (repeat_suppressed_before == UINT32_MAX) return;
    if (!dbg_rxtx_should_log(i, true)) return;
    const uint32_t suppressed_before = dbg_rxtx_take_suppressed(i, true);

    dbg_print_bus_header(F("TX"), i);
    DBG.print(F("src="));
    print_addr_with_name(src);
    DBG.print(F(" dst="));
    print_addr_with_name(dst);
    DBG.print(F(" fid=0x"));
    if (fid < 0x10) DBG.print('0');
    DBG.print(fid, HEX);
    DBG.print(F(" ctrl=0x"));
    if (ctrl < 0x10) DBG.print('0');
    DBG.print(ctrl, HEX);
    DBG.print(F(" len="));
    DBG.print(len);
    print_ctrl_name(fid, ctrl);
    dbg_print_payload_inline(d, len, ctrl);
    if (repeat_suppressed_before) {
      DBG.print(F("  [prev unchanged x"));
      DBG.print(repeat_suppressed_before);
      DBG.print(F("]"));
    }
    if (suppressed_before) {
      DBG.print(F("  [+"));
      DBG.print(suppressed_before);
      DBG.print(F(" rate-suppressed]"));
    }
    DBG.println();

    if (len && n >= (size_t)(5 + len)) {
      switch (ctrl) {
        case jbc_cmd::FE_02::M_R_SUCTIONLEVEL:
          if (len >= 1) DBG_PRINTF("        ↳ suctionLevel=%u\n", d[0]);
          break;
        case jbc_cmd::FE_02::M_R_SELECTFLOW:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ selectFlow=%u\n", v); }
          break;
        case jbc_cmd::FE_02::M_R_FLOW:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ flow_x1000=%u\n", v); }
          break;
        case jbc_cmd::FE_02::M_R_SPEED:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ speed_rpm=%u\n", v); }
          break;
        case jbc_cmd::FE_02::M_R_INTAKEACTIVATION:
          if (len == 3) DBG_PRINTF("        ↳ intakeAct=%u port=%u intake=%u\n", d[0], d[1], d[2]);
          break;
        case jbc_cmd::FE_02::M_R_SUCTIONDELAY:
          if (len == 4) { uint16_t secs = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ delaySec=%u port=%u intake=%u\n", secs, d[2], d[3]); }
          break;
        case jbc_cmd::FE_02::M_R_DELAYTIME:
          if (len == 4) { uint16_t ms = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ tstopMs=%u port=%u intake=%u\n", ms, d[2], d[3]); }
          break;
        case jbc_cmd::FE_02::M_R_ACTIVATIONPEDAL:
          if (len == 2) DBG_PRINTF("        ↳ pedalAct=%u port=%u\n", d[0], d[1]);
          break;
        case jbc_cmd::FE_02::M_R_PEDALMODE:
          if (len == 2) DBG_PRINTF("        ↳ pedalMode=%u port=%u\n", d[0], d[1]);
          break;
        case jbc_cmd::FE_02::M_R_FILTERSTATUS:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ filterLife=%u\n", v); }
          break;
        case jbc_cmd::FE_02::M_R_FILTERSAT:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ filterSaturation=%u\n", v); }
          break;
        case jbc_cmd::FE_02::M_R_CONNECTEDPEDAL:
          if (len == 2) DBG_PRINTF("        ↳ connectedPedal=%u port=%u\n", d[0], d[1]);
          break;
        case jbc_cmd::FE_02::M_R_CONTINUOUSSUCTION:
          if (len >= 1) DBG_PRINTF("        ↳ continuous=%u\n", d[0]);
          break;
        case jbc_cmd::FE_02::M_R_USB_CONNECTSTATUS:
          if (len >= 1) DBG_PRINTF("        ↳ usbConnect=%u\n", d[0]);
          break;
        case jbc_cmd::FE_02::M_R_STATERROR:
          if (len >= 2) { uint16_t v = (uint16_t)(d[0] | (d[1] << 8)); DBG_PRINTF("        ↳ statError=%u\n", v); }
          break;
        case jbc_cmd::FE_02::M_FIRMWARE:
          if (len >= 1) { DBG.print(F("        ↳ fw=")); print_ascii_quoted(d, len); DBG.println(); }
          break;
        case jbc_cmd::FE_02::M_R_DEVICEID:
          if (len >= 1) { DBG.print(F("        ↳ deviceId=")); print_ascii_quoted(d, len); DBG.println(); }
          break;
        default:
          break;
      }
    }
  }
}

static void send_hs_ack_p02_as(uint8_t i, uint8_t dst_node, uint8_t fid){
  uint8_t pl[] = { g_bus[i].my_addr, dst_node, fid, jbc_cmd::FE_02::M_HS, 0x01, 0x06 };
  send_p02(i, pl, sizeof(pl));
}
static void send_syn_p02(uint8_t i, uint8_t dst_node, uint8_t fid){
  uint8_t pl[] = { g_bus[i].my_addr, dst_node, fid, CTRL_SYN_P02, 0x01, 0x06 };
  send_p02(i, pl, sizeof(pl));
}

static void send_firmware_fe_as(uint8_t i, uint8_t src_node, uint8_t dst_node, uint8_t mirror_fid){
  const uint8_t len=(uint8_t)(sizeof(FE_FW_STR)-1);
  uint8_t pl[5+64]; size_t o=0;
  pl[o++]=src_node; pl[o++]=dst_node; pl[o++]=mirror_fid; pl[o++]=jbc_cmd::FE_02::M_FIRMWARE; pl[o++]=len;
  memcpy(&pl[o], FE_FW_STR, len);
  o+=len;
  send_p02(i, pl,o);
}

static void send_deviceid_fe_as(uint8_t i, uint8_t src_node, uint8_t dst_node, uint8_t mirror_fid){
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
    case M_R_INTAKEACTIVATION:{ uint8_t port=(len_field>=1&&data)?data[0]:0; uint8_t in=(len_field>=2&&data)?data[1]:0; if(port>=MAX_PORTS) port=0; if(in>=NUM_INTAKES) in=0; out[0]=g_bus_intakeAct[i][port][in]; out[1]=port; out[2]=in; olen=3; } break;
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

static inline void fe_send_ack_as(uint8_t i, uint8_t dst, uint8_t fid, uint8_t code=0x06){
  uint8_t pl[] = { g_bus[i].my_addr, dst, fid, jbc_cmd::FE_02::M_ACK, 0x01, code };
  send_p02(i, pl, sizeof(pl));
}

static bool handle_special_ctrl(uint8_t i, uint8_t src, uint8_t dst, uint8_t fid, uint8_t ctrl, uint8_t reply_to){
  using namespace jbc_cmd::FE_02;

  // M_HS muss auch dann angenommen werden, wenn wir unsere Adresse nach ERASE
  // noch nicht kennen. Sonst bleibt my_addr=0x91 und der Handshake kommt nie hoch.
  if (ctrl == M_HS) {
    update_station_addr(i, src);   // src kann hier 0x00 sein -> bleibt dann UNKNOWN

    if (!g_bus[i].addr_locked && dst != 0x00) {
      g_bus[i].my_addr = dst;
      persist_save_addr(i, g_bus[i].my_addr);
      g_bus[i].addr_locked = true;

      if (dbg_show_proto) {
        dbg_print_bus_header(F("ADDR"), i);
        DBG.print(F("adopt "));
        print_addr_with_name(g_bus[i].my_addr);
        DBG.print(F(" via ctrl M_HS fid=0x"));
        if (fid < 0x10) DBG.print('0');
        DBG.print(fid, HEX);
        DBG.print(F(" | peer="));
        print_addr_with_name(g_bus[i].stn_addr);
        DBG.println();
      }
    }

    send_hs_ack_p02_as(i, reply_to, fid);

    if (dst != 0x00) {
      if (g_bus[i].linkState == LINK_DOWN) g_bus[i].linkState = LINK_CONNECTING;
      else g_bus[i].linkState = LINK_UP;
    }
    return true;
  }

  if (!is_for_us(i, dst)) return false;

  update_station_addr(i, src);

  switch (ctrl){
    case CTRL_SYN_P02:
      send_syn_p02(i, reply_to, fid);
      g_bus[i].linkState = LINK_UP;
      if (dbg_show_proto && !(dbg_show_rx || dbg_show_tx)) {
        dbg_print_bus_header(F("P02"), i);
        DBG.print(F("RX SYN ctrl, mirrored fid=0x"));
        if (fid < 0x10) DBG.print('0');
        DBG.print(fid, HEX);
        DBG.println();
      }
      return true;

    case M_FIRMWARE: {
      uint8_t src_use = reply_src_for(i, dst);
      send_firmware_fe_as(i, src_use, reply_to, fid);
      g_bus[i].linkState = LINK_UP;
      return true;
    }

    case M_R_DEVICEID: {
      uint8_t src_use = reply_src_for(i, dst);
      send_deviceid_fe_as(i, src_use, reply_to, fid);
      g_bus[i].linkState = LINK_UP;
      return true;
    }

    default:
      return false;
  }
}

static bool handle_fe_like_write(uint8_t i, uint8_t req_dst, uint8_t reply_to, uint8_t fid, uint8_t ctrl, const uint8_t* data, uint8_t len_field){
  using namespace jbc_cmd::FE_02;
  (void)reply_src_for(i, req_dst);

  switch (ctrl) {
    case M_W_SUCTIONLEVEL:
      if (len_field>=1 && data){
        uint8_t v = data[0]; if (v > 3) v = 3;
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
        cfg.stand_intakes = data[0] ? 1 : 0;
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
          if (val) intake_sensor_refresh(i, in);
          uint8_t prev = g_bus_intakeAct[i][port][in];
          fe_send_ack_as(i, reply_to, fid);

          if (in == WORK_IDX) {
            uint8_t before = g_work_mask;
            const bool was_owner = (g_relay_owner == (int8_t)i);
            const bool rising = (prev == 0 && val != 0);
            if (val) {
              work_set(i, true);
              if (rising && (g_relay_owner < 0 || !bus_active((uint8_t)g_relay_owner) || g_relay_off_deadline != 0)) {
                g_relay_owner = i;
              }
              g_afterrun_owner = -1;
              g_relay_off_deadline = 0;
              relay_set(true);
            } else {
              work_set(i, false);
              if (before && g_work_mask == 0 && !g_relay_off_deadline) {
                g_relay_owner = -1;
                g_afterrun_owner = i;
                uint16_t d = st.delaySec[port][WORK_IDX];
                g_relay_off_deadline = millis() + (uint32_t)d * 1000UL;
                if (dbg_show_state) { dbg_time_prefix(); dbg_print_bus_header(F("RELAY"), i); DBG_PRINTF("schedule OFF in %us\n", d); }
              } else if (was_owner) {
                g_relay_owner = pick_active_bus_owner();
              }
            }
          }
          if (prev != val){
            g_bus_intakeAct[i][port][in] = val;
            mark_state_dirty();
            maybe_dump_state_on_write();
          } else {
#if DBG_LOG_DUP_WRITES
            dbg_time_prefix();
            dbg_print_bus_header(F("RELAY"), i); DBG_PRINTF("duplicate M_W_INTAKEACTIVATION ignored (val=%u port=%u intake=%u)\n", val, port, in);
#endif
          }
          return true;
        }
      }
      break;

    case M_W_SUCTIONDELAY:
      if (len_field>=4 && data){
        uint16_t v = (uint16_t)(data[0] | (data[1]<<8));
        uint8_t  port = data[2];
        uint8_t  in = data[3];
        if (port >= MAX_PORTS || in >= NUM_INTAKES) break;
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
        if (data[1] >= MAX_PORTS || data[0] > CL_PEDAL_ACT_MAX) break;
        cfg.pedal_act = data[0];
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_PEDALMODE:
      if (len_field>=2 && data){
        if (data[1] >= MAX_PORTS || data[0] > CL_PEDAL_MODE_MAX) break;
        cfg.pedal_mode = data[0];
        fe_send_ack_as(i, reply_to, fid);
        apply_cfg_to_state();
        schedule_cfg_save();
        return true;
      }
      break;

    case M_W_CONTINUOUSSUCTION:
      if (len_field>=1 && data){
        fe_send_ack_as(i, reply_to, fid);
        runtime_set_continuous(data[0] ? true : false);
        return true;
      }
      break;

    case M_W_USB_CONNECTSTATUS:
      if (len_field>=1 && data){
        fe_send_ack_as(i, reply_to, fid);
        runtime_set_usb_connect(data[0] ? true : false);
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
        if (dbg_show_tx || dbg_show_rx || dbg_show_state) { dbg_time_prefix(); DBG_PRINTF("[ID bus%u] updated len=%u ascii=", i, g_bus[i].dev_id_len); print_ascii_quoted(g_bus[i].dev_id, g_bus[i].dev_id_len); DBG.println(); }
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
  if(n < 5) return;

  const uint8_t len_field = f[4];
  if (n != (size_t)(5 + len_field)) {
    if (dbg_show_err) DBG_PRINTF("[ERR b%u] frame length mismatch actual=%u declared=%u\n",
                                 i, (unsigned)(n - 5), len_field);
    return;
  }

  const uint8_t src = f[0];
  const uint8_t dst = f[1];
  const uint8_t fid = f[2];
  const uint8_t ctrl = f[3];
  const uint8_t* data = len_field ? (f + 5) : nullptr;
  const uint8_t reply_to = (src == 0x00) ? 0x00 : src;

  bool for_us = is_for_us(i, dst);
  const uint32_t now_ms = millis();

  if (dbg_show_rx) {
    if (dbg_should_hide_intake_poll(ctrl)) goto rx_log_done;
    {
      const uint32_t repeat_suppressed_before = dbg_ctrl_repeat_before_log(i, false, src, dst, fid, ctrl, len_field, data);
      if (repeat_suppressed_before == UINT32_MAX) goto rx_log_done;
      if (!dbg_rxtx_should_log(i, false)) goto rx_log_done;
      const uint32_t suppressed_before = dbg_rxtx_take_suppressed(i, false);
      dbg_print_bus_header(F("RX"), i);
      DBG.print(F("src="));
    print_addr_with_name(src);
    DBG.print(F(" dst="));
    print_addr_with_name(dst);
    DBG.print(F(" fid=0x"));
    if (fid < 0x10) DBG.print('0');
    DBG.print(fid, HEX);
    DBG.print(F(" ctrl=0x"));
    if (ctrl < 0x10) DBG.print('0');
    DBG.print(ctrl, HEX);
    DBG.print(F(" len="));
      DBG.print(len_field);
      print_ctrl_name(fid, ctrl);
      dbg_print_payload_inline(data, len_field, ctrl);
      if (repeat_suppressed_before) {
        DBG.print(F("  [prev unchanged x"));
        DBG.print(repeat_suppressed_before);
        DBG.print(F("]"));
      }
      if (suppressed_before) {
        DBG.print(F("  [+"));
        DBG.print(suppressed_before);
        DBG.print(F(" rate-suppressed]"));
      }
      DBG.println();
    }
  }
rx_log_done:

  // Spezialbefehle ausschließlich über CTRL behandeln, FID nur spiegeln
  if (handle_special_ctrl(i, src, dst, fid, ctrl, reply_to)) {
    return;
  }

  // nach möglicher Adressübernahme durch M_HS nochmal prüfen
  for_us = is_for_us(i, dst);

  if (for_us) {
    bool was_up = (g_bus[i].linkState == LINK_UP);
    g_bus[i].linkState = LINK_UP;
    g_bus[i].last_for_us_ms = now_ms;
    if (!was_up) g_bus[i].t_last_intake_ms = now_ms;
    update_station_addr(i, src);
  }

  if (!for_us) return;

  if (is_intake_activation_ctrl(ctrl)) intake_touch(i);

  if (handle_fe_like_write(i, dst, reply_to, fid, ctrl, data, len_field)) return;
  handle_fe_like_read(i, dst, reply_to, fid, ctrl, data, len_field);
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
              if(!b.have_len && b.payLen==5){ b.data_len=b.payBuf[4]; b.have_len=true; }
              if(b.have_len && b.payLen == (size_t)(5 + b.data_len + 1)){
                uint8_t bcc_rx=b.payBuf[b.payLen-1];
                size_t n_no_bcc=5+b.data_len;
                uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
                if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
                else if (dbg_show_err) DBG_PRINTF("[ERR b%u] BCC mismatch rx=%02X calc=%02X\n", i, bcc_rx, bcc_cmp);
                b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
              }
            } else {
              if (dbg_show_err) DBG_PRINTF("[ERR b%u] payload overflow\n", i);
              b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
            }
          }
        } else {
          if(by==DLE){
            if(b.payLen < BusCtx::MAX_PAY){
              b.payBuf[b.payLen++]=DLE;
              if(!b.have_len && b.payLen==5){ b.data_len=b.payBuf[4]; b.have_len=true; }
              if(b.have_len && b.payLen==(size_t)(5+b.data_len+1)){
                uint8_t bcc_rx=b.payBuf[b.payLen-1];
                size_t n_no_bcc=5+b.data_len;
                uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
                if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
                else if (dbg_show_err) DBG_PRINTF("[ERR b%u] BCC mismatch rx=%02X calc=%02X\n", i, bcc_rx, bcc_cmp);
                b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
              }
            } else {
              if (dbg_show_err) DBG_PRINTF("[ERR b%u] payload overflow\n", i);
              b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
            }
            b.in_escape=false;
          }
          else if(by==ETX){
            if(!(b.have_len && b.payLen>=(size_t)(5+b.data_len+1)) && b.payLen>=6){
              uint8_t bcc_rx=b.payBuf[b.payLen-1];
              size_t n_no_bcc=b.payLen-1;
              uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
              if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
            }
            b.rxState=WAIT_DLE; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
          }
          else if(by==STX){
            if(!(b.have_len && b.payLen==(size_t)(5+b.data_len+1)) && b.payLen>=6){
              uint8_t bcc_rx=b.payBuf[b.payLen-1];
              size_t n_no_bcc=b.payLen-1;
              uint8_t bcc_cmp=bcc_calc_xor(b.payBuf,n_no_bcc);
              if(bcc_rx==bcc_cmp) handle_frame(i,b.payBuf,n_no_bcc);
            }
            b.rxState=IN_FRAME; b.payLen=0; b.in_escape=false; b.have_len=false; b.data_len=0;
          }
          else {
            if (dbg_show_err) DBG_PRINTF("[ERR b%u] invalid DLE-sequence byte=0x%02X\n", i, by);
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
      if (dbg_show_proto) DBG_PRINTF("[BASE b%u] DLE -> P02\n", i);
      return;
    }
    switch(b.baseState){
      case BASE_IDLE:
        if(by==jbc_cmd::BASE::M_NACK){ b.baseState=BASE_SEEN_NAK;
          if (dbg_show_proto) DBG_PRINTF("[BASE b%u] RX NAK\n", i);
        }
        break;
      case BASE_SEEN_NAK:
        if(by==jbc_cmd::BASE::M_NACK){ }
        if (send_base_syn(i)) b.baseState=BASE_SENT_SYN;
        break;
      case BASE_SENT_SYN:
        if(by==jbc_cmd::BASE::M_ACK){ b.baseState=BASE_GOT_ACK1;
          if (dbg_show_proto) DBG_PRINTF("[BASE b%u] RX ACK1\n", i);
        }
        break;
      case BASE_GOT_ACK1:
        send_base_ack(i, F("(after ACK1)"));
        b.baseState=BASE_SENT_ACK2;
        break;
      case BASE_SENT_ACK2:
        if(by==0x01 /*SOH*/){ b.baseState=BASE_GOT_SOH;
          if (dbg_show_proto) DBG_PRINTF("[BASE b%u] RX SOH\n", i);
        }
        break;
      case BASE_GOT_SOH:
        send_base_ack(i, F("(after SOH)"));
        b.baseState=BASE_P02_ACTIVE; b.rxState=WAIT_DLE; b.linkState=LINK_CONNECTING;
        if (dbg_show_proto) DBG_PRINTF("[BASE b%u] -> P02 mode\n", i);
        return;
      case BASE_P02_ACTIVE:
        return;
    }
  }
}

// ===== LEDs (pro Bus 2 Stück) ===============================================
#if LED_ENABLE
static inline bool led_safe_to_show(){
  uint32_t now_us = micros();
  for (uint8_t i = 0; i < g_bus_count; i++){
    if (g_bus[i].ser && g_bus[i].ser->available() > 0) return false;
    if ((uint32_t)(now_us - g_bus[i].t_last_rx_us) < LED_SHOW_MIN_GAP_US) return false;
  }
  return true;
}
static inline void safe_pixels_show(){
  if(led_safe_to_show()) pixels.show();
}
static inline void leds_begin(){
  pixels.begin(); pixels.clear(); pixels.show();
}
static inline void set_led_idx(uint8_t idx, uint8_t r,uint8_t g,uint8_t b,uint8_t w){
  if(idx<LED_COUNT_MAX) pixels.setPixelColor(idx, pixels.Color(r,g,b,w));
}

static inline void leds_render_ota_white_fade(){
#if HTTP_ENABLE
  if (LED_COUNT_MAX == 0) return;
  const uint32_t now = millis();
  const uint8_t hi = (uint8_t)max<uint16_t>(16, g_led_brightness);
  const uint8_t mid = (uint8_t)max<uint16_t>(6, hi / 3);
  const uint8_t low = (uint8_t)max<uint16_t>(2, hi / 8);
  const uint8_t head = (uint8_t)((now / 320UL) % LED_COUNT_MAX);
  const uint8_t prev1 = (uint8_t)((head + LED_COUNT_MAX - 1) % LED_COUNT_MAX);
  const uint8_t prev2 = (uint8_t)((head + LED_COUNT_MAX - 2) % LED_COUNT_MAX);
  for(uint8_t idx = 0; idx < LED_COUNT_MAX; ++idx){
    uint8_t w = 0;
    if (idx == head) w = hi;
    else if (idx == prev1) w = mid;
    else if (idx == prev2) w = low;
    set_led_idx(idx, 0, 0, 0, w);
  }
#endif
}

static inline void leds_tick_ota_upload(bool force_show=false){
#if HTTP_ENABLE
  static uint32_t t_ota_last = 0;
  const uint32_t now = millis();
  if (!force_show && (uint32_t)(now - t_ota_last) < 120UL) return;
  t_ota_last = now;
  leds_render_ota_white_fade();
  if (force_show) pixels.show();
  else safe_pixels_show();
#else
  (void)force_show;
#endif
}

static void leds_render_bus(uint8_t i){
  const uint32_t now=millis();
  uint8_t linkIdx = i*2; uint8_t statIdx = i*2+1;
  if(fwloop_active(i)){
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2)==0;
    if(on) set_led_idx(linkIdx, g_led_brightness, g_led_brightness, 0, 0);
    else set_led_idx(linkIdx,0,0,0,0);
  }
  else {
    switch(g_bus[i].linkState){
      case LINK_DOWN:{
        uint8_t lv=breath_level(LED_BREATH_PERIOD_MS,2,g_led_brightness);
        set_led_idx(linkIdx, lv,0,0,0);
      } break;
      case LINK_CONNECTING:{
        bool on=((now/(LED_BLINK_PERIOD_MS/2))%2)==0;
        if(on) set_led_idx(linkIdx, g_led_brightness, g_led_brightness,0,0);
        else set_led_idx(linkIdx,0,0,0,0);
      } break;
      case LINK_UP:{
        set_led_idx(linkIdx,0,g_led_brightness,0,0);
      } break;
    }
  }
  const bool continuous_me = st.continuous && g_relay_on;
  if (continuous_me) {
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2) == 0;
    uint8_t v = on ? g_led_brightness : 0;
    set_led_idx(statIdx, 0, 0, v, 0);
    return;
  }

  const bool stopErr = has_stop_error(st.statError);
  const bool warnErr = has_warn_error(st.statError);
  if (stopErr) {
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2) == 0;
    set_led_idx(statIdx, on ? g_led_brightness : 0, 0, 0, 0);
    return;
  }
  if (warnErr) {
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2) == 0;
    uint8_t v = on ? g_led_brightness : 0;
    set_led_idx(statIdx, v, v, 0, 0);
    return;
  }

  if (g_bus[i].linkState != LINK_UP){
    set_led_idx(statIdx, 0, 0, 0, 0);
    return;
  }

  const bool active      = bus_active(i) && g_relay_on && (g_relay_off_deadline == 0);
  const bool afterrun_me = bus_afterrun(i);
  uint8_t lv = breath_level(LED_BREATH_PERIOD_MS, 2, g_led_brightness);

  if (active) {
    set_led_idx(statIdx, 0, 0, lv, 0);
    return;
  }
  if (afterrun_me) {
    bool on = ((now / (LED_BLINK_PERIOD_MS/2)) % 2) == 0;
    uint8_t v = on ? g_led_brightness : 0;
    if (on) set_led_idx(statIdx, v, 0, v, 0);
    else    set_led_idx(statIdx, 0, 0, 0, 0);
    return;
  }
  set_led_idx(statIdx, 0, 0, 0, lv);
}
static inline void leds_task(){
  uint32_t now=millis();
  if(now - t_led_last_render < 20) return;
  t_led_last_render=now;
#if HTTP_ENABLE
  if (g_ota_upload_active){
    leds_tick_ota_upload(false);
    return;
  }
#endif
  for(uint8_t i=0;i<g_bus_count;i++) leds_render_bus(i);
  for(uint8_t idx = g_bus_count*2; idx < LED_COUNT_MAX; ++idx) set_led_idx(idx,0,0,0,0);
  safe_pixels_show();
}
#endif



static bool cpu_idle_hook_0(void){
  g_cpu_idle_counter[0]++;
  return false;
}
#if defined(portNUM_PROCESSORS) && (portNUM_PROCESSORS > 1)
static bool cpu_idle_hook_1(void){
  g_cpu_idle_counter[1]++;
  return false;
}
#endif

static void cpu_load_init(){
  g_cpu_idle_counter[0] = 0;
  g_cpu_idle_counter[1] = 0;
  g_cpu_idle_last_sample[0] = 0;
  g_cpu_idle_last_sample[1] = 0;
  g_cpu_idle_baseline_rate[0] = 0;
  g_cpu_idle_baseline_rate[1] = 0;
  g_cpu_load_core_pct[0] = 0;
  g_cpu_load_core_pct[1] = 0;
  g_cpu_load_pct = 0;
  g_cpu_load_last_ms = millis();
  esp_register_freertos_idle_hook_for_cpu(cpu_idle_hook_0, 0);
#if defined(portNUM_PROCESSORS) && (portNUM_PROCESSORS > 1)
  esp_register_freertos_idle_hook_for_cpu(cpu_idle_hook_1, 1);
#endif
}

static void cpu_load_task(){
  const uint32_t now = millis();
  const uint32_t elapsed_ms = (uint32_t)(now - g_cpu_load_last_ms);
  if (elapsed_ms < CPU_LOAD_UPDATE_MS) return;
  g_cpu_load_last_ms = now;

  uint32_t sample[2] = { g_cpu_idle_counter[0], g_cpu_idle_counter[1] };
  uint32_t delta0 = sample[0] - g_cpu_idle_last_sample[0];
  uint32_t delta1 = sample[1] - g_cpu_idle_last_sample[1];
  g_cpu_idle_last_sample[0] = sample[0];
  g_cpu_idle_last_sample[1] = sample[1];

  uint32_t idle0_rate = (elapsed_ms > 0U) ? (uint32_t)((1000ULL * delta0) / elapsed_ms) : 0U;
  uint32_t idle1_rate = (elapsed_ms > 0U) ? (uint32_t)((1000ULL * delta1) / elapsed_ms) : 0U;
  const bool baseline_sample_ok = (elapsed_ms <= CPU_LOAD_BASELINE_MAX_ELAPSED_MS);

  if (baseline_sample_ok) {
    if ((g_cpu_idle_baseline_rate[0] == 0U) || (idle0_rate > g_cpu_idle_baseline_rate[0])) g_cpu_idle_baseline_rate[0] = idle0_rate;
#if defined(portNUM_PROCESSORS) && (portNUM_PROCESSORS > 1)
    if ((g_cpu_idle_baseline_rate[1] == 0U) || (idle1_rate > g_cpu_idle_baseline_rate[1])) g_cpu_idle_baseline_rate[1] = idle1_rate;
#else
    g_cpu_idle_baseline_rate[1] = 0;
#endif
  }

  uint32_t idle0_pct = (g_cpu_idle_baseline_rate[0] > 0U) ? (uint32_t)((100ULL * idle0_rate) / g_cpu_idle_baseline_rate[0]) : 0U;
  if (idle0_pct > 100U) idle0_pct = 100U;
  g_cpu_load_core_pct[0] = 100U - idle0_pct;

#if defined(portNUM_PROCESSORS) && (portNUM_PROCESSORS > 1)
  uint32_t idle1_pct = (g_cpu_idle_baseline_rate[1] > 0U) ? (uint32_t)((100ULL * idle1_rate) / g_cpu_idle_baseline_rate[1]) : 0U;
  if (idle1_pct > 100U) idle1_pct = 100U;
  g_cpu_load_core_pct[1] = 100U - idle1_pct;
  g_cpu_load_pct = (g_cpu_load_core_pct[0] + g_cpu_load_core_pct[1]) / 2U;
#else
  g_cpu_load_core_pct[1] = 0;
  g_cpu_load_pct = g_cpu_load_core_pct[0];
#endif
}

static uint32_t cpu_load_pct(){
  return g_cpu_load_pct;
}

static const char* link_state_name(uint8_t s){
  switch (s){
    case LINK_DOWN: return "disconnected";
    case LINK_CONNECTING: return "connecting";
    case LINK_UP: return "connected";
    default: return "unknown";
  }
}
static inline const char* json_bool(bool v){ return v ? "true" : "false"; }
static inline void copy_cstr(char* dst, size_t dstlen, const String& src){
  if (!dst || !dstlen) return;
  strlcpy(dst, src.c_str(), dstlen);
}
static String html_escape(const String& in){
  String out; out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); ++i){
    const char c = in[i];
    switch (c){
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}
static String json_escape(const char* s){
  String out;
  if (!s) return out;
  for (const char* p = s; *p; ++p){
    const unsigned char c = (unsigned char)(*p);
    switch (c){
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char tmp[7];
          snprintf(tmp, sizeof(tmp), "\\u%04x", c);
          out += tmp;
        } else {
          out += (char)c;
        }
        break;
    }
  }
  return out;
}
static String json_escape(const String& s){ return json_escape(s.c_str()); }
static String cfg_value(const char* s){ return html_escape(String(s ? s : "")); }
static bool parse_ip_field(const char* s, IPAddress& out){
  return s && s[0] && out.fromString(s);
}
static String active_ip_string(){
  if (WiFi.isConnected()) return WiFi.localIP().toString();
  if (g_cfg_portal_active) return WiFi.softAPIP().toString();
  return String("0.0.0.0");
}
static uint8_t wifi_signal_pct_from_rssi(int32_t rssi){
  if (rssi <= -100) return 0;
  if (rssi >= -50) return 100;
  return (uint8_t)(2 * (rssi + 100));
}
static String wifi_signal_string(){
  if (!WiFi.isConnected()){
    if (g_cfg_portal_active) return String("setup AP active");
    return String("n/a");
  }
  int32_t rssi = WiFi.RSSI();
  String s = String(rssi);
  s += F(" dBm (");
  s += String(wifi_signal_pct_from_rssi(rssi));
  s += F(" %)");
  return s;
}
static String portal_url(){
  if (!g_cfg_portal_active) return String("");
  return String("http://") + WiFi.softAPIP().toString() + "/config";
}
static String web_captive_target_url(){
  const String url = portal_url();
  if (url.length()) return url;
  return String("http://") + WiFi.softAPIP().toString() + "/config";
}
static void web_send_redirect_302(const String& url){
#if HTTP_ENABLE
  g_http.sendHeader("Cache-Control", "no-store");
  g_http.sendHeader("Location", url, true);
  g_http.send(302, "text/plain; charset=utf-8", "");
#else
  (void)url;
#endif
}
static void web_handle_captive_probe_redirect(){
  if (g_cfg_portal_active) {
    web_send_redirect_302(web_captive_target_url());
    return;
  }
  g_http.send(204, "text/plain; charset=utf-8", "");
}
static void web_handle_captive_success(){
  if (g_cfg_portal_active) {
    web_send_redirect_302(web_captive_target_url());
    return;
  }
  g_http.send(200, "text/plain; charset=utf-8", "success\n");
}
static void web_handle_wpad(){
  g_http.send(404, "text/plain; charset=utf-8", "Not found");
}
static void web_handle_not_found(){
  if (g_cfg_portal_active) {
    web_send_redirect_302(web_captive_target_url());
    return;
  }
  g_http.send(404, "text/plain; charset=utf-8", "Not found");
}
static const char* wifi_auth_label(uint8_t auth){
  switch ((wifi_auth_mode_t)auth){
    case WIFI_AUTH_OPEN: return "open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK: return "WAPI";
    default: return "secured";
  }
}
static void net_wifi_scan_task(){
#if WIFI_ENABLE && HTTP_ENABLE
  if (!g_wifi_scan_running) return;

  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING){
    if ((uint32_t)(millis() - g_wifi_scan_started_ms) > 15000UL){
      WiFi.scanDelete();
      g_wifi_scan_running = false;
      if (g_wifi_scan_restore_ap_only) WiFi.mode(g_wifi_scan_restore_mode);
      if (dbg_show_err) DBG.println(F("[WIFI] scan timeout"));
    }
    return;
  }

  g_wifi_scan_running = false;
  g_wifi_scan_count = 0;

  if (n > 0){
    for (int i = 0; i < n; ++i){
      String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;

      int existing = -1;
      for (uint8_t k = 0; k < g_wifi_scan_count; ++k){
        if (ssid.equals(String(g_wifi_scan[k].ssid))){ existing = (int)k; break; }
      }

      const int32_t rssi = WiFi.RSSI(i);
      const uint8_t auth = (uint8_t)WiFi.encryptionType(i);

      if (existing >= 0){
        if (rssi > g_wifi_scan[existing].rssi){
          strlcpy(g_wifi_scan[existing].ssid, ssid.c_str(), sizeof(g_wifi_scan[existing].ssid));
          g_wifi_scan[existing].rssi = rssi;
          g_wifi_scan[existing].auth = auth;
        }
      } else if (g_wifi_scan_count < WIFI_SCAN_MAX_RESULTS){
        strlcpy(g_wifi_scan[g_wifi_scan_count].ssid, ssid.c_str(), sizeof(g_wifi_scan[g_wifi_scan_count].ssid));
        g_wifi_scan[g_wifi_scan_count].rssi = rssi;
        g_wifi_scan[g_wifi_scan_count].auth = auth;
        ++g_wifi_scan_count;
      }

      if ((i & 0x03) == 0x03) service_bus_io_once();
    }

    for (uint8_t a = 0; a < g_wifi_scan_count; ++a){
      for (uint8_t b = a + 1; b < g_wifi_scan_count; ++b){
        if (g_wifi_scan[b].rssi > g_wifi_scan[a].rssi){
          WifiScanEntry tmp = g_wifi_scan[a];
          g_wifi_scan[a] = g_wifi_scan[b];
          g_wifi_scan[b] = tmp;
        }
      }
    }
    g_wifi_scan_valid = true;
    g_wifi_scan_last_ms = millis();
  } else if (n < 0){
    g_wifi_scan_valid = false;
    if (dbg_show_err) DBG.println(F("[WIFI] scan failed"));
  }

  WiFi.scanDelete();
  if (g_wifi_scan_restore_ap_only) WiFi.mode(g_wifi_scan_restore_mode);
#endif
}
static void net_refresh_wifi_scan(bool force){
#if WIFI_ENABLE && HTTP_ENABLE
  const uint32_t now = millis();
  if (!force && g_wifi_scan_valid && (uint32_t)(now - g_wifi_scan_last_ms) < 30000UL) return;
  if (g_wifi_scan_running) return;

  g_wifi_scan_restore_mode = WiFi.getMode();
  g_wifi_scan_restore_ap_only = (g_wifi_scan_restore_mode == WIFI_AP);
  if (g_wifi_scan_restore_ap_only) WiFi.mode(WIFI_AP_STA);

  const int rc = WiFi.scanNetworks(true, true);
  if (rc == WIFI_SCAN_RUNNING){
    g_wifi_scan_running = true;
    g_wifi_scan_started_ms = now;
  } else if (rc >= 0){
    g_wifi_scan_running = true;
    g_wifi_scan_started_ms = now;
    net_wifi_scan_task();
  } else {
    if (g_wifi_scan_restore_ap_only) WiFi.mode(g_wifi_scan_restore_mode);
    if (dbg_show_err) DBG.println(F("[WIFI] could not start async scan"));
  }
#endif
}
static bool netcfg_has_wifi(){
  if (!g_netcfg.wifi_ssid[0]) return false;
  if (!strcmp(g_netcfg.wifi_ssid, "YOUR_WIFI_SSID")) return false;
  return true;
}
static void netcfg_set_defaults(){
  memset(&g_netcfg, 0, sizeof(g_netcfg));
  strlcpy(g_netcfg.wifi_ssid, WIFI_SSID, sizeof(g_netcfg.wifi_ssid));
  strlcpy(g_netcfg.wifi_password, WIFI_PASSWORD, sizeof(g_netcfg.wifi_password));
  strlcpy(g_netcfg.ap_password, WIFI_CONFIG_AP_PASSWORD, sizeof(g_netcfg.ap_password));
  strlcpy(g_netcfg.wifi_hostname,
          g_cfg_wifi_hostname_default[0] ? g_cfg_wifi_hostname_default : WIFI_HOSTNAME_PREFIX,
          sizeof(g_netcfg.wifi_hostname));
  strlcpy(g_netcfg.mqtt_host, MQTT_HOST, sizeof(g_netcfg.mqtt_host));
  g_netcfg.mqtt_port = MQTT_PORT;
  strlcpy(g_netcfg.mqtt_username, MQTT_USERNAME, sizeof(g_netcfg.mqtt_username));
  strlcpy(g_netcfg.mqtt_password, MQTT_PASSWORD, sizeof(g_netcfg.mqtt_password));
  g_netcfg.mqtt_enable = MQTT_ENABLE ? 1 : 0;
  g_netcfg.mqtt_tls_enable = 0;
  g_netcfg.mqtt_tls_insecure = 0;
  g_netcfg.mqtt_ca_cert[0] = 0;
  g_netcfg.web_username[0] = 0;
  g_netcfg.web_password[0] = 0;
  g_netcfg.use_dhcp = 1;
}
static void netcfg_load(){
  netcfg_set_defaults();
  copy_cstr(g_netcfg.wifi_ssid, sizeof(g_netcfg.wifi_ssid), nv.getString("w_ssid", g_netcfg.wifi_ssid));
  copy_cstr(g_netcfg.wifi_password, sizeof(g_netcfg.wifi_password), nv.getString("w_pwd", g_netcfg.wifi_password));
  copy_cstr(g_netcfg.ap_password, sizeof(g_netcfg.ap_password), nv.getString("ap_pwd", g_netcfg.ap_password));
  copy_cstr(g_netcfg.wifi_hostname, sizeof(g_netcfg.wifi_hostname), nv.getString("w_host", g_netcfg.wifi_hostname));
  if (!g_netcfg.wifi_hostname[0] || !strcmp(g_netcfg.wifi_hostname, "esp32") ||
      !strncmp(g_netcfg.wifi_hostname, "esp32-", 6) ||
      !strncmp(g_netcfg.wifi_hostname, "esp32_", 6) ||
      !strcmp(g_netcfg.wifi_hostname, "jbc-fae-emulator") ||
      !strcmp(g_netcfg.wifi_hostname, "JBC-FAE-Emulator") ||
      !strcmp(g_netcfg.wifi_hostname, "JBC-FAE-Emulator-esp32")) {
    strlcpy(g_netcfg.wifi_hostname,
            g_cfg_wifi_hostname_default[0] ? g_cfg_wifi_hostname_default : WIFI_HOSTNAME_PREFIX,
            sizeof(g_netcfg.wifi_hostname));
  }
  copy_cstr(g_netcfg.mqtt_host, sizeof(g_netcfg.mqtt_host), nv.getString("mq_host", g_netcfg.mqtt_host));
  g_netcfg.mqtt_port = nv.getUShort("mq_port", g_netcfg.mqtt_port);
  copy_cstr(g_netcfg.mqtt_username, sizeof(g_netcfg.mqtt_username), nv.getString("mq_user", g_netcfg.mqtt_username));
  copy_cstr(g_netcfg.mqtt_password, sizeof(g_netcfg.mqtt_password), nv.getString("mq_pwd", g_netcfg.mqtt_password));
  g_netcfg.mqtt_enable = nv.getUChar("mq_en", g_netcfg.mqtt_enable);
  g_netcfg.mqtt_tls_enable = nv.getUChar("mq_tls", 0);
  g_netcfg.mqtt_tls_insecure = nv.getUChar("mq_tlsi", 0);
  copy_cstr(g_netcfg.mqtt_ca_cert, sizeof(g_netcfg.mqtt_ca_cert), nv.getString("mq_cacert", ""));
  copy_cstr(g_netcfg.web_username, sizeof(g_netcfg.web_username), nv.getString("http_user", ""));
  copy_cstr(g_netcfg.web_password, sizeof(g_netcfg.web_password), nv.getString("http_pwd", ""));
  g_netcfg.use_dhcp = nv.getUChar("dhcp", 1);
  copy_cstr(g_netcfg.static_ip, sizeof(g_netcfg.static_ip), nv.getString("ip", ""));
  copy_cstr(g_netcfg.static_gw, sizeof(g_netcfg.static_gw), nv.getString("gw", ""));
  copy_cstr(g_netcfg.static_mask, sizeof(g_netcfg.static_mask), nv.getString("mask", ""));
  copy_cstr(g_netcfg.static_dns1, sizeof(g_netcfg.static_dns1), nv.getString("dns1", ""));
  copy_cstr(g_netcfg.static_dns2, sizeof(g_netcfg.static_dns2), nv.getString("dns2", ""));
}
static void netcfg_save(){
  nv.putString("w_ssid", g_netcfg.wifi_ssid);
  nv.putString("w_pwd", g_netcfg.wifi_password);
  nv.putString("ap_pwd", g_netcfg.ap_password);
  nv.putString("w_host", g_netcfg.wifi_hostname);
  nv.putString("mq_host", g_netcfg.mqtt_host);
  nv.putUShort("mq_port", g_netcfg.mqtt_port);
  nv.putString("mq_user", g_netcfg.mqtt_username);
  nv.putString("mq_pwd", g_netcfg.mqtt_password);
  nv.putUChar("mq_en", g_netcfg.mqtt_enable ? 1 : 0);
  nv.putUChar("mq_tls", g_netcfg.mqtt_tls_enable ? 1 : 0);
  nv.putUChar("mq_tlsi", g_netcfg.mqtt_tls_insecure ? 1 : 0);
  nv.putString("mq_cacert", g_netcfg.mqtt_ca_cert);
  nv.putString("http_user", g_netcfg.web_username);
  nv.putString("http_pwd", g_netcfg.web_password);
  nv.putUChar("dhcp", g_netcfg.use_dhcp ? 1 : 0);
  nv.putString("ip", g_netcfg.static_ip);
  nv.putString("gw", g_netcfg.static_gw);
  nv.putString("mask", g_netcfg.static_mask);
  nv.putString("dns1", g_netcfg.static_dns1);
  nv.putString("dns2", g_netcfg.static_dns2);
}
static const char* netcfg_ap_password(){
  return (strlen(g_netcfg.ap_password) >= 8) ? g_netcfg.ap_password : WIFI_CONFIG_AP_PASSWORD;
}
static void net_init_identity(){
  uint64_t mac = ESP.getEfuseMac();
  uint8_t sta_mac[6] = {0};
  if (esp_read_mac(sta_mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    sta_mac[0] = (uint8_t)((mac >> 40) & 0xFF);
    sta_mac[1] = (uint8_t)((mac >> 32) & 0xFF);
    sta_mac[2] = (uint8_t)((mac >> 24) & 0xFF);
    sta_mac[3] = (uint8_t)((mac >> 16) & 0xFF);
    sta_mac[4] = (uint8_t)((mac >> 8) & 0xFF);
    sta_mac[5] = (uint8_t)(mac & 0xFF);
  }
  const uint32_t sta_low24 = ((uint32_t)sta_mac[3] << 16) | ((uint32_t)sta_mac[4] << 8) | (uint32_t)sta_mac[5];
  const uint16_t sta_low16 = ((uint16_t)sta_mac[4] << 8) | (uint16_t)sta_mac[5];
  if (!g_cfg_wifi_hostname_default[0]) {
    snprintf(g_cfg_wifi_hostname_default, sizeof(g_cfg_wifi_hostname_default), "%s-%06X",
             WIFI_HOSTNAME_PREFIX, sta_low24);
  }
#if MQTT_ENABLE
  if (!g_mqtt_device_id[0]) {
    snprintf(g_mqtt_device_id, sizeof(g_mqtt_device_id), "jbcfae-%04X%08X",
             (uint16_t)(mac >> 32), (uint32_t)mac);
    snprintf(g_mqtt_base_topic, sizeof(g_mqtt_base_topic), "%s/%s", MQTT_BASE_TOPIC, g_mqtt_device_id);
  }
  if (!g_cfg_ap_ssid[0]) {
    snprintf(g_cfg_ap_ssid, sizeof(g_cfg_ap_ssid), "%s-%04X", WIFI_CONFIG_AP_SSID_PREFIX, sta_low16);
  }
#else
  if (!g_cfg_ap_ssid[0]) {
    snprintf(g_cfg_ap_ssid, sizeof(g_cfg_ap_ssid), "%s-%04X", WIFI_CONFIG_AP_SSID_PREFIX, sta_low16);
  }
#endif
}
static bool net_apply_sta_ip_config(){
  if (g_netcfg.use_dhcp) {
    IPAddress zero(0,0,0,0);
    return WiFi.config(zero, zero, zero);
  }
  IPAddress ip, gw, mask, dns1, dns2;
  if (!parse_ip_field(g_netcfg.static_ip, ip)) return false;
  if (!parse_ip_field(g_netcfg.static_gw, gw)) return false;
  if (!parse_ip_field(g_netcfg.static_mask, mask)) return false;
  const bool have_dns1 = parse_ip_field(g_netcfg.static_dns1, dns1);
  const bool have_dns2 = parse_ip_field(g_netcfg.static_dns2, dns2);
  if (have_dns1 && have_dns2) return WiFi.config(ip, gw, mask, dns1, dns2);
  if (have_dns1) return WiFi.config(ip, gw, mask, dns1);
  return WiFi.config(ip, gw, mask);
}
static uint32_t net_state_hash(){
  uint32_t h = 2166136261UL;
  auto mix = [&](uint32_t v){ h ^= v; h *= 16777619UL; };
  mix(st.suctionLevel);
  mix(st.flow_x1000);
  mix(st.speed_rpm);
  mix(st.selectFlow);
  mix(st.standIntakes);
  mix(bus_intake_latched(0, WORK_IDX) ? 1U : 0U);
  mix(bus_intake_latched(0, STAND_IDX) ? 1U : 0U);
  mix(bus_intake_latched(1, WORK_IDX) ? 1U : 0U);
  mix(bus_intake_latched(1, STAND_IDX) ? 1U : 0U);
  mix(st.delaySec[0][0]);
  mix(st.delaySec[0][1]);
  mix(st.pedalAct[0]);
  mix(st.pedalMode[0]);
  mix(st.filterLife);
  mix(st.filterSaturation);
  mix(st.statError);
  mix(st.usbConnect);
  mix(st.continuous);
  mix(st.connectedPedal[0]);
  mix(g_relay_on ? 1 : 0);
  mix(g_relay_off_deadline ? relay_time_left_ms()/1000UL : 0UL);
  mix((g_relay_owner >= 0) ? (uint32_t)(g_relay_owner + 1) : 0UL);
  mix((g_afterrun_owner >= 0) ? (uint32_t)(g_afterrun_owner + 1) : 0UL);
  mix(g_relay_inverted ? 1U : 0U);
  mix(g_bus_count);
  mix(g_cfg_portal_active ? 1U : 0U);
  for(uint8_t i=0;i<g_bus_count;i++){
    mix(g_bus[i].my_addr);
    mix(g_bus[i].stn_addr);
    mix((uint32_t)g_bus[i].linkState);
  }
  return h;
}
static String net_build_state_json_string(){
  const uint32_t afterrun_s = g_relay_off_deadline ? (relay_time_left_ms() / 1000UL) : 0UL;
  const int relay_owner = (g_relay_owner >= 0) ? g_relay_owner : -1;
  const int afterrun_owner = (g_afterrun_owner >= 0) ? g_afterrun_owner : -1;
  const String ip_s = active_ip_string();
  const ErrInfo* err_info = find_errinfo_by_mask(st.statError);
  const char* err_key = (st.statError == 0) ? "NOERR" : (err_info ? err_info->key : "CUSTOM");
  const char* err_text = (st.statError == 0) ? "No error" : (err_info ? (const char*)err_info->desc : "Custom / unknown mask");
  const String err_label = error_option_label_by_mask(st.statError);
  const bool wifi_connected = WiFi.isConnected();
  const int32_t wifi_rssi = wifi_connected ? WiFi.RSSI() : 0;
  const uint8_t wifi_signal_pct = wifi_connected ? wifi_signal_pct_from_rssi(wifi_rssi) : 0;
  const String bus0_dev = bus_device_id_string(0);
  const String bus1_dev = bus_device_id_string(1);

  String json;
  json.reserve(2048);
  json += '{';
  json += "\"relay\":"; json += json_bool(g_relay_on);
  json += ",\"continuous\":"; json += json_bool(st.continuous);
  json += ",\"usb_connect\":"; json += json_bool(st.usbConnect);
  json += ",\"wifi_connected\":"; json += json_bool(wifi_connected);
  json += ",\"portal_active\":"; json += json_bool(g_cfg_portal_active);
  json += ",\"wifi_rssi\":"; json += String(wifi_rssi);
  json += ",\"wifi_signal_pct\":"; json += String(wifi_signal_pct);
  json += ",\"cpu_load_pct\":"; json += String(cpu_load_pct());
  json += ",\"suction_level\":"; json += String(st.suctionLevel);
  json += ",\"suction_level_key\":\""; json += json_escape(suction_level_key(st.suctionLevel)); json += '"';
  json += ",\"suction_level_label\":\""; json += json_escape(suction_level_label(st.suctionLevel)); json += '"';
  json += ",\"flow_x1000\":"; json += String(st.flow_x1000);
  json += ",\"actual_flow\":"; json += String(actualflow_raw_to_pct(st.flow_x1000));
  json += ",\"speed_rpm\":"; json += String(st.speed_rpm);
  json += ",\"select_flow\":"; json += String(selectflow_raw_to_pct(st.selectFlow));
  json += ",\"stand_intakes\":"; json += String(st.standIntakes);
  json += ",\"intake_work\":"; json += String(any_intake_latched(WORK_IDX) ? 1U : 0U);
  json += ",\"intake_stand\":"; json += String(any_intake_latched(STAND_IDX) ? 1U : 0U);
  json += ",\"bus0_work_active\":"; json += String(bus_intake_latched(0, WORK_IDX) ? 1U : 0U);
  json += ",\"bus0_stand_active\":"; json += String(bus_intake_latched(0, STAND_IDX) ? 1U : 0U);
  json += ",\"bus1_work_active\":"; json += String(bus_intake_latched(1, WORK_IDX) ? 1U : 0U);
  json += ",\"bus1_stand_active\":"; json += String(bus_intake_latched(1, STAND_IDX) ? 1U : 0U);
  json += ",\"bus0_active\":"; json += String(bus_active(0) ? 1U : 0U);
  json += ",\"bus1_active\":"; json += String(bus_active(1) ? 1U : 0U);
  json += ",\"bus0_afterrun\":"; json += String(bus_afterrun(0) ? 1U : 0U);
  json += ",\"bus1_afterrun\":"; json += String(bus_afterrun(1) ? 1U : 0U);
  json += ",\"bus0_relay_state\":\""; json += json_escape(bus_relay_state_label(0)); json += '"';
  json += ",\"bus1_relay_state\":\""; json += json_escape(bus_relay_state_label(1)); json += '"';
  json += ",\"delay_work_s\":"; json += String(st.delaySec[0][WORK_IDX]);
  json += ",\"delay_stand_s\":"; json += String(st.delaySec[0][STAND_IDX]);
  json += ",\"filter_life\":"; json += String(filter_raw_to_pct(st.filterLife));
  json += ",\"filter_saturation\":"; json += String(filter_raw_to_pct(st.filterSaturation));
  json += ",\"stat_error\":"; json += String(st.statError);
  json += ",\"error_level\":\""; json += json_escape(error_level_name(st.statError)); json += '"';
  json += ",\"error_key\":\""; json += json_escape(err_key); json += '"';
  json += ",\"error_text\":\""; json += json_escape(err_text); json += '"';
  json += ",\"error_label\":\""; json += json_escape(err_label); json += '"';
  json += ",\"connected_pedal\":"; json += String(st.connectedPedal[0]);
  json += ",\"afterrun_remaining_s\":"; json += String(afterrun_s);
  json += ",\"afterrun_active\":"; json += json_bool(g_relay_off_deadline != 0);
  json += ",\"relay_owner\":"; json += String(relay_owner);
  json += ",\"afterrun_owner\":"; json += String(afterrun_owner);
  json += ",\"relay_mode\":\""; json += json_escape(relay_mode_key()); json += '"';
  json += ",\"led_brightness\":"; json += String(led_brightness_raw_to_pct(g_led_brightness));
  json += ",\"relay_inverted\":"; json += json_bool(g_relay_inverted != 0);
  json += ",\"mqtt_enabled\":"; json += json_bool(g_netcfg.mqtt_enable);
  json += ",\"bus_count\":"; json += String(g_bus_count);
  json += ",\"bus0_addr\":"; json += String(g_bus[0].my_addr);
  json += ",\"bus0_station\":\""; json += json_escape((const char*)station_name_from_addr(g_bus[0].my_addr)); json += '"';
  json += ",\"bus0_link\":\""; json += json_escape(link_state_name(g_bus[0].linkState)); json += '"';
  json += ",\"bus0_device_id\":\""; json += json_escape(bus0_dev); json += '"';
  json += ",\"led_jbc1_mode\":\""; json += json_escape(led_link_mode_key(0)); json += '"';
  json += ",\"led_evt1_mode\":\""; json += json_escape(led_event_mode_key(0)); json += '"';
  json += ",\"bus1_addr\":"; json += String(g_bus[1].my_addr);
  json += ",\"bus1_station\":\""; json += json_escape((const char*)station_name_from_addr(g_bus[1].my_addr)); json += '"';
  json += ",\"bus1_link\":\""; json += json_escape(link_state_name(g_bus[1].linkState)); json += '"';
  json += ",\"bus1_device_id\":\""; json += json_escape(bus1_dev); json += '"';
  json += ",\"led_jbc2_mode\":\""; json += json_escape(led_link_mode_key(1)); json += '"';
  json += ",\"led_evt2_mode\":\""; json += json_escape(led_event_mode_key(1)); json += '"';
  json += ",\"ip\":\""; json += json_escape(ip_s); json += '"';
  json += ",\"wifi_hostname\":\""; json += json_escape(g_netcfg.wifi_hostname); json += '"';
  json += ",\"ap_ssid\":\""; json += json_escape(g_cfg_ap_ssid); json += '"';
  json += ",\"ssid\":\""; json += json_escape(netcfg_has_wifi() ? g_netcfg.wifi_ssid : ""); json += '"';
  json += '}';

  return json;
}

static void cli_show_net(){
#if WIFI_ENABLE
  DBG.print(F("[NET] mode="));
  if (WiFi.isConnected()) DBG.print(F("sta"));
  else if (g_cfg_portal_active) DBG.print(F("ap"));
  else DBG.print(F("idle"));
  DBG.print(F(" ssid=")); DBG.print(netcfg_has_wifi() ? g_netcfg.wifi_ssid : "<unset>");
  DBG.print(F(" ip=")); DBG.print(active_ip_string());
  if (WiFi.isConnected()){
    DBG.print(F(" rssi=")); DBG.print(WiFi.RSSI()); DBG.print(F(" dBm (")); DBG.print(wifi_signal_pct_from_rssi(WiFi.RSSI())); DBG.print(F("%)"));
  }
  DBG.print(F(" cpu~=")); DBG.print(cpu_load_pct()); DBG.print(F("%")); DBG.print(F(" hostname=")); DBG.print(g_netcfg.wifi_hostname);
  DBG.print(F(" | portal=")); DBG.print(g_cfg_portal_active ? F("on") : F("off"));
  if (g_cfg_portal_active){
    DBG.print(F(" ap_ssid=")); DBG.print(g_cfg_ap_ssid);
    DBG.print(F(" ap_ip=")); DBG.print(WiFi.softAPIP());
  }
#else
  DBG.print(F("[NET] wifi=disabled"));
#endif
#if HTTP_ENABLE
  DBG.print(F(" | web=")); DBG.print(g_http_started ? F("on") : F("off"));
  DBG.print(F(" port=")); DBG.print(HTTP_PORT);
#endif
#if MQTT_ENABLE
  DBG.print(F(" | mqtt="));
  if (!g_netcfg.mqtt_enable) DBG.print(F("disabled"));
  else DBG.print(g_mqtt.connected() ? F("connected") : F("disconnected"));
  DBG.print(F(" host=")); DBG.print(g_netcfg.mqtt_host); DBG.print(':'); DBG.print(g_netcfg.mqtt_port);
  DBG.print(F(" tls=")); if (!g_netcfg.mqtt_tls_enable) DBG.print(F("off")); else if (g_netcfg.mqtt_tls_insecure) DBG.print(F("insecure")); else DBG.print(F("ca"));
  DBG.print(F(" topic=")); DBG.print(g_mqtt_base_topic);
#endif
  DBG.println();
}
#if HTTP_ENABLE
static String web_html_status(){
  String json = net_build_state_json_string();
  String html;
  html.reserve(11200);
  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>JBC FAE Emulator</title>");
  html += F("<style>*{box-sizing:border-box}body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:24px}"
            ".nav,.actions{display:flex;flex-wrap:wrap;gap:10px;margin:0 0 12px 0}.nav a,.nav button,.actions a,.actions button{display:inline-flex;align-items:center;justify-content:center;margin:0;padding:10px 14px;border-radius:10px;background:#2a6df4;color:#fff;text-decoration:none;border:0;max-width:100%}"
            ".card{background:#1d1d1d;padding:16px;border-radius:12px;margin:12px 0;overflow:hidden}"
            "table{width:100%;border-collapse:collapse} td,th{padding:8px;border-bottom:1px solid #333;text-align:left;vertical-align:top}"
            "code,pre{background:#000;padding:12px;border-radius:10px;display:block;overflow:auto;white-space:pre-wrap}"
            ".muted{color:#aaa}"
            ".live-dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:#2ecc71;margin-right:8px;vertical-align:middle}"
            ".bus-leds{display:flex;flex-wrap:wrap;gap:28px;align-items:center;margin-top:8px}"
            ".led-wrap{text-align:center;min-width:96px}"
            ".led-text{display:inline-block;padding:4px 8px;font-size:28px;font-weight:800;letter-spacing:1px;color:#555;opacity:.35;transition:all .2s ease;line-height:1.1}"
            ".led-text.led-off{color:#555;text-shadow:none;opacity:.35}"
            ".front-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:14px;margin-top:14px}"
            ".front-tile{background:linear-gradient(180deg,#171717,#121212);border:1px solid #2a2a2a;border-radius:14px;padding:14px;box-shadow:0 8px 24px rgba(0,0,0,.22)}"
            ".front-head{display:flex;align-items:center;justify-content:flex-start;gap:12px;margin-bottom:10px}"
            ".front-title{font-size:24px;font-weight:800;letter-spacing:.4px}"
            ".front-sub{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px 12px;margin-top:12px}"
            ".front-meta{background:#181818;border:1px solid #2b2b2b;border-radius:10px;padding:8px 10px;min-height:54px}"
            ".front-meta .k{display:block;font-size:11px;color:#9aa0aa;text-transform:uppercase;letter-spacing:.7px;margin-bottom:4px}"
            ".front-meta .v{display:block;font-size:14px;font-weight:700;color:#f2f2f2;overflow-wrap:anywhere}"
            ".state-chip{display:inline-flex;align-items:center;justify-content:center;min-width:88px;padding:6px 10px;border-radius:999px;font-size:12px;font-weight:800;text-transform:uppercase;letter-spacing:.65px;border:1px solid #313131;background:#1b1b1b;color:#d0d0d0}"
            ".state-chip.state-active{background:rgba(47,128,255,.14);border-color:rgba(47,128,255,.42);color:#8ec0ff}"            ".state-chip.tool-state-active{background:rgba(59,209,111,.14);border-color:rgba(59,209,111,.42);color:#98f0b4}"
            ".state-chip.state-afterrun{background:rgba(255,51,204,.12);border-color:rgba(255,51,204,.38);color:#ff8ae6}"
            ".state-chip.state-idle{background:#1a1a1a;border-color:#313131;color:#d0d0d0}"
            ".state-chip.state-inactive{background:#151515;border-color:#272727;color:#7e7e7e}"
            ".state-chip.state-continuous{background:rgba(47,128,255,.14);border-color:rgba(47,128,255,.42);color:#8ec0ff}"
            ".quick-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-top:14px}"
            ".quick-card{background:#171717;border:1px solid #2a2a2a;border-radius:12px;padding:12px}"
            ".quick-card .k{display:block;font-size:11px;color:#9aa0aa;text-transform:uppercase;letter-spacing:.7px;margin-bottom:6px}"
            ".quick-card .v{display:block;font-size:18px;font-weight:800;color:#f2f2f2}"
            ".quick-sub{display:block;margin-top:6px;font-size:12px;color:#9aa0aa}.quick-sub.afterrun-strong{font-size:18px;font-weight:800;line-height:1.15;color:#f2f5fb}"
            ".quick-signal{display:flex;align-items:center;gap:12px;min-height:28px}.signal-bars{display:inline-flex;align-items:flex-end;gap:4px;height:22px}.signal-bars i{display:block;width:6px;border-radius:4px;background:#404040;opacity:.45}.signal-bars i:nth-child(1){height:6px}.signal-bars i:nth-child(2){height:11px}.signal-bars i:nth-child(3){height:16px}.signal-bars i:nth-child(4){height:22px}.signal-bars.level-1 i:nth-child(1),.signal-bars.level-2 i:nth-child(-n+2),.signal-bars.level-3 i:nth-child(-n+3),.signal-bars.level-4 i:nth-child(-n+4){background:#8ec0ff;opacity:1;box-shadow:0 0 10px rgba(142,192,255,.18)}.signal-bars.off i{background:#404040;opacity:.35}"
            ".signal-text{font-size:16px!important}.quick-cpu{display:flex;flex-direction:column;gap:8px}.quick-meter{display:block;width:100%;height:8px;border-radius:999px;background:#232323;border:1px solid #2f2f2f;overflow:hidden}.quick-meter-fill{display:block;height:100%;width:0%;border-radius:999px;background:#8ec0ff;box-shadow:0 0 10px rgba(142,192,255,.18)}"
            ".front-hint{margin-top:8px;font-size:12px;color:#9aa0aa}"
            ".front-sensors{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;margin-top:12px}"
            ".front-sensor-card{background:#181818;border:1px solid #2b2b2b;border-radius:10px;padding:10px;min-height:92px}"
            ".front-sensor-card .k{display:block;font-size:11px;color:#9aa0aa;text-transform:uppercase;letter-spacing:.7px;margin-bottom:8px}"
            ".relay-chip-wrap{display:flex;align-items:center;min-height:42px}"
            ".front-device{margin-top:10px;font-size:15px;font-weight:700;color:#e4e8ee;overflow-wrap:anywhere}"
            "@media (max-width:900px){.front-sensors{grid-template-columns:1fr}}"
            ".tool-status-wrap{display:inline-flex;align-items:center;gap:0;min-height:38px}"
            ".tool-state-chip{min-width:88px;padding:6px 10px;font-size:12px;letter-spacing:.65px;margin-left:0}"
            ".tool-icon-box.is-work + .tool-state-chip{margin-left:-10px}"
            ".tool-icon-box.stand-only + .tool-state-chip{margin-left:8px}"
            ".tool-icon-box{position:relative;display:inline-block;width:88px;height:50px;flex:0 0 auto;--tool-color:#f2f2f2;--tool-glow:none;filter:drop-shadow(var(--tool-glow))}"
            ".tool-icon-box.is-work{width:88px;height:50px}"
            ".tool-icon-box.stand-only{width:88px;height:50px}"
            ".tool-iron{position:absolute;left:0;top:-6px;width:58px;height:58px;background:var(--tool-color);-webkit-mask-image:url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAQAAAAEACAMAAABrrFhUAAAAAXNSR0IB2cksfwAAAn9QTFRFAAAAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICA/S6DpQAAANV0Uk5TAAGa/8vn/mEZDv2X+/zuKErwKjH0L+wr8SxOZG5qElRVFNJn+bRWUFcPtXELzuUjsd0NbLDWFViI6VmJ77yYY1ODlrdJsxidx9GHj8E3QUMf6zu2OsiKMnMwSOI9wosivx2okuBwuKqCCXSp8kbcw2YGRHaluZR/eq1RbSFMb5Cc1dNH3tmGNqs5PGsMfE8+G1I0gQis0DjPfsoKYOTa2BfhmxaeaAdlGr74A3WuHtRF9l9aXEATWwJeNfeZJ+gR6ikt7foz9bIFLvMQzSWvBE3JJOYmwH4nZQAACBlJREFUeNrt3fdzVOUex/FP+IZg1iwkoBAukGYSyhUFARERe7te9aq39957793ee6GpNOm9BBOIITGEhSglf5A/6Mx3hudsNsnnPNnzned5/wE7+3nNsztn55yZRSwWi8VisVgsFovFYrFYLBaLxWKANnTmfNj7J8qF82HvFxUIc78KhLlfBcLcrwJh7g9TQPeHKaD7wxTQ/WEK6P4wBXR/mAK6P0wB3R+mgO4PU0D3hymg+8MU0P1hCuj+MAV0f5gCuj9MAd0fpoDuD1NA94cpoPtNCRz5ypLaoQEP+60InBMRyU389Ybl88uynxfgAT4pt2fT3g5+vx0BBdCO3zhI7jcloABa7rb3mf3mBBRA2/YYsd+cgAJoVY9vJPabEVAAt+kHiP12BBTAbdNvif28QPkBpKWH2M8LlB9A9p3FiNsgaXTV+UwBSH4/RtqHH4hk7QzwAFI126QAD6DdZFuAB8g9O3KBXekItJ8t2WveAbTt9xFnwFdV73gH0B7cnUWBm7wDaK8PZFFgkncA7S/IosB+pNShI3NurMzLMNWcyKJA7ihS7Mr7bqmXorUMZFLgCJCqwYH/SrHeQSYFDiDlGnslufrubAosR8ptLPbedyKTAvl5SLu1rclHoDqjAk1Iu0MFSWoOsilQ04i0e61fErqwIxyB/+cloeeQVYG5SLtbJKEuhCOw45y49a/JrsBZpNyUVnH7HDIr0Je6wPfF7TpkWKAW6dZdEKdlyKqAe53C1yVuzdkV6HwXKTdF3J4EIWBkv/YZcdoPQsDaftwqTl8CIWBtP2rFaSkIAWv7MV+cWpBFgf534af35OJWYKwCizzufx+a52/BPMbawEl7+3GvOF1DCJjbjzZxuhOEgLX9WMVdCroCxvbjbvIj4ArY2o8fuD+7K8AJmNq/Je8+wAOQAob2435x+iZoATv78VlxmoDsCFzmez8WitNTyIzAZdPguSlV4vQ78G01sh+Xitsl4LvEyP6GgjgtRhoANvbjVnFbmQqAjf1f7iPuC9AA5d/f0SJunT8tAwC7n78G0O5CKgAW9h8k7g7zAOXff3a7JLS+wwNAJvcf6JSk1qL8AIXL4b+f1EhSvWvKD1A4Be9d/UXiQUEaoPz7f98vyf1qi3eA8u/vfnSdFOuX8AbA77/+avB1N+3qlKK9Am8A/P4jfVdxAmtX3rVqcU6GqX+rRwB+vwgncE5K1Q5fAPz+g30iIhMHfQL8B74A+P1H+0RUwA/AmTU+Afj9KuAHYMV3UV6A+hMl9quAD4DCVJQXoH5mif0q4AOgfxo8AfD79+t+FUgboHfngB8Afv+kGhFHgAZwe3tzh28Afr8K8ABub97uF4DfrwLHWIDkTm71CcDv144f8wIghWd2pAjA75+l+0sIkADa6n94BeD3q4AfACn8xjsAv18FeAC3tupxBKibMer9KuAJQJY9MG4AdVOH3U8IMADy4EM0AL3/ihoRQoADkNaDLAC9Py9CCLAAUjWbBPC7XwV6MOJmHKptPPqvrrv3yYj6Mwfgd7/2Xg9G3Z0P/ezevJTs3yB6eT2zf3NexIuAVn3/t+qkRNeB6K03x75/ju73JwC8/M87cjJsczgBZr9XAW33G/UyXH+jBJbR+/0L4Om/3yDF2/4EiK4tLjCZ3M8LaKdf6hvmlZ+nBPYU2z8FxVri7PcugKFtUrTVW0B0eM9o98/OiYy/QMWsfVKsDaAE/ud5vwrMANNgpRTrIJgaWvzu16ZzAgP3SJEKu3kBfr9/AbywT5LbBk7g1ZHu/zS1nxd4sVeS2wyqbhWg9vsXGHxbEuv8NimwkNg/rgLPPyKJLQLX/IWl9+/V/YxAD6jeKnIG9rICZz5+e7rfqeJS4ePPwNQ6SerhLeCqrhSR6UOAf4GZoPpGqyQ1G7SA7vcr0HsCVE2S1ORraIEFQ4AJgT9JUksAPhsCA68nvujT4Qgc65SEnkI4AjsloeMfhiNQcU4Suh3hCPS0itttCEhggiQ0aEzgFPPzZbK4vQFbApMvx9hbKW7rKgISOF0QtxcRkECXuL2E8RPoKrfA7rw4XagYR4EJ5Rb4o7jNREACz4rbJAQk0LFOnH6OkAQ2idMvdpgU+CrG1CFxOwGLAnXTMJY6povTPIQk0MY9NGNf4Ifi9B2EJDBTnPorghK4QZweQEgCq8XpCyhDXSJ8ua9h1P1BnOYCNs9Arhaj7xlxugIGBXQ/f5PoHpgR4PfjlDh9D2YE+P04LE6rYEaA34+OvFxcJQwJuPv5C4H1sCPA78fX5eJWwI4Avx973KenYUiA/9PWSnGCQQHrALyAdQBewDoAL2AdgBewDsALWAfgBawD8ALWAXgB6wC8gHUAXsA6AC9gHYAX4AGsC9AA5gVYAPsCBgCIZ8kWlxaYRQJk+QxUfaq6kjgDxgBUQPcDhIA5ABXQ/YSAOQAV0P2EgDkAFdD9hIBBABXQ/YSAQQAV0P2EgEEAFdD9hIBdAFScAkYvsNcwgNsYBHZxAPYFaro5APsCNxMAJgTOyPAtJQBMdLqEQE0DB2BfoJEDsC+wiAOwL5Bv4ADsCzRxAPYFTnIA9gXyhzkA+wLtHIB9gTs4APsC+WYOwL5AOwdgX6CNA7AvkG8OBABXzv1xqyQ0jwCw1um5r/Q5ixYEAaAGL/z1IoNccxgAWvfNH9SIaNcHA6A1NC5Vg88HBaAGTSc/MchtDAtAO9zeltfPQEgAWvO8BXn5UYgA2rXL25pHAxCLxWKxWCwWi8VisVgsFovFYrGA+gjhuQDdoLZCkAAAAABJRU5ErkJggg==');mask-image:url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAQAAAAEACAMAAABrrFhUAAAAAXNSR0IB2cksfwAAAn9QTFRFAAAAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICAAICA/S6DpQAAANV0Uk5TAAGa/8vn/mEZDv2X+/zuKErwKjH0L+wr8SxOZG5qElRVFNJn+bRWUFcPtXELzuUjsd0NbLDWFViI6VmJ77yYY1ODlrdJsxidx9GHj8E3QUMf6zu2OsiKMnMwSOI9wosivx2okuBwuKqCCXSp8kbcw2YGRHaluZR/eq1RbSFMb5Cc1dNH3tmGNqs5PGsMfE8+G1I0gQis0DjPfsoKYOTa2BfhmxaeaAdlGr74A3WuHtRF9l9aXEATWwJeNfeZJ+gR6ikt7foz9bIFLvMQzSWvBE3JJOYmwH4nZQAACBlJREFUeNrt3fdzVOUex/FP+IZg1iwkoBAukGYSyhUFARERe7te9aq39957793ee6GpNOm9BBOIITGEhSglf5A/6Mx3hudsNsnnPNnzned5/wE7+3nNsztn55yZRSwWi8VisVgsFovFYrFYLBaLxWKANnTmfNj7J8qF82HvFxUIc78KhLlfBcLcrwJh7g9TQPeHKaD7wxTQ/WEK6P4wBXR/mAK6P0wB3R+mgO4PU0D3hymg+8MU0P1hCuj+MAV0f5gCuj9MAd0fpoDuD1NA94cpoPtNCRz5ypLaoQEP+60InBMRyU389Ybl88uynxfgAT4pt2fT3g5+vx0BBdCO3zhI7jcloABa7rb3mf3mBBRA2/YYsd+cgAJoVY9vJPabEVAAt+kHiP12BBTAbdNvif28QPkBpKWH2M8LlB9A9p3FiNsgaXTV+UwBSH4/RtqHH4hk7QzwAFI126QAD6DdZFuAB8g9O3KBXekItJ8t2WveAbTt9xFnwFdV73gH0B7cnUWBm7wDaK8PZFFgkncA7S/IosB+pNShI3NurMzLMNWcyKJA7ihS7Mr7bqmXorUMZFLgCJCqwYH/SrHeQSYFDiDlGnslufrubAosR8ptLPbedyKTAvl5SLu1rclHoDqjAk1Iu0MFSWoOsilQ04i0e61fErqwIxyB/+cloeeQVYG5SLtbJKEuhCOw45y49a/JrsBZpNyUVnH7HDIr0Je6wPfF7TpkWKAW6dZdEKdlyKqAe53C1yVuzdkV6HwXKTdF3J4EIWBkv/YZcdoPQsDaftwqTl8CIWBtP2rFaSkIAWv7MV+cWpBFgf534af35OJWYKwCizzufx+a52/BPMbawEl7+3GvOF1DCJjbjzZxuhOEgLX9WMVdCroCxvbjbvIj4ArY2o8fuD+7K8AJmNq/Je8+wAOQAob2435x+iZoATv78VlxmoDsCFzmez8WitNTyIzAZdPguSlV4vQ78G01sh+Xitsl4LvEyP6GgjgtRhoANvbjVnFbmQqAjf1f7iPuC9AA5d/f0SJunT8tAwC7n78G0O5CKgAW9h8k7g7zAOXff3a7JLS+wwNAJvcf6JSk1qL8AIXL4b+f1EhSvWvKD1A4Be9d/UXiQUEaoPz7f98vyf1qi3eA8u/vfnSdFOuX8AbA77/+avB1N+3qlKK9Am8A/P4jfVdxAmtX3rVqcU6GqX+rRwB+vwgncE5K1Q5fAPz+g30iIhMHfQL8B74A+P1H+0RUwA/AmTU+Afj9KuAHYMV3UV6A+hMl9quAD4DCVJQXoH5mif0q4AOgfxo8AfD79+t+FUgboHfngB8Afv+kGhFHgAZwe3tzh28Afr8K8ABub97uF4DfrwLHWIDkTm71CcDv144f8wIghWd2pAjA75+l+0sIkADa6n94BeD3q4AfACn8xjsAv18FeAC3tupxBKibMer9KuAJQJY9MG4AdVOH3U8IMADy4EM0AL3/ihoRQoADkNaDLAC9Py9CCLAAUjWbBPC7XwV6MOJmHKptPPqvrrv3yYj6Mwfgd7/2Xg9G3Z0P/ezevJTs3yB6eT2zf3NexIuAVn3/t+qkRNeB6K03x75/ju73JwC8/M87cjJsczgBZr9XAW33G/UyXH+jBJbR+/0L4Om/3yDF2/4EiK4tLjCZ3M8LaKdf6hvmlZ+nBPYU2z8FxVri7PcugKFtUrTVW0B0eM9o98/OiYy/QMWsfVKsDaAE/ud5vwrMANNgpRTrIJgaWvzu16ZzAgP3SJEKu3kBfr9/AbywT5LbBk7g1ZHu/zS1nxd4sVeS2wyqbhWg9vsXGHxbEuv8NimwkNg/rgLPPyKJLQLX/IWl9+/V/YxAD6jeKnIG9rICZz5+e7rfqeJS4ePPwNQ6SerhLeCqrhSR6UOAf4GZoPpGqyQ1G7SA7vcr0HsCVE2S1ORraIEFQ4AJgT9JUksAPhsCA68nvujT4Qgc65SEnkI4AjsloeMfhiNQcU4Suh3hCPS0itttCEhggiQ0aEzgFPPzZbK4vQFbApMvx9hbKW7rKgISOF0QtxcRkECXuL2E8RPoKrfA7rw4XagYR4EJ5Rb4o7jNREACz4rbJAQk0LFOnH6OkAQ2idMvdpgU+CrG1CFxOwGLAnXTMJY6povTPIQk0MY9NGNf4Ifi9B2EJDBTnPorghK4QZweQEgCq8XpCyhDXSJ8ua9h1P1BnOYCNs9Arhaj7xlxugIGBXQ/f5PoHpgR4PfjlDh9D2YE+P04LE6rYEaA34+OvFxcJQwJuPv5C4H1sCPA78fX5eJWwI4Avx973KenYUiA/9PWSnGCQQHrALyAdQBewDoAL2AdgBewDsALWAfgBawD8ALWAXgB6wC8gHUAXsA6AC9gHYAX4AGsC9AA5gVYAPsCBgCIZ8kWlxaYRQJk+QxUfaq6kjgDxgBUQPcDhIA5ABXQ/YSAOQAV0P2EgDkAFdD9hIBBABXQ/YSAQQAV0P2EgEEAFdD9hIBdAFScAkYvsNcwgNsYBHZxAPYFaro5APsCNxMAJgTOyPAtJQBMdLqEQE0DB2BfoJEDsC+wiAOwL5Bv4ADsCzRxAPYFTnIA9gXyhzkA+wLtHIB9gTs4APsC+WYOwL5AOwdgX6CNA7AvkG8OBABXzv1xqyQ0jwCw1um5r/Q5ixYEAaAGL/z1IoNccxgAWvfNH9SIaNcHA6A1NC5Vg88HBaAGTSc/MchtDAtAO9zeltfPQEgAWvO8BXn5UYgA2rXL25pHAxCLxWKxWCwWi8VisVgsFovFYrGA+gjhuQDdoLZCkAAAAABJRU5ErkJggg==');-webkit-mask-repeat:no-repeat;mask-repeat:no-repeat;-webkit-mask-position:center;mask-position:center;-webkit-mask-size:contain;mask-size:contain}"
            ".tool-icon-box.with-stand{width:76px;height:52px}"
            ".tool-icon-box.is-work .tool-iron{left:50%;top:8px;width:34px;height:34px;transform:translateX(-50%) rotate(-12deg)}"
            ".tool-icon-box.stand-only .tool-stand-base{position:absolute;left:50%;bottom:10px;width:34px;height:6px;transform:translateX(-50%);border-radius:999px;background:var(--tool-color);opacity:.92}"
            ".tool-icon-box.stand-only .tool-stand-post{position:absolute;left:50%;bottom:16px;width:7px;height:15px;transform:translateX(-50%);border-radius:999px;background:var(--tool-color);opacity:.94}"
            ".tool-icon-box.stand-only .tool-stand-cradle{position:absolute;left:calc(50% + 2px);bottom:29px;width:19px;height:5px;border-radius:999px;background:var(--tool-color);transform:rotate(-26deg);transform-origin:left center;opacity:.96}"
            ".tool-icon-box.stand-only .tool-stand-hook{position:absolute;left:calc(50% + 17px);bottom:27px;width:10px;height:10px;border-right:3px solid var(--tool-color);border-bottom:3px solid var(--tool-color);border-radius:0 0 12px 0;transform:rotate(12deg);opacity:.94}"
            ".tool-badge{position:absolute;right:8px;top:9px;display:flex;align-items:center;justify-content:center;width:30px;height:30px;border-radius:999px;font-size:21px;font-weight:800;line-height:1;box-sizing:border-box}"
            ".tool-icon-box.is-work .tool-badge{right:8px;top:9px}"
            ".tool-icon-box.stand-only .tool-badge{right:8px;top:9px}"
            ".tool-badge-check{background:#1f7f45;color:#fff;border:2px solid #49df83;box-shadow:0 0 10px rgba(59,209,111,.24)}"
            ".tool-badge-cycle{background:rgba(255,255,255,.06);color:#f2f2f2;border:2px solid rgba(255,255,255,.30)}"
            ".tool-state-label{font-size:12px;color:#cfd3da;text-transform:uppercase;letter-spacing:.6px;font-weight:700}"
            ".led-text.led-solid-green{color:#00d26a;text-shadow:0 0 10px rgba(0,210,106,.9),0 0 22px rgba(0,210,106,.45);opacity:1}"
            ".led-text.led-breathe-red{color:#ff3b30;animation:textBreatheRed 2.2s ease-in-out infinite;opacity:1}"
            ".led-text.led-blink-red{color:#ff3b30;animation:textBlinkRed 1s steps(1,end) infinite;opacity:1}"
            ".led-text.led-blink-yellow{color:#ffd400;animation:textBlinkYellow 1s steps(1,end) infinite;opacity:1}"
            ".led-text.led-breathe-blue{color:#2f80ff;animation:textBreatheBlue 2.2s ease-in-out infinite;opacity:1}"
            ".led-text.led-blink-blue{color:#2f80ff;animation:textBlinkBlue 1s steps(1,end) infinite;opacity:1}"
            ".led-text.led-blink-magenta{color:#ff33cc;animation:textBlinkMagenta 1s steps(1,end) infinite;opacity:1}"
            ".led-text.led-breathe-white{color:#f2f2f2;animation:textBreatheWhite 2.2s ease-in-out infinite;opacity:1}"
            "@keyframes textBreatheRed{0%,100%{opacity:.28;text-shadow:0 0 3px rgba(255,59,48,.22)}50%{opacity:1;text-shadow:0 0 10px rgba(255,59,48,.9),0 0 22px rgba(255,59,48,.4)}}"
            "@keyframes textBreatheBlue{0%,100%{opacity:.28;text-shadow:0 0 3px rgba(47,128,255,.22)}50%{opacity:1;text-shadow:0 0 10px rgba(47,128,255,.9),0 0 22px rgba(47,128,255,.4)}}"
            "@keyframes textBreatheWhite{0%,100%{opacity:.25;text-shadow:0 0 3px rgba(255,255,255,.18)}50%{opacity:1;text-shadow:0 0 10px rgba(255,255,255,.9),0 0 22px rgba(255,255,255,.32)}}"
            "@keyframes textBlinkYellow{0%,49%{opacity:1;text-shadow:0 0 10px rgba(255,212,0,.9),0 0 22px rgba(255,212,0,.38)}50%,100%{opacity:.18;text-shadow:0 0 2px rgba(255,212,0,.14)}}"
            "@keyframes textBlinkBlue{0%,49%{opacity:1;text-shadow:0 0 10px rgba(47,128,255,.9),0 0 22px rgba(47,128,255,.38)}50%,100%{opacity:.18;text-shadow:0 0 2px rgba(47,128,255,.14)}}"
            "@keyframes textBlinkMagenta{0%,49%{opacity:1;text-shadow:0 0 10px rgba(255,51,204,.9),0 0 22px rgba(255,51,204,.38)}50%,100%{opacity:.18;text-shadow:0 0 2px rgba(255,51,204,.14)}}"
            "@keyframes textBlinkRed{0%,49%{opacity:1;text-shadow:0 0 10px rgba(255,59,48,.9),0 0 22px rgba(255,59,48,.38)}50%,100%{opacity:.18;text-shadow:0 0 2px rgba(255,59,48,.14)}}</style></head><body>");
  html += F("<h1>JBC FAE Emulator</h1><div class='muted' style='margin:-8px 0 16px 0'>FW "); html += F(USER_FW_VERSION); html += F(" / by IceCube20</div>");
  if (g_cfg_portal_active){
    html += F("<div class='card'><strong>Setup AP active:</strong> "); html += g_cfg_ap_ssid; html += F(" / "); html += WiFi.softAPIP().toString();
    const char* ap_pwd = netcfg_ap_password(); if (strlen(ap_pwd) >= 8) { html += F(" / Password: <strong>"); html += html_escape(String(ap_pwd)); html += F("</strong>"); }
    html += F("</div>");
  }
  html += F("<div class='nav'><a href='/control'>Control</a><a href='/config'>Network setup</a><a href='/cli'>CLI</a><a href='/update'>Firmware update</a><a href='/api/state'>JSON</a><a href='/reboot'>Reboot</a></div>");
  html += F("<div class='card'><span class='live-dot'></span><strong>Live status</strong><div class='muted' style='margin-top:6px'>Values update directly from /api/state without reloading the page.</div><div class='quick-grid'>");
  html += F("<div class='quick-card'><span class='k'>Relay</span><span class='v' id='relay_state_quick'>"); html += (g_relay_on ? F("ON") : F("OFF")); html += F("</span><span class='quick-sub afterrun-strong' id='relay_afterrun_quick'>Afterrun "); html += String(g_relay_off_deadline ? (relay_time_left_ms() / 1000UL) : 0UL); html += F(" s</span></div>");
  html += F("<div class='quick-card'><span class='k'>Error</span><span class='v' id='error_status_quick'>"); html += html_escape(current_error_label()); html += F("</span></div>");
  html += F("<div class='quick-card'><span class='k'>Continuous</span><span class='v' id='continuous_state_quick'>"); html += (st.continuous ? F("ON") : F("OFF")); html += F("</span></div>");
  html += F("<div class='quick-card'><span class='k'>MQTT</span><span class='v' id='mqtt_status_quick'>"); html += (g_netcfg.mqtt_enable ? F("enabled") : F("disabled")); html += F("</span></div>");
  html += F("<div class='quick-card'><span class='k'>WiFi</span><span class='v' id='wifi_status_quick'>"); html += (WiFi.isConnected() ? F("connected") : F("not connected")); html += F("</span></div>");
  uint8_t wifiPctQuick = WiFi.isConnected() ? wifi_signal_pct_from_rssi(WiFi.RSSI()) : 0;
  uint8_t wifiBarsQuick = (wifiPctQuick >= 75) ? 4 : (wifiPctQuick >= 50 ? 3 : (wifiPctQuick >= 25 ? 2 : (wifiPctQuick > 0 ? 1 : 0)));
  html += F("<div class='quick-card'><span class='k'>WiFi signal</span><div class='quick-signal'><span class='signal-bars level-"); html += String(wifiBarsQuick); html += F("' id='wifi_bars_quick'><i></i><i></i><i></i><i></i></span><span class='v signal-text' id='wifi_signal_quick'>"); html += wifi_signal_string(); html += F("</span></div></div>");
  html += F("<div class='quick-card'><span class='k'>CPU load</span><div class='quick-cpu'><span class='v' id='cpu_load_quick'>"); html += String(cpu_load_pct()); html += F(" %</span><span class='quick-meter'><span id='cpu_fill_quick' class='quick-meter-fill' style='width:"); html += String(cpu_load_pct()); html += F("%'></span></span></div></div>");
  html += F("</div></div>");
  html += F("<div class='card'><strong>Front panel</strong><div class='front-grid'>");
  html += F("<div class='front-tile'><div class='front-head'><span class='front-title'>JBC1</span></div><div class='bus-leds'><div class='led-wrap'><span id='led_jbc1' class='led-text led-"); html += led_link_mode_key(0); html += F("'>JBC1</span></div><div class='led-wrap'><span id='led_evt1' class='led-text led-"); html += led_event_mode_key(0); html += F("'>EVT</span></div></div><div class='front-sub'><div class='front-meta'><span class='k'>Bus</span><span class='v' id='front0_bus'>Bus 0</span></div><div class='front-meta'><span class='k'>Type</span><span class='v' id='front0_type'>"); html += html_escape((const char*)station_name_from_addr(g_bus[0].my_addr)); html += F("</span></div><div class='front-meta'><span class='k'>Address</span><span class='v' id='front0_addr'>0x"); if (g_bus[0].my_addr < 16) html += '0'; html += String(g_bus[0].my_addr, HEX); html += F("</span></div><div class='front-meta'><span class='k'>Link</span><span class='v' id='front0_link'>"); html += link_state_name(g_bus[0].linkState); html += F("</span></div></div><div class='front-sensors'><div class='front-sensor-card'><span class='k'>WORK</span><div id='front0_work'>"); html += tool_sensor_html(0, WORK_IDX); html += F("</div></div><div class='front-sensor-card'><span class='k'>STAND</span><div id='front0_stand'>"); html += tool_sensor_html(0, STAND_IDX); html += F("</div></div><div class='front-sensor-card'><span class='k'>Relay state</span><div class='relay-chip-wrap' id='front0_relay'>"); html += relay_state_chip_html(0); html += F("</div></div></div><div class='front-device' id='front0_hint'>Device ID: "); html += html_escape(bus_device_id_string(0)); html += F("</div></div>");
  html += F("<div class='front-tile'><div class='front-head'><span class='front-title'>JBC2</span></div><div class='bus-leds'><div class='led-wrap'><span id='led_jbc2' class='led-text led-"); html += led_link_mode_key(1); html += F("'>JBC2</span></div><div class='led-wrap'><span id='led_evt2' class='led-text led-"); html += led_event_mode_key(1); html += F("'>EVT</span></div></div><div class='front-sub'><div class='front-meta'><span class='k'>Bus</span><span class='v' id='front1_bus'>Bus 1</span></div><div class='front-meta'><span class='k'>Type</span><span class='v' id='front1_type'>"); html += html_escape((const char*)station_name_from_addr(g_bus[1].my_addr)); html += F("</span></div><div class='front-meta'><span class='k'>Address</span><span class='v' id='front1_addr'>0x"); if (g_bus[1].my_addr < 16) html += '0'; html += String(g_bus[1].my_addr, HEX); html += F("</span></div><div class='front-meta'><span class='k'>Link</span><span class='v' id='front1_link'>"); html += link_state_name(g_bus[1].linkState); html += F("</span></div></div><div class='front-sensors'><div class='front-sensor-card'><span class='k'>WORK</span><div id='front1_work'>"); html += tool_sensor_html(1, WORK_IDX); html += F("</div></div><div class='front-sensor-card'><span class='k'>STAND</span><div id='front1_stand'>"); html += tool_sensor_html(1, STAND_IDX); html += F("</div></div><div class='front-sensor-card'><span class='k'>Relay state</span><div class='relay-chip-wrap' id='front1_relay'>"); html += relay_state_chip_html(1); html += F("</div></div></div><div class='front-device' id='front1_hint'>Device ID: "); html += html_escape(bus_device_id_string(1)); html += F("</div></div></div></div>");
  html += F("<div class='card'><table>");
  html += F("<tr><th>Item</th><th>Value</th></tr>");
  html += F("<tr><td>WiFi</td><td id='wifi_status'>"); html += (WiFi.isConnected() ? F("connected") : F("not connected")); html += F("</td></tr>");
  html += F("<tr><td>IP</td><td id='ip_addr'>"); html += active_ip_string(); html += F("</td></tr>");
  html += F("<tr><td>Firmware</td><td id='fw_version'>"); html += F(USER_FW_VERSION); html += F("</td></tr>");
  html += F("<tr><td>WiFi signal</td><td id='wifi_signal'>"); html += wifi_signal_string(); html += F("</td></tr>");
  html += F("<tr><td>CPU load (approx)</td><td id='cpu_load'>"); html += String(cpu_load_pct()); html += F(" %</td></tr>");
  html += F("<tr><td>Afterrun remaining</td><td id='afterrun_remaining'>"); html += String(g_relay_off_deadline ? (relay_time_left_ms() / 1000UL) : 0UL); html += F(" s</td></tr>");
  html += F("<tr><td>Afterrun work</td><td id='afterrun_work'>"); html += String(st.delaySec[0][WORK_IDX]); html += F(" s</td></tr>");
  html += F("<tr><td>Afterrun stand</td><td id='afterrun_stand'>"); html += String(st.delaySec[0][STAND_IDX]); html += F(" s</td></tr>");
  html += F("<tr><td>Relay</td><td id='relay_state'>"); html += (g_relay_on ? F("ON") : F("OFF")); html += F("</td></tr>");
  html += F("<tr><td>Relay mode</td><td id='relay_mode'>"); html += relay_mode_label(); html += F("</td></tr>");
  html += F("<tr><td>LED brightness</td><td id='led_brightness'>"); html += String(led_brightness_raw_to_pct(g_led_brightness)); html += F(" %</td></tr>");
  html += F("<tr><td>Continuous</td><td id='continuous_state'>"); html += (st.continuous ? F("ON (blue blink)") : F("OFF")); html += F("</td></tr>");
  html += F("<tr><td>Suction level</td><td id='suction_level'>"); html += suction_level_label(st.suctionLevel); html += F(" ("); html += String(st.suctionLevel); html += F(")</td></tr>");
  html += F("<tr><td>Select flow</td><td id='select_flow'>"); html += String(selectflow_raw_to_pct(st.selectFlow)); html += F(" %</td></tr>");
  html += F("<tr><td>Actual flow</td><td id='actual_flow'>"); html += String(actualflow_raw_to_pct(st.flow_x1000)); html += F(" %</td></tr>");
  html += F("<tr><td>Filter life</td><td id='filter_life'>"); html += String(filter_raw_to_pct(st.filterLife)); html += F(" %</td></tr>");
  html += F("<tr><td>Filter saturation</td><td id='filter_saturation'>"); html += String(filter_raw_to_pct(st.filterSaturation)); html += F(" %</td></tr>");
  html += F("<tr><td>Status error</td><td id='status_error'>"); html += String(st.statError); html += F(" ("); html += error_level_name(st.statError); html += F(")</td></tr>");
  html += F("<tr><td>Error preset</td><td id='error_preset'>"); html += html_escape(current_error_label()); html += F("</td></tr>");
  html += F("</table></div>");
#if MQTT_ENABLE
  html += F("<div class='card'><strong>MQTT:</strong> "); html += (g_netcfg.mqtt_enable ? F("enabled") : F("disabled"));
  if (g_netcfg.mqtt_enable){ html += F(" / "); html += g_netcfg.mqtt_host; html += ':'; html += String(g_netcfg.mqtt_port); html += F(" / topic "); html += g_mqtt_base_topic; html += F(" / commands "); html += g_mqtt_base_topic; html += F("/cmd/#"); }
  html += F("</div>");
#endif
  html += F("<div class='card'><strong>JSON state:</strong><pre id='json_state'>"); html += html_escape(json); html += F("</pre></div>");
  html += F("<script>(function(){"
            "function setText(id,value){var el=document.getElementById(id);if(el){el.textContent=value;}}"
            "function busText(index,data){"
              "var station=(index===0?data.bus0_station:data.bus1_station)||'UNKNOWN';"
              "var link=(index===0?data.bus0_link:data.bus1_link)||'disconnected';"
              "var dev=(index===0?data.bus0_device_id:data.bus1_device_id)||'-';"
              "var label='disconnected';"
              "if(index>=data.bus_count){label='inactive';}"
              "else if(link==='connected'){label='connected';}"
              "else if(link==='connecting'){label='connecting';}"
              "return 'Model: '+station+' '+label+' / Device ID: '+(dev||'-');"
            "}"
            "function toolHtml(v,busIndex,data,intake){"
              "var inactiveBus = busIndex >= data.bus_count;"
              "var active = (!inactiveBus) && !!v;"
              "var isWork = (intake === 0);"
              "var showStand = (intake === 1);"
              "var color = active ? '#3bd16f' : (inactiveBus ? '#777777' : '#f2f2f2');"
              "var glow = active ? '0 0 12px rgba(59,209,111,.26)' : (inactiveBus ? 'none' : '0 0 7px rgba(255,255,255,.08)');"
              "var html = '<div class=\"tool-status-wrap\"><span class=\"tool-icon-box'+(showStand ? ' with-stand' : '')+(isWork ? ' is-work' : (showStand ? ' stand-only' : ''))+'\" style=\"--tool-color:'+color+';--tool-glow:'+glow+'\">';"
              "if(isWork){html += '<span class=\"tool-iron\"></span>';}"
              "else if(showStand){html += '<span class=\"tool-stand-base\"></span><span class=\"tool-stand-post\"></span><span class=\"tool-stand-cradle\"></span><span class=\"tool-stand-hook\"></span>';}"
              "var stateCls = inactiveBus ? 'state-chip state-inactive tool-state-chip' : (active ? 'state-chip tool-state-active tool-state-chip' : 'state-chip state-idle tool-state-chip');"\
              "html += '</span><span class=\"'+stateCls+'\">' + (active ? 'active' : 'inactive') + '</span></div>';"
              "return html;"
            "}"
            "function wifiSignalText(data){"
              "if(data.wifi_connected){return data.wifi_rssi+' dBm ('+data.wifi_signal_pct+' %)';}"
              "if(data.portal_active){return 'setup AP active';}"
              "return 'n/a';"
            "}"
            "function wifiBarsLevel(data){if(!data.wifi_connected)return 0;var p=Number(data.wifi_signal_pct)||0;return p>=75?4:(p>=50?3:(p>=25?2:(p>0?1:0)));}"
            "function setWifiBars(id,level){var el=document.getElementById(id);if(!el)return;el.className='signal-bars '+(level>0?('level-'+level):'off');}"
            "function clampPct(v){v=Number(v)||0;if(v<0)v=0;if(v>100)v=100;return Math.round(v);}"
            "function setLed(id,mode){var el=document.getElementById(id);if(!el)return;el.className='led-text led-'+(mode||'off');}"
            "function setHtml(id,html){var el=document.getElementById(id);if(el)el.innerHTML=html;}"
            ""
            "function hexByte(v){var n=Number(v)||0;var s=n.toString(16).toUpperCase();return '0x'+(s.length<2?'0':'')+s;}"
            "function linkText(index,data){return (index===0?data.bus0_link:data.bus1_link)||'disconnected';}"
            "function setFrontState(index,data){var enabled=index < data.bus_count;var relayState=(index===0?data.bus0_relay_state:data.bus1_relay_state)||'idle';var station=(index===0?data.bus0_station:data.bus1_station)||'UNKNOWN';var dev=(index===0?data.bus0_device_id:data.bus1_device_id)||'-';var addr=(index===0?data.bus0_addr:data.bus1_addr);var chip=document.getElementById(index===0?'front0_state':'front1_state');var state=relayState;var cls='state-chip state-idle';if(!enabled){state='inactive';cls='state-chip state-inactive';}else if(relayState==='afterrun'){cls='state-chip state-afterrun';}else if(relayState==='continuous'){cls='state-chip state-continuous';}else if(relayState==='work'){state='active';cls='state-chip state-active';}else if(relayState==='inactive'){cls='state-chip state-inactive';}else{state='idle';}setText(index===0?'front0_bus':'front1_bus','Bus '+index);setText(index===0?'front0_type':'front1_type',station);setText(index===0?'front0_addr':'front1_addr',enabled?hexByte(addr):'--');setText(index===0?'front0_link':'front1_link',enabled?linkText(index,data):'inactive');setText(index===0?'front0_hint':'front1_hint','Device ID: '+dev);setHtml(index===0?'front0_work':'front1_work', toolHtml(index===0?data.bus0_work_active:data.bus1_work_active,index,data,0));setHtml(index===0?'front0_stand':'front1_stand', toolHtml(index===0?data.bus0_stand_active:data.bus1_stand_active,index,data,1));var relay=index===0?document.getElementById('front0_relay'):document.getElementById('front1_relay');if(relay){var relayCls='state-chip state-idle';var relayText=relayState;if(index>=data.bus_count){relayCls='state-chip state-inactive';relayText='inactive';}else if(relayState==='continuous'){relayCls='state-chip state-continuous';relayText='continuous';}else if(relayState==='afterrun'){relayCls='state-chip state-afterrun';relayText='afterrun';}else if(relayState==='work'){relayCls='state-chip state-active';relayText='work';}else if(relayState==='inactive'){relayCls='state-chip state-inactive';relayText='inactive';}else{relayText='idle';}relay.innerHTML='<span class=\"'+relayCls+'\">'+relayText+'</span>'; }if(chip){chip.className=cls;chip.textContent=state;}}"
            "function applyState(data){"
              "setText('wifi_status', data.wifi_connected ? 'connected' : 'not connected');"
              "setText('wifi_status_quick', data.wifi_connected ? 'connected' : 'not connected');"
              "setText('mqtt_status_quick', data.mqtt_enabled ? 'enabled' : 'disabled');"
              "setText('relay_state_quick', data.relay ? 'ON' : 'OFF');"
              "setText('continuous_state_quick', data.continuous ? 'ON' : 'OFF');"
              "setText('ip_addr', data.ip || '0.0.0.0');"
              "setText('wifi_signal', wifiSignalText(data));"
              "setText('wifi_signal_quick', wifiSignalText(data));"
              "setWifiBars('wifi_bars_quick', wifiBarsLevel(data));"
              "setText('cpu_load', data.cpu_load_pct+' %');"
              "setText('cpu_load_quick', data.cpu_load_pct+' %');"
              "var cpuFill=document.getElementById('cpu_fill_quick');if(cpuFill)cpuFill.style.width=clampPct(data.cpu_load_pct)+'%';"
              "setText('afterrun_remaining', data.afterrun_remaining_s+' s');"
              "setText('relay_afterrun_quick','Afterrun '+data.afterrun_remaining_s+' s');setText('error_status_quick', data.error_label || (data.error_key==='NOERR' ? 'No error' : (data.error_key+' - '+data.error_text)));"
              "setText('afterrun_work', data.delay_work_s+' s');"
              "setText('afterrun_stand', data.delay_stand_s+' s');"
              "setText('relay_state', data.relay ? 'ON' : 'OFF');"
              "setText('relay_mode', data.relay_inverted ? 'Öffner (NC)' : 'Schließer (NO)');"
              "setText('led_brightness', data.led_brightness+' %');"
              "setText('continuous_state', data.continuous ? 'ON (blue blink)' : 'OFF');"
              "setText('suction_level', (data.suction_level_label||data.suction_level) + ' (' + data.suction_level + ')');"
              "setText('select_flow', data.select_flow+' %');"
              "setText('actual_flow', data.actual_flow+' %');"
              "setText('filter_life', data.filter_life+' %');"
              "setText('filter_saturation', data.filter_saturation+' %');"
              "setText('status_error', data.stat_error+' ('+data.error_level+')');"
              "setText('error_preset', data.error_key==='NOERR' ? 'No error' : (data.error_key+' - '+data.error_text));"
              ""
              "setFrontState(0,data);"
              ""
              ""
              ""
              ""
              "setFrontState(1,data);"
              ""
              ""
              ""
              "setLed('led_jbc1', data.led_jbc1_mode);"
              "setLed('led_evt1', data.led_evt1_mode);"
              "setLed('led_jbc2', data.led_jbc2_mode);"
              "setLed('led_evt2', data.led_evt2_mode);"




              "setText('json_state', JSON.stringify(data,null,2));"
            "}"
            "function refreshState(){"
              "fetch('/api/state?_=' + Date.now(), {cache:'no-store'})"
                ".then(function(r){ if(!r.ok) throw new Error('HTTP '+r.status); return r.json(); })"
                ".then(applyState)"
                ".catch(function(){});"
            "}"
            "refreshState();"
            "setInterval(refreshState, 1500);"
          "})();</script>");
  html += F("</body></html>");
  return html;
}
static bool web_auth_enabled(){
  return g_netcfg.web_username[0] && g_netcfg.web_password[0];
}

static bool web_require_auth(){
  if (!web_auth_enabled()) return true;
  if (g_http.authenticate(g_netcfg.web_username, g_netcfg.web_password)) return true;
  g_http.requestAuthentication(BASIC_AUTH, "JBC FAE Emulator");
  return false;
}

static String web_html_config(const String& msg = String()){
  String html;
  html.reserve(7600);
  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>JBC FAE Setup</title><style>*{box-sizing:border-box}body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:24px}"
            "input,select,textarea{display:block;width:100%;max-width:100%;min-width:0;padding:10px;border-radius:8px;border:1px solid #444;background:#1d1d1d;color:#eee}"
            "label{display:block;margin:12px 0 6px}.card{background:#1d1d1d;padding:16px;border-radius:12px;margin:12px 0;overflow:hidden}"
            ".nav{display:flex;flex-wrap:wrap;gap:10px;margin:0 0 14px 0}.actions{display:flex;flex-wrap:wrap;gap:10px;margin:16px 0 0 0;padding-bottom:6px}.nav a,.nav button,.actions a,.actions button{display:inline-flex;align-items:center;justify-content:center;margin:0;padding:10px 14px;border-radius:10px;background:#2a6df4;color:#fff;text-decoration:none;border:0;max-width:100%}"
            "small{color:#aaa}</style></head><body>");
  html += F("<h1>Network setup</h1><div style='color:#aaa;margin:-8px 0 16px 0'>FW "); html += F(USER_FW_VERSION); html += F(" / by IceCube20</div>");
  if (msg.length()) { html += F("<div class='card'><strong>"); html += html_escape(msg); html += F("</strong></div>"); }
  if (g_cfg_portal_active){
    html += F("<div class='card'>Connect to setup AP <strong>"); html += g_cfg_ap_ssid; html += F("</strong>");
    const char* ap_pwd = netcfg_ap_password(); if (strlen(ap_pwd) >= 8) { html += F(" using password <strong>"); html += html_escape(String(ap_pwd)); html += F("</strong>"); }
    html += F(" and open <strong>"); html += WiFi.softAPIP().toString(); html += F("</strong>.</div>");
  }
  html += F("<div class='nav'><a href='/'>Status</a><a href='/control'>Control</a><a href='/cli'>CLI</a><a href='/update'>Firmware update</a></div>");
  html += F("<div class='card'><strong>Current WiFi signal:</strong> "); html += wifi_signal_string();
  if (WiFi.isConnected()) { html += F(" / SSID: "); html += html_escape(String(WiFi.SSID())); }
  html += F("</div>");

  html += F("<div class='card'><strong>Detected WLANs</strong><br>");
  html += F("<small>Select a found network to copy its SSID into the field below.</small><br>");
  if (g_wifi_scan_count > 0){
    html += F("<label>Found networks</label><select id='ssid_pick' onchange=\"if(this.value){document.getElementById('wifi_ssid').value=this.value;}\">");
    html += F("<option value=''>Select WLAN…</option>");
    for (uint8_t i = 0; i < g_wifi_scan_count; ++i){
      html += F("<option value='");
      html += html_escape(String(g_wifi_scan[i].ssid));
      html += F("'>");
      html += html_escape(String(g_wifi_scan[i].ssid));
      html += F(" (");
      html += String(g_wifi_scan[i].rssi);
      html += F(" dBm, ");
      html += String(wifi_signal_pct_from_rssi(g_wifi_scan[i].rssi));
      html += F(" %, ");
      html += wifi_auth_label(g_wifi_scan[i].auth);
      html += F(")</option>");
    }
    html += F("</select>");
  } else {
    html += F("<small>No WLANs found yet.</small><br>");
  }
  html += F("<div class='actions' style='margin-top:16px;padding-bottom:4px'><a href='/config?scan=1'>Scan again</a></div></div>");

  html += F("<form method='post' action='/save'><div class='card'>");
  html += F("<label>Hostname</label><input name='wifi_hostname' maxlength='32' value='"); html += cfg_value(g_netcfg.wifi_hostname); html += F("'>");
  html += F("<label>WiFi SSID</label><input id='wifi_ssid' name='wifi_ssid' maxlength='32' value='"); html += cfg_value(g_netcfg.wifi_ssid); html += F("'>");
  html += F("<label>WiFi password</label><input name='wifi_password' type='password' maxlength='64' value='"); html += cfg_value(g_netcfg.wifi_password); html += F("'>");
  html += F("<label>Setup AP password</label><input name='ap_password' type='password' maxlength='64' value='"); html += cfg_value(g_netcfg.ap_password); html += F("'>");
  html += F("<small>Used for the fallback setup AP. Minimum 8 characters. If left shorter, the default password is used.</small>");
  html += F("<label><input style='width:auto' type='checkbox' name='use_dhcp' value='1' "); if (g_netcfg.use_dhcp) html += F("checked"); html += F("> Use DHCP</label>");
  html += F("<small>Uncheck DHCP only if you want a static IP for the ESP itself.</small>");
  html += F("<label>Static IP</label><input name='static_ip' maxlength='15' value='"); html += cfg_value(g_netcfg.static_ip); html += F("'>");
  html += F("<label>Gateway</label><input name='static_gw' maxlength='15' value='"); html += cfg_value(g_netcfg.static_gw); html += F("'>");
  html += F("<label>Subnet mask</label><input name='static_mask' maxlength='15' value='"); html += cfg_value(g_netcfg.static_mask); html += F("'>");
  html += F("<label>DNS 1</label><input name='static_dns1' maxlength='15' value='"); html += cfg_value(g_netcfg.static_dns1); html += F("'>");
  html += F("<label>DNS 2</label><input name='static_dns2' maxlength='15' value='"); html += cfg_value(g_netcfg.static_dns2); html += F("'>");
  html += F("<hr style='border-color:#333;margin:18px 0'><label>Web username</label><input name='web_username' maxlength='32' value='"); html += cfg_value(g_netcfg.web_username); html += F("'>");
  html += F("<label>Web password</label><input name='web_password' type='password' maxlength='64' value='"); html += cfg_value(g_netcfg.web_password); html += F("'>");
  html += F("<small>HTTP Basic Auth is active only when both web username and password are set. Status, config, control, API and OTA are then protected in normal WLAN mode and in setup AP mode. If you forget the credentials, reset them via serial CLI with <strong>clearweb</strong>.</small>");
#if MQTT_ENABLE
  html += F("<hr style='border-color:#333;margin:18px 0'><label><input style='width:auto' type='checkbox' name='mqtt_enable' value='1' "); if (g_netcfg.mqtt_enable) html += F("checked"); html += F("> Enable MQTT</label>");
  html += F("<label>MQTT host / IP</label><input name='mqtt_host' maxlength='64' value='"); html += cfg_value(g_netcfg.mqtt_host); html += F("'>");
  html += F("<label>MQTT port</label><input name='mqtt_port' maxlength='5' value='"); html += String(g_netcfg.mqtt_port); html += F("'>");
  html += F("<label>MQTT username</label><input name='mqtt_username' maxlength='32' value='"); html += cfg_value(g_netcfg.mqtt_username); html += F("'>");
  html += F("<label>MQTT password</label><input name='mqtt_password' type='password' maxlength='64' value='"); html += cfg_value(g_netcfg.mqtt_password); html += F("'>");
  html += F("<label><input style='width:auto' type='checkbox' name='mqtt_tls_enable' value='1' "); if (g_netcfg.mqtt_tls_enable) html += F("checked"); html += F("> Enable MQTT TLS</label>");
  html += F("<label><input style='width:auto' type='checkbox' name='mqtt_tls_insecure' value='1' "); if (g_netcfg.mqtt_tls_insecure) html += F("checked"); html += F("> Insecure TLS (test only)</label>");
  html += F("<label>MQTT CA certificate (PEM)</label>");
  html += F("<textarea name='mqtt_ca_cert' rows='12' style='font-family:monospace'>");
  html += html_escape(String(g_netcfg.mqtt_ca_cert));
  html += F("</textarea>");
  html += F("<small>For secure TLS operation, paste the root CA certificate here in PEM format. For testing, insecure TLS can be used.</small>");
#endif
  html += F("<div class='actions'><button type='submit'>Save and reboot</button><a href='/'>Back</a></div></div></form></body></html>");
  return html;
}
static String web_html_control(const String& msg){
  String html;
  html.reserve(7600);
  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>JBC FAE Control</title><style>*{box-sizing:border-box}body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:24px}"
            "input,select,textarea{display:block;width:100%;max-width:100%;min-width:0;padding:10px;border-radius:8px;border:1px solid #444;background:#1d1d1d;color:#eee}"
            "label{display:block;margin:12px 0 6px}.card{background:#1d1d1d;padding:16px;border-radius:12px;margin:12px 0;overflow:hidden}"
            ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px}.nav{display:flex;flex-wrap:wrap;gap:10px;margin:0 0 14px 0}.actions{display:flex;flex-wrap:wrap;gap:10px;margin:16px 0 0 0;padding-bottom:6px}.nav a,.nav button,.actions a,.actions button{display:inline-flex;align-items:center;justify-content:center;margin:0;padding:10px 14px;border-radius:10px;background:#2a6df4;color:#fff;text-decoration:none;border:0;max-width:100%}"
            "small{color:#aaa} h2{margin-top:0}</style></head><body>");
  html += F("<h1>Emulator control</h1><div style='color:#aaa;margin:-8px 0 16px 0'>FW "); html += F(USER_FW_VERSION); html += F(" / by IceCube20</div>");
  if (msg.length()) { html += F("<div class='card'><strong>"); html += html_escape(msg); html += F("</strong></div>"); }
  html += F("<div class='nav'><a href='/'>Status</a><a href='/config'>Network setup</a><a href='/cli'>CLI</a><a href='/update'>Firmware update</a><a href='/api/state'>JSON</a></div>");
  html += F("<form method='post' action='/apply'><div class='grid'>");

  html += F("<div class='card'><h2>Live runtime</h2>");
  html += F("<label>Active buses</label><select name='bus_count'>");
  html += F("<option value='1'");
  if (g_bus_count == 1) html += F(" selected");
  html += F(">1 bus</option>");
  html += F("<option value='2'");
  if (g_bus_count == 2) html += F(" selected");
  html += F(">2 buses</option></select>");
  html += F("<label><input style='width:auto' type='checkbox' name='continuous' value='1' " );
  if (st.continuous) html += F("checked");
  html += F("> Continuous suction</label>");
  html += F("<small>Continuous suction blinks blue on all linked buses. WORK and STAND activity sensors are shown on the status page and published to MQTT/Home Assistant per bus.</small>");
  html += F("<label>Relay contact mode</label><select name='relay_inverted'>");
  html += F("<option value='0'");
  if (!g_relay_inverted) html += F(" selected");
  html += F(">Schließer (NO)</option>");
  html += F("<option value='1'");
  if (g_relay_inverted) html += F(" selected");
  html += F(">Öffner (NC)</option></select>");
  html += F("<label>LED brightness (10..100 %)</label><input name='led_brightness' type='number' min='10' max='100' value='");
  html += String(led_brightness_raw_to_pct(g_led_brightness));
  html += F("'>");
  html += F("<small>Continuous suction blinks blue on all linked buses. Relay mode and LED brightness are saved persistently.</small></div>");

  html += F("<div class='card'><h2>Suction</h2>");
  html += F("<label>Suction level</label><select name='suctionLevel'>");
  html += F("<option value='0'"); if (cfg.suctionLevel == 0) html += F(" selected"); html += F(">High</option>");
  html += F("<option value='1'"); if (cfg.suctionLevel == 1) html += F(" selected"); html += F(">Mid</option>");
  html += F("<option value='2'"); if (cfg.suctionLevel == 2) html += F(" selected"); html += F(">Low</option>");
  html += F("<option value='3'"); if (cfg.suctionLevel == 3) html += F(" selected"); html += F(">Custom</option></select>");
  html += F("<label>Select flow (0..100)</label><input name='selectFlow' type='number' min='0' max='100' value='");
  html += String(selectflow_raw_to_pct(cfg.selectFlow));
  html += F("'>");
  html += F("<small>Select flow is entered as 0..100 and internally stored as 0..1000.</small>");
  html += F("<label><input style='width:auto' type='checkbox' name='stand_intakes' value='1' ");
  if (cfg.stand_intakes) html += F("checked");
  html += F("> Stand intakes enabled</label></div>");

  html += F("<div class='card'><h2>Simulation</h2>");
  html += F("<label>Actual flow (0..100)</label><input name='actual_flow' type='number' min='0' max='100' value='");
  html += String(actualflow_raw_to_pct(st.flow_x1000));
  html += F("'>");
  html += F("<label>Speed RPM (0..65535)</label><input name='speed' type='number' min='0' max='65535' value='");
  html += String(st.speed_rpm);
  html += F("'>");
  html += F("<small>Actual flow is entered as 0..100 and internally stored as 0..1000. Flow and speed are runtime simulation values and are not persisted.</small></div>");

  html += F("<div class='card'><h2>Delay</h2>");
  html += F("<label>Afterrun WORK (s)</label><input name='tstop_work' type='number' min='0' max='300' value='");
  html += String(cfg.tstop_work);
  html += F("'>");
  html += F("<label>Afterrun STAND (s)</label><input name='tstop_stand' type='number' min='0' max='999' value='");
  html += String(cfg.tstop_stand);
  html += F("'></div>");

  html += F("<div class='card'><h2>Filter / error</h2>");
  html += F("<label>Filter life</label><input name='filter_life' type='number' min='0' max='100' value='");
  html += String(filter_raw_to_pct(cfg.filter_life));
  html += F("'>");
  html += F("<label>Filter saturation</label><input name='filter_sat' type='number' min='0' max='100' value='");
  html += String(filter_raw_to_pct(cfg.filter_sat));
  html += F("'>");
  html += F("<small>Filter values are entered as 0..100 %, internally stored as 0..1000.</small>");
  html += F("<label>Error preset</label><select name='error_code'>");
  html += F("<option value='NOERR'");
  if (st.statError == 0) html += F(" selected");
  html += F(">No error</option>");
  html += F("<optgroup label='Warnings'>");
  for (size_t i = 0; i < sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    const ErrInfo& e = kErrMap[i];
    if (!has_warn_error(e.mask)) continue;
    html += F("<option value='");
    html += e.key;
    html += F("'");
    if (st.statError == e.mask) html += F(" selected");
    html += F(">");
    html += e.key;
    html += F(" - ");
    html += html_escape(String((const char*)e.desc));
    html += F("</option>");
  }
  html += F("</optgroup>");
  html += F("<optgroup label='Stops'>");
  for (size_t i = 0; i < sizeof(kErrMap)/sizeof(kErrMap[0]); ++i){
    const ErrInfo& e = kErrMap[i];
    if (!has_stop_error(e.mask)) continue;
    html += F("<option value='");
    html += e.key;
    html += F("'");
    if (st.statError == e.mask) html += F(" selected");
    html += F(">");
    html += e.key;
    html += F(" - ");
    html += html_escape(String((const char*)e.desc));
    html += F("</option>");
  }
  html += F("</optgroup></select>");
  html += F("<small>Current: ");
  html += html_escape(current_error_label());
  html += F(" (level: ");
  html += error_level_name(st.statError);
  html += F(", mask: ");
  html += String(st.statError);
  html += F(" / 0x");
  html += String(st.statError, HEX);
  html += F(")</small></div>");

  html += F("</div><div class='card'><div class='actions'><button type='submit'>Apply values</button><a href='/'>Back</a></div></div></form></body></html>");
  return html;
}
static String web_html_cli(const String& msg, const String& output, const String& command){
  String html;
  html.reserve(8600);
  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>JBC FAE Web CLI</title><style>*{box-sizing:border-box}body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:24px}"
            "input{display:block;width:100%;max-width:100%;min-width:0;padding:10px;border-radius:8px;border:1px solid #444;background:#1d1d1d;color:#eee}"
            "label{display:block;margin:12px 0 6px}.card{background:#1d1d1d;padding:16px;border-radius:12px;margin:12px 0;overflow:hidden}"
            ".nav{display:flex;flex-wrap:wrap;gap:10px;margin:0 0 14px 0}.actions{display:flex;flex-wrap:wrap;gap:10px;margin:16px 0 0 0;padding-bottom:6px}.nav a,.nav button,.actions a,.actions button{display:inline-flex;align-items:center;justify-content:center;margin:0;padding:10px 14px;border-radius:10px;background:#2a6df4;color:#fff;text-decoration:none;border:0;max-width:100%}"
            "small{color:#aaa} pre{margin:0;white-space:pre-wrap;word-break:break-word;font-family:Consolas,Monaco,monospace;background:#0d0d0d;border:1px solid #333;border-radius:10px;padding:12px;min-height:260px;max-height:60vh;overflow:auto}"
            ".muted{color:#aaa}.toolbar{display:flex;gap:10px;flex-wrap:wrap;align-items:center}.toolbar label{margin:0}</style>");
  html += F("<script>var cliPollMs=250;var cliLogPos="); html += String(DBG.recentLogSeq()); html += F(";var cliPending=false;var cliTimer=0;var cliAutoScroll=false;function cliDefaultText(){return 'Ready. Enter a command above, for example: help, show cfg, show net, dbg all off, clearweb';}function cliGetLogEl(){return document.getElementById('cli_log');}function cliHasDefault(el){return !!el&&el.textContent===cliDefaultText();}function cliLoadAutoScroll(){try{var v=localStorage.getItem('cliAutoScroll');if(v!==null)cliAutoScroll=(v==='1');}catch(e){}var cb=document.getElementById('cli_autoscroll');if(cb)cb.checked=cliAutoScroll;}function cliSetAutoScroll(v){cliAutoScroll=!!v;try{localStorage.setItem('cliAutoScroll',cliAutoScroll?'1':'0');}catch(e){}var cb=document.getElementById('cli_autoscroll');if(cb)cb.checked=cliAutoScroll;var el=cliGetLogEl();if(cliAutoScroll&&el)el.scrollTop=el.scrollHeight;}function cliSchedule(){clearTimeout(cliTimer);if(document.hidden)return;cliTimer=setTimeout(function(){updateLog(false);},cliPollMs);}function cliSetLog(el,txt){if(!el)return;el.textContent=(txt&&txt.length)?txt:cliDefaultText();}function cliAppendLog(el,txt){if(!el||!txt||!txt.length)return;if(cliHasDefault(el))el.textContent='';var oldTop=el.scrollTop;var oldHeight=el.scrollHeight;var wasNearBottom=(oldTop+el.clientHeight>=(oldHeight-4));if(el.childNodes.length===1&&el.firstChild&&el.firstChild.nodeType===3&&typeof el.firstChild.appendData==='function'){el.firstChild.appendData(txt);}else{el.appendChild(document.createTextNode(txt));}if(cliAutoScroll||wasNearBottom){el.scrollTop=el.scrollHeight;}else{el.scrollTop=oldTop;}}function updateLog(forceReset){if(cliPending)return;var el=cliGetLogEl();if(!el)return;cliPending=true;var pos=forceReset?0:cliLogPos;var forceReplace=!!forceReset;fetch('/cli/log?pos='+pos+'&_='+Date.now(),{cache:'no-store'}).then(function(r){var next=parseInt(r.headers.get('X-Log-Next')||'0',10);var reset=r.headers.get('X-Log-Reset')==='1';return r.text().then(function(txt){return {txt:txt,next:isNaN(next)?cliLogPos:next,reset:reset};});}).then(function(res){if(res.reset){if(forceReplace||!el.textContent.length||cliHasDefault(el)){cliSetLog(el,res.txt||'');}else{if(res.txt&&res.txt.length){cliAppendLog(el,'\\n[webcli] device log buffer rotated; continuing with newest available lines.\\n');cliAppendLog(el,res.txt);}}}else if(res.txt&&res.txt.length){cliAppendLog(el,res.txt);}cliLogPos=res.next;if(!el.textContent.length)cliSetLog(el,'');}).catch(function(){}).finally(function(){cliPending=false;cliSchedule();});}function clearCliLog(){fetch('/cli/clearlog',{method:'POST',cache:'no-store'}).then(function(){var el=cliGetLogEl();if(el)cliSetLog(el,'');cliLogPos=0;updateLog(true);}).catch(function(){});return false;}function runCliCommand(ev){if(ev)ev.preventDefault();var form=document.getElementById('cli_form');var input=document.getElementById('cli_cmd');var btn=document.getElementById('cli_exec_btn');var msg=document.getElementById('cli_msg');if(!form||!input||!btn)return false;var cmd=(input.value||'').trim();if(!cmd.length){input.focus();return false;}btn.disabled=true;fetch('/cli/run',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:'cmd='+encodeURIComponent(cmd),cache:'no-store'}).then(function(r){return r.json();}).then(function(res){if(msg){msg.textContent=res.message||'Command executed.';var wrap=document.getElementById('cli_msg_wrap');if(wrap)wrap.style.display='block';}var el=cliGetLogEl();if(el&&res.output&&res.output.length)cliAppendLog(el,res.output);if(typeof res.next_seq==='number')cliLogPos=res.next_seq;input.value='';input.focus();}).catch(function(){if(msg){msg.textContent='Command failed.';var wrap=document.getElementById('cli_msg_wrap');if(wrap)wrap.style.display='block';}}).finally(function(){btn.disabled=false;});return false;}window.addEventListener('load',function(){var input=document.getElementById('cli_cmd');cliLoadAutoScroll();if(input){input.addEventListener('keydown',function(ev){if(ev.key==='Enter'&&!ev.shiftKey){runCliCommand(ev);}});}document.addEventListener('visibilitychange',function(){if(!document.hidden)updateLog(false);});updateLog(false);});</script></head><body>");
  html += F("<h1>Web CLI</h1><div style='color:#aaa;margin:-8px 0 16px 0'>FW "); html += F(USER_FW_VERSION); html += F(" / by IceCube20</div>");
  html += F("<div class='nav'><a href='/'>Status</a><a href='/control'>Control</a><a href='/config'>Network setup</a><a href='/update'>Firmware update</a><a href='/api/state'>JSON</a></div>");
  html += F("<div class='card' id='cli_msg_wrap'"); if (!msg.length()) html += F(" style='display:none'"); html += F("><strong id='cli_msg'>"); html += html_escape(msg.length() ? msg : String("Command ready.")); html += F("</strong></div>");
  html += F("<div class='card'><strong>Note</strong><br><small>The Web CLI uses the same commands as the serial console. Critical commands such as <strong>erase</strong>, <strong>save</strong>, <strong>clearweb</strong>, or error presets take effect immediately. The log output is refreshed automatically.</small></div>");
  html += F("<form id='cli_form' method='post' action='/cli/exec' onsubmit='return runCliCommand(event);'><div class='card'>");
  html += F("<label>Command</label><input id='cli_cmd' name='cmd' maxlength='95' autocomplete='off' autofocus value='"); html += cfg_value(command.c_str()); html += F("'>");
  html += F("<div class='actions'><button id='cli_exec_btn' type='submit'>Execute</button><a href='/cli?cmd=help'>Help</a><a href='/'>Back</a></div></div></form>");
  html += F("<div class='card'><div class='toolbar'><strong>Live output</strong><small>Delta refresh every 250 ms</small><label><input id='cli_autoscroll' type='checkbox' onchange='cliSetAutoScroll(this.checked)'> Auto-scroll</label><button type='button' onclick='return clearCliLog();'>Clear log</button></div><pre id='cli_log' data-log-seq='"); html += String(DBG.recentLogSeq()); html += F("'>");
  if (output.length()) html += html_escape(output);
  else html += html_escape(DBG.recentLogString());
  if (!output.length() && !DBG.recentLogString().length()) html += F("Ready. Enter a command above, for example: help, show cfg, show net, dbg all off, clearweb");
  html += F("</pre></div></body></html>");
  return html;
}

static String web_html_update(const String& msg, bool success){
  String html;
  html.reserve(3600);
  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>JBC FAE Firmware update</title><style>*{box-sizing:border-box}body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:24px}"
            "input{display:block;width:100%;max-width:100%;min-width:0;padding:10px;border-radius:8px;border:1px solid #444;background:#1d1d1d;color:#eee}"
            "label{display:block;margin:12px 0 6px}.card{background:#1d1d1d;padding:16px;border-radius:12px;margin:12px 0;overflow:hidden}"
            ".nav{display:flex;flex-wrap:wrap;gap:10px;margin:0 0 14px 0}.actions{display:flex;flex-wrap:wrap;gap:10px;margin:16px 0 0 0;padding-bottom:6px}.nav a,.nav button,.actions a,.actions button{display:inline-flex;align-items:center;justify-content:center;margin:0;padding:10px 14px;border-radius:10px;background:#2a6df4;color:#fff;text-decoration:none;border:0;max-width:100%}"
            "small{color:#aaa}</style>");
  if (success){
    html += F("<script>if(history&&history.replaceState){history.replaceState(null,'','/update');}setTimeout(function(){window.location.replace('/');},5000);</script>");
  }
  html += F("</head><body>");
  html += F("<h1>Firmware update</h1><div style='color:#aaa;margin:-8px 0 16px 0'>FW "); html += F(USER_FW_VERSION); html += F(" / by IceCube20</div>");
  html += F("<div class='nav'><a href='/'>Status</a><a href='/control'>Control</a><a href='/cli'>CLI</a><a href='/config'>Network setup</a></div>");
  if (msg.length()) { html += F("<div class='card'><strong>"); html += html_escape(msg); html += F("</strong></div>"); }
  html += F("<div class='card'><strong>Current firmware:</strong> "); html += F(USER_FW_VERSION); html += F("<br><small>Upload the compiled ESP32 firmware .bin file. The device will reboot automatically after a successful update.</small></div>");
  html += F("<form method='post' action='/update' enctype='multipart/form-data'>");
  html += F("<div class='card'>");
  html += F("<label>Firmware file (.bin)</label>");
  html += F("<input type='file' name='firmware' accept='.bin,application/octet-stream' required>");
  html += F("<div class='actions'><button type='submit'>Upload firmware</button><a href='/'>Back</a></div>");
  html += F("</div></form>");
  if (success){
    html += F("<div class='card'><small>Update finished. Returning to the status page automatically...</small></div>");
  }
  html += F("</body></html>");
  return html;
}
static void web_handle_control(){
  if (!web_require_auth()) return;
  g_http.send(200, "text/html; charset=utf-8", web_html_control());
}
static void web_handle_apply(){
  if (!web_require_auth()) return;
  web_apply_emulator_values(true);
  g_http.send(200, "text/html; charset=utf-8", web_html_control("Values applied without reboot."));
}

static String web_cli_exec_capture(const String& command){
  String cmd = command;
  cmd.trim();
  if (!cmd.length()) return String("[CLI] empty command\n");
  if (cmd.length() >= sizeof(cli_buf)) return String("[CLI] command too long\n");

  char line[sizeof(cli_buf)];
  memset(line, 0, sizeof(line));
  cmd.toCharArray(line, sizeof(line));

  DBG.beginCapture();
  cli_handle_line(line);
  String out = DBG.captureString();
  DBG.endCapture();
  out.trim();
  out += "\n";
  return out;
}

static void web_handle_cli(){
  if (!web_require_auth()) return;
  const String cmd = g_http.hasArg("cmd") ? g_http.arg("cmd") : String("");
  if (cmd.length()) web_cli_exec_capture(cmd);
  g_http.send(200, "text/html; charset=utf-8", web_html_cli(cmd.length() ? String("Command executed.") : String(), DBG.recentLogString(), String()));
}

static void web_handle_cli_exec(){
  if (!web_require_auth()) return;
  const String cmd = g_http.hasArg("cmd") ? g_http.arg("cmd") : String("");
  if (cmd.length()) web_cli_exec_capture(cmd);
  g_http.send(200, "text/html; charset=utf-8", web_html_cli(cmd.length() ? String("Command executed.") : String("No command received."), DBG.recentLogString(), String()));
}

static void web_handle_cli_run(){
  if (!web_require_auth()) return;
  const String cmd = g_http.hasArg("cmd") ? g_http.arg("cmd") : String("");
  const String output = cmd.length() ? web_cli_exec_capture(cmd) : String("");
  const String message = cmd.length() ? String("Command executed.") : String("No command received.");
  String json;
  json.reserve(256 + output.length());
  json += F("{\"ok\":true,\"message\":\"");
  json += json_escape(message);
  json += F("\",\"output\":\"");
  json += json_escape(output);
  json += F("\",\"next_seq\":");
  json += String(DBG.recentLogSeq());
  json += '}';
  g_http.sendHeader("Cache-Control", "no-store");
  g_http.send(200, "application/json; charset=utf-8", json);
}

static void web_handle_cli_log(){
  if (!web_require_auth()) return;
  const uint32_t pos = g_http.hasArg("pos") ? (uint32_t) strtoul(g_http.arg("pos").c_str(), nullptr, 10) : 0U;
  bool reset = false;
  uint32_t next_seq = 0;
  const String chunk = DBG.recentLogChunk(pos, &reset, &next_seq);
  g_http.sendHeader("Cache-Control", "no-store");
  g_http.sendHeader("X-Log-Reset", reset ? "1" : "0");
  g_http.sendHeader("X-Log-Next", String(next_seq));
  g_http.send(200, "text/plain; charset=utf-8", chunk);
}

static void web_handle_cli_clearlog(){
  if (!web_require_auth()) return;
  DBG.clearRecentLog();
  dbg_reset_runtime_filters();
  g_http.send(200, "text/plain; charset=utf-8", "");
}

static void web_handle_update(){
  if (!web_require_auth()) return;
  g_http.send(200, "text/html; charset=utf-8", web_html_update());
}

static void web_handle_update_upload(){
  if (!web_require_auth()) return;
  HTTPUpload& upload = g_http.upload();

  if (upload.status == UPLOAD_FILE_START){
    g_ota_upload_success = false;
    g_ota_upload_active = true;
    strlcpy(g_ota_status_msg, "Starting firmware update...", sizeof(g_ota_status_msg));
#if LED_ENABLE
    leds_tick_ota_upload(true);
#endif
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(DBG);
      strlcpy(g_ota_status_msg, "OTA start failed. Check partition scheme / free OTA space.", sizeof(g_ota_status_msg));
    }
  } else if (upload.status == UPLOAD_FILE_WRITE){
#if LED_ENABLE
    leds_tick_ota_upload(true);
#endif
    if (Update.hasError()) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize){
      Update.printError(DBG);
      strlcpy(g_ota_status_msg, "Firmware write failed.", sizeof(g_ota_status_msg));
    }
  } else if (upload.status == UPLOAD_FILE_END){
    if (!Update.hasError() && Update.end(true)){
      g_ota_upload_success = true;
      strlcpy(g_ota_status_msg, "Firmware uploaded successfully. Device will reboot in 2 seconds.", sizeof(g_ota_status_msg));
      DBG.printf("[OTA] success, %u bytes\n", upload.totalSize);
    } else {
      Update.printError(DBG);
      strlcpy(g_ota_status_msg, "Firmware finalize failed.", sizeof(g_ota_status_msg));
      g_ota_upload_success = false;
    }
#if LED_ENABLE
    leds_tick_ota_upload(true);
#endif
    g_ota_upload_active = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED){
    Update.abort();
    g_ota_upload_success = false;
    g_ota_upload_active = false;
    strlcpy(g_ota_status_msg, "Firmware upload aborted.", sizeof(g_ota_status_msg));
#if LED_ENABLE
    for(uint8_t idx = 0; idx < LED_COUNT_MAX; ++idx) set_led_idx(idx, 0, 0, 0, 0);
    pixels.show();
#endif
    DBG.println(F("[OTA] aborted"));
  }
}

static void web_handle_update_post(){
  if (!web_require_auth()) return;
  const bool ok = g_ota_upload_success && !Update.hasError();
  const String msg = g_ota_status_msg[0] ? String(g_ota_status_msg) : String(ok ? "Firmware uploaded successfully. Device will reboot in 2 seconds." : "Firmware update failed.");
  g_http.send(200, "text/html; charset=utf-8", web_html_update(msg, ok));
  if (ok) g_net_restart_deadline_ms = millis() + 2000UL;
}

static void web_handle_root(){
  if (!web_require_auth()) return;
  if (g_cfg_portal_active) g_http.send(200, "text/html; charset=utf-8", web_html_config());
  else g_http.send(200, "text/html; charset=utf-8", web_html_status());
}
static void web_handle_config(){
  if (!web_require_auth()) return;
  const bool force_scan = g_http.hasArg("scan");
  net_refresh_wifi_scan(force_scan);
  String msg;
  if (force_scan){
    if (g_wifi_scan_running) msg = F("WLAN scan started. Refresh in a moment for updated results.");
    else if (g_wifi_scan_valid) msg = F("WLAN scan updated.");
    else msg = F("WLAN scan requested.");
  }
  g_http.send(200, "text/html; charset=utf-8", web_html_config(msg));
}
static void web_handle_state(){
  if (!web_require_auth()) return;
  String json = net_build_state_json_string();
  g_http.send(200, "application/json", json);
}
static void web_handle_save(){
  if (!web_require_auth()) return;
  auto argOrEmpty = [](const char* name) -> String { return g_http.hasArg(name) ? g_http.arg(name) : String(""); };
  // NetCfg contains the CA certificate buffer; keep the staging copy out of the
  // comparatively small Arduino loop-task stack.
  static NetCfg candidate;
  candidate = g_netcfg;
  copy_cstr(candidate.wifi_hostname, sizeof(candidate.wifi_hostname), argOrEmpty("wifi_hostname"));
  copy_cstr(candidate.wifi_ssid, sizeof(candidate.wifi_ssid), argOrEmpty("wifi_ssid"));
  copy_cstr(candidate.wifi_password, sizeof(candidate.wifi_password), argOrEmpty("wifi_password"));
  copy_cstr(candidate.ap_password, sizeof(candidate.ap_password), argOrEmpty("ap_password"));
  candidate.use_dhcp = g_http.hasArg("use_dhcp") ? 1 : 0;
  copy_cstr(candidate.static_ip, sizeof(candidate.static_ip), argOrEmpty("static_ip"));
  copy_cstr(candidate.static_gw, sizeof(candidate.static_gw), argOrEmpty("static_gw"));
  copy_cstr(candidate.static_mask, sizeof(candidate.static_mask), argOrEmpty("static_mask"));
  copy_cstr(candidate.static_dns1, sizeof(candidate.static_dns1), argOrEmpty("static_dns1"));
  copy_cstr(candidate.static_dns2, sizeof(candidate.static_dns2), argOrEmpty("static_dns2"));
  copy_cstr(candidate.web_username, sizeof(candidate.web_username), argOrEmpty("web_username"));
  copy_cstr(candidate.web_password, sizeof(candidate.web_password), argOrEmpty("web_password"));

  if (!candidate.use_dhcp){
    IPAddress ip, gw, mask, dns1, dns2;
    if (!parse_ip_field(candidate.static_ip, ip)) {
      g_http.send(400, "text/html; charset=utf-8", web_html_config(String("Invalid static IP address.")));
      return;
    }
    if (!parse_ip_field(candidate.static_gw, gw)) {
      g_http.send(400, "text/html; charset=utf-8", web_html_config(String("Invalid gateway address.")));
      return;
    }
    if (!parse_ip_field(candidate.static_mask, mask)) {
      g_http.send(400, "text/html; charset=utf-8", web_html_config(String("Invalid subnet mask.")));
      return;
    }
    if (candidate.static_dns1[0] && !parse_ip_field(candidate.static_dns1, dns1)) {
      g_http.send(400, "text/html; charset=utf-8", web_html_config(String("Invalid DNS server #1 address.")));
      return;
    }
    if (candidate.static_dns2[0] && !parse_ip_field(candidate.static_dns2, dns2)) {
      g_http.send(400, "text/html; charset=utf-8", web_html_config(String("Invalid DNS server #2 address.")));
      return;
    }
  }
#if MQTT_ENABLE
  const bool prev_mqtt_tls_enable = g_netcfg.mqtt_tls_enable ? true : false;
  const uint16_t prev_mqtt_port = g_netcfg.mqtt_port;
  candidate.mqtt_enable = g_http.hasArg("mqtt_enable") ? 1 : 0;
  copy_cstr(candidate.mqtt_host, sizeof(candidate.mqtt_host), argOrEmpty("mqtt_host"));
  const int port = argOrEmpty("mqtt_port").toInt();
  candidate.mqtt_tls_enable = g_http.hasArg("mqtt_tls_enable") ? 1 : 0;
  candidate.mqtt_tls_insecure = g_http.hasArg("mqtt_tls_insecure") ? 1 : 0;
  copy_cstr(candidate.mqtt_ca_cert, sizeof(candidate.mqtt_ca_cert), argOrEmpty("mqtt_ca_cert"));
  if (port > 0 && port <= 65535) candidate.mqtt_port = (uint16_t)port;
  else candidate.mqtt_port = candidate.mqtt_tls_enable ? 8883 : 1883;
  if (prev_mqtt_tls_enable != (candidate.mqtt_tls_enable ? true : false)){
    if (!candidate.mqtt_tls_enable && prev_mqtt_port == 8883 && candidate.mqtt_port == 8883) candidate.mqtt_port = 1883;
    if ( candidate.mqtt_tls_enable && prev_mqtt_port == 1883 && candidate.mqtt_port == 1883) candidate.mqtt_port = 8883;
  }
  copy_cstr(candidate.mqtt_username, sizeof(candidate.mqtt_username), argOrEmpty("mqtt_username"));
  copy_cstr(candidate.mqtt_password, sizeof(candidate.mqtt_password), argOrEmpty("mqtt_password"));
#endif
  if (!candidate.wifi_hostname[0]) {
    strlcpy(candidate.wifi_hostname,
            g_cfg_wifi_hostname_default[0] ? g_cfg_wifi_hostname_default : WIFI_HOSTNAME_PREFIX,
            sizeof(candidate.wifi_hostname));
  }
  if (strlen(candidate.ap_password) < 8) strlcpy(candidate.ap_password, WIFI_CONFIG_AP_PASSWORD, sizeof(candidate.ap_password));
  g_netcfg = candidate;
  netcfg_save();
  g_http.send(200, "text/html; charset=utf-8", web_html_config("Configuration saved. Device will reboot in 2 seconds."));
  g_net_restart_deadline_ms = millis() + 2000UL;
}
static void web_handle_reboot(){
  if (!web_require_auth()) return;
  g_http.send(200, "text/html; charset=utf-8",
              "<!doctype html><html><head><meta charset='utf-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>Rebooting...</title>"
              "<script>"
              "if(history && history.replaceState){history.replaceState(null,'','/');}"
              "setTimeout(function(){window.location.replace('/');}, 3500);"
              "</script>"
              "</head><body style='font-family:Arial;background:#111;color:#eee;margin:24px'>"
              "<h1>Rebooting…</h1>"
              "<p style='color:#aaa'>The page will return to the status view automatically.</p>"
              "<div style='color:#aaa'>FW " USER_FW_VERSION " / by IceCube20</div>"
              "</body></html>");
  g_net_restart_deadline_ms = millis() + 1000UL;
}
#endif
#if MQTT_ENABLE
static bool mqtt_publish_json(const char* topic, const char* payload, bool retain=true){
  const bool ok = g_mqtt.publish(topic, payload, retain);
  service_bus_io_once();
  yield();
  return ok;
}
static void mqtt_publish_availability(bool online){
  if (!g_mqtt.connected()) return;
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/availability", g_mqtt_base_topic);
  g_mqtt.publish(topic, online ? "online" : "offline", true);
}
static void mqtt_publish_discovery_entity(const char* component, const char* object_id, const char* payload){
  char topic[192];
  snprintf(topic, sizeof(topic), "%s/%s/%s/config", MQTT_DISCOVERY_PREFIX, component, object_id);
  g_mqtt.publish(topic, payload, true);
  service_bus_io_once();
  yield();
}
static void mqtt_clear_discovery_entity(const char* component, const char* object_id){
  char topic[192];
  snprintf(topic, sizeof(topic), "%s/%s/%s/config", MQTT_DISCOVERY_PREFIX, component, object_id);
  g_mqtt.publish(topic, "", true);
  service_bus_io_once();
  yield();
}
static void mqtt_publish_discovery(){
  if (!g_mqtt.connected()) return;
  const String cfg_url = WiFi.isConnected() ? (String("http://") + WiFi.localIP().toString() + "/") : portal_url();
  static char state_topic[128], availability_topic[128], cmd_cont_topic[128];
  static char object_id[64];
  static char payload[1400];
  static char cmd_topic[128];
  snprintf(state_topic, sizeof(state_topic), "%s/state", g_mqtt_base_topic);
  snprintf(availability_topic, sizeof(availability_topic), "%s/availability", g_mqtt_base_topic);
  snprintf(cmd_cont_topic, sizeof(cmd_cont_topic), "%s/cmd/continuous", g_mqtt_base_topic);
  struct EntityDef {
    const char* component;
    const char* suffix;
    const char* name;
    const char* value_template;
    const char* extra;
  } defs[] = {
    {"binary_sensor", "relay", "Relay", "{{ 'ON' if value_json.relay else 'OFF' }}", "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:power-socket-eu\""},
    {"sensor", "suction_level", "Suction level", "{{ value_json.suction_level_label }}", "\"icon\":\"mdi:fan-chevron-up\""},
    {"sensor", "select_flow", "Selected flow (%)", "{{ value_json.select_flow }}", "\"unit_of_measurement\":\"%\",\"icon\":\"mdi:fan\""},
    {"sensor", "filter_life", "Filter life (%)", "{{ value_json.filter_life }}", "\"unit_of_measurement\":\"%\",\"entity_category\":\"diagnostic\",\"icon\":\"mdi:air-filter\""},
    {"sensor", "filter_saturation", "Filter saturation (%)", "{{ value_json.filter_saturation }}", "\"unit_of_measurement\":\"%\",\"entity_category\":\"diagnostic\",\"icon\":\"mdi:air-filter\""},
    {"sensor", "afterrun_remaining_s", "Afterrun remaining", "{{ value_json.afterrun_remaining_s }}", "\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-outline\""},
    {"sensor", "cpu_load_pct", "CPU load (%)", "{{ value_json.cpu_load_pct }}", "\"unit_of_measurement\":\"%\",\"entity_category\":\"diagnostic\",\"icon\":\"mdi:chip\""},
    {"sensor", "bus0_link", "Bus 0 link", "{{ value_json.bus0_link }}", "\"icon\":\"mdi:connection\""},
    {"sensor", "bus0_model", "Bus 0 model", "{{ value_json.bus0_station }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:information-outline\""},
    {"sensor", "bus0_device_id", "Bus 0 device ID", "{{ value_json.bus0_device_id }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:identifier\""},
    {"sensor", "bus1_link", "Bus 1 link", "{{ value_json.bus1_link }}", "\"icon\":\"mdi:connection\""},
    {"sensor", "bus1_model", "Bus 1 model", "{{ value_json.bus1_station }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:information-outline\""},
    {"sensor", "bus1_device_id", "Bus 1 device ID", "{{ value_json.bus1_device_id }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:identifier\""},
    {"sensor", "led_jbc1_mode", "LED JBC1 mode", "{{ value_json.led_jbc1_mode }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:led-on\""},
    {"sensor", "led_evt1_mode", "LED EVT1 mode", "{{ value_json.led_evt1_mode }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:led-on\""},
    {"sensor", "led_jbc2_mode", "LED JBC2 mode", "{{ value_json.led_jbc2_mode }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:led-on\""},
    {"sensor", "led_evt2_mode", "LED EVT2 mode", "{{ value_json.led_evt2_mode }}", "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:led-on\""}
  };
  for (const auto& d : defs){
    snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, d.suffix);
    snprintf(
      payload, sizeof(payload),
      "{"
        "\"name\":\"%s\","
        "\"unique_id\":\"%s\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_available\":\"online\","
        "\"payload_not_available\":\"offline\","
        "\"value_template\":\"%s\","
        "%s,"
        "\"device\":{"
          "\"name\":\"%s\","
          "\"identifiers\":[\"%s\"],"
          "\"manufacturer\":\"IceCube20\","
          "\"model\":\"JBC FAE Emulator\","
          "\"sw_version\":\"%s\","
          "\"configuration_url\":\"%s\""
        "}"
      "}",
      d.name, object_id, state_topic, availability_topic, d.value_template, d.extra,
      MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str()
    );
    mqtt_publish_discovery_entity(d.component, object_id, payload);
  }

  snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, "led_jbc1_visual"); mqtt_clear_discovery_entity("light", object_id);
  snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, "led_evt1_visual"); mqtt_clear_discovery_entity("light", object_id);
  snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, "led_jbc2_visual"); mqtt_clear_discovery_entity("light", object_id);
  snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, "led_evt2_visual"); mqtt_clear_discovery_entity("light", object_id);

  {
    snprintf(object_id, sizeof(object_id), "%s_continuous", g_mqtt_device_id);

    // Cleanup legacy discovery topics from older firmware revisions
    // that exposed continuous suction as buttons instead of a switch.
    mqtt_clear_discovery_entity("button", object_id);
    {
      char legacy_id[64];
      snprintf(legacy_id, sizeof(legacy_id), "%s_continuous_on", g_mqtt_device_id);
      mqtt_clear_discovery_entity("button", legacy_id);
      snprintf(legacy_id, sizeof(legacy_id), "%s_continuous_off", g_mqtt_device_id);
      mqtt_clear_discovery_entity("button", legacy_id);
    }

    snprintf(
      payload, sizeof(payload),
      "{"
        "\"name\":\"Continuous suction\","
        "\"unique_id\":\"%s\","
        "\"state_topic\":\"%s\","
        "\"command_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_available\":\"online\","
        "\"payload_not_available\":\"offline\","
        "\"payload_on\":\"ON\","
        "\"payload_off\":\"OFF\","
        "\"state_on\":\"ON\","
        "\"state_off\":\"OFF\","
        "\"value_template\":\"{{ 'ON' if value_json.continuous else 'OFF' }}\","
        "\"icon\":\"mdi:fan\","
        "\"device\":{"
          "\"name\":\"%s\","
          "\"identifiers\":[\"%s\"],"
          "\"manufacturer\":\"IceCube20\","
          "\"model\":\"JBC FAE Emulator\","
          "\"sw_version\":\"%s\","
          "\"configuration_url\":\"%s\""
        "}"
      "}",
      object_id, state_topic, cmd_cont_topic, availability_topic,
      MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str()
    );
    mqtt_publish_discovery_entity("switch", object_id, payload);
  }
  {
    auto publish_switch_entity = [&](const char* suffix, const char* name, const char* cmd_suffix, const char* state_tmpl, const char* state_on, const char* state_off, const char* icon){
      snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, suffix);
      snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/%s", g_mqtt_base_topic, cmd_suffix);
      snprintf(
        payload, sizeof(payload),
        "{"
          "\"name\":\"%s\","
          "\"unique_id\":\"%s\","
          "\"state_topic\":\"%s\","
          "\"command_topic\":\"%s\","
          "\"availability_topic\":\"%s\","
          "\"payload_available\":\"online\","
          "\"payload_not_available\":\"offline\","
          "\"payload_on\":\"ON\","
          "\"payload_off\":\"OFF\","
          "\"state_on\":\"%s\","
          "\"state_off\":\"%s\","
          "\"value_template\":\"%s\","
          "\"icon\":\"%s\","
          "\"device\":{"
            "\"name\":\"%s\","
            "\"identifiers\":[\"%s\"],"
            "\"manufacturer\":\"IceCube20\","
            "\"model\":\"JBC FAE Emulator\","
            "\"sw_version\":\"%s\","
            "\"configuration_url\":\"%s\""
          "}"
        "}",
        name, object_id, state_topic, cmd_topic, availability_topic,
        state_on, state_off, state_tmpl, icon,
        MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str()
      );
      mqtt_publish_discovery_entity("switch", object_id, payload);
    };

    auto publish_number_entity = [&](const char* suffix, const char* name, const char* cmd_suffix, const char* state_tmpl, int minv, int maxv, int step, const char* icon, const char* unit, const char* entity_category){
      snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, suffix);
      snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/%s", g_mqtt_base_topic, cmd_suffix);
      snprintf(
        payload, sizeof(payload),
        "{"
          "\"name\":\"%s\","
          "\"unique_id\":\"%s\","
          "\"state_topic\":\"%s\","
          "\"command_topic\":\"%s\","
          "\"availability_topic\":\"%s\","
          "\"payload_available\":\"online\","
          "\"payload_not_available\":\"offline\","
          "\"value_template\":\"%s\","
          "\"min\":%d,"
          "\"max\":%d,"
          "\"step\":%d%s%s%s,"
          "\"icon\":\"%s\","
          "\"device\":{"
            "\"name\":\"%s\","
            "\"identifiers\":[\"%s\"],"
            "\"manufacturer\":\"IceCube20\","
            "\"model\":\"JBC FAE Emulator\","
            "\"sw_version\":\"%s\","
            "\"configuration_url\":\"%s\""
          "}"
        "}",
        name, object_id, state_topic, cmd_topic, availability_topic,
        state_tmpl, minv, maxv, step,
        unit ? ",\"unit_of_measurement\":\"" : "", unit ? unit : "", unit ? "\"" : "",
        icon,
        MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str()
      );
      if (entity_category && entity_category[0]){
        size_t pos = strlen(payload) - 1;
      }
      mqtt_publish_discovery_entity("number", object_id, payload);
    };

    auto publish_select_entity = [&](const char* suffix, const char* name, const char* cmd_suffix, const char* state_tmpl, const char* options_json, const char* icon){
      snprintf(object_id, sizeof(object_id), "%s_%s", g_mqtt_device_id, suffix);
      snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/%s", g_mqtt_base_topic, cmd_suffix);
      snprintf(
        payload, sizeof(payload),
        "{"
          "\"name\":\"%s\","
          "\"unique_id\":\"%s\","
          "\"state_topic\":\"%s\","
          "\"command_topic\":\"%s\","
          "\"availability_topic\":\"%s\","
          "\"payload_available\":\"online\","
          "\"payload_not_available\":\"offline\","
          "\"value_template\":\"%s\","
          "\"options\":%s,"
          "\"icon\":\"%s\","
          "\"device\":{"
            "\"name\":\"%s\","
            "\"identifiers\":[\"%s\"],"
            "\"manufacturer\":\"IceCube20\","
            "\"model\":\"JBC FAE Emulator\","
            "\"sw_version\":\"%s\","
            "\"configuration_url\":\"%s\""
          "}"
        "}",
        name, object_id, state_topic, cmd_topic, availability_topic,
        state_tmpl, options_json, icon,
        MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str()
      );
      mqtt_publish_discovery_entity("select", object_id, payload);
    };

    snprintf(object_id, sizeof(object_id), "%s_bus0_work_active", g_mqtt_device_id);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Bus 0 Work active\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ 'ON' if value_json.bus0_work_active else 'OFF' }}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:soldering-iron\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("binary_sensor", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_bus0_active", g_mqtt_device_id);
    mqtt_clear_discovery_entity("binary_sensor", object_id);
    snprintf(object_id, sizeof(object_id), "%s_bus0_afterrun", g_mqtt_device_id);
    mqtt_clear_discovery_entity("binary_sensor", object_id);

    snprintf(object_id, sizeof(object_id), "%s_bus0_relay_state", g_mqtt_device_id);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Bus 0 Relay state\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.bus0_relay_state }}\",\"icon\":\"mdi:fan\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("sensor", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_bus0_stand_active", g_mqtt_device_id);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Bus 0 Stand active\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ 'ON' if value_json.bus0_stand_active else 'OFF' }}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:power-standby\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("binary_sensor", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_bus1_work_active", g_mqtt_device_id);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Bus 1 Work active\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ 'ON' if value_json.bus1_work_active else 'OFF' }}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:soldering-iron\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("binary_sensor", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_bus1_active", g_mqtt_device_id);
    mqtt_clear_discovery_entity("binary_sensor", object_id);
    snprintf(object_id, sizeof(object_id), "%s_bus1_afterrun", g_mqtt_device_id);
    mqtt_clear_discovery_entity("binary_sensor", object_id);

    snprintf(object_id, sizeof(object_id), "%s_bus1_relay_state", g_mqtt_device_id);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Bus 1 Relay state\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.bus1_relay_state }}\",\"icon\":\"mdi:fan\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("sensor", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_bus1_stand_active", g_mqtt_device_id);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Bus 1 Stand active\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ 'ON' if value_json.bus1_stand_active else 'OFF' }}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:power-standby\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("binary_sensor", object_id, payload);

    publish_switch_entity("stand_intakes", "Stand intakes", "stand_intakes", "{{ value_json.stand_intakes }}", "1", "0", "mdi:valve");

    // Numbers / Selects
    snprintf(object_id, sizeof(object_id), "%s_cfg_suction_level", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/suction_level", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Suction level set\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.suction_level_label }}\",\"options\":[\"High\",\"Mid\",\"Low\",\"Custom\"],\"icon\":\"mdi:fan-chevron-up\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("select", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_cfg_select_flow", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/select_flow", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Selected flow set (%%)\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.select_flow }}\",\"min\":0,\"max\":100,\"step\":1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:air-filter\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("number", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_cfg_tstop_work", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/tstop_work", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Afterrun work set\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.delay_work_s }}\",\"min\":0,\"max\":300,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-outline\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("number", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_cfg_tstop_stand", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/tstop_stand", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Afterrun stand set\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.delay_stand_s }}\",\"min\":0,\"max\":999,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-outline\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("number", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_cfg_filter_life", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/filter_life", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Filter life set (%%)\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.filter_life }}\",\"min\":0,\"max\":100,\"step\":1,\"unit_of_measurement\":\"%%\",\"entity_category\":\"diagnostic\",\"icon\":\"mdi:air-filter\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("number", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_cfg_filter_saturation", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/filter_sat", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Filter saturation set (%%)\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.filter_saturation }}\",\"min\":0,\"max\":100,\"step\":1,\"unit_of_measurement\":\"%%\",\"entity_category\":\"diagnostic\",\"icon\":\"mdi:air-filter\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("number", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_cfg_speed", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/speed", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Speed RPM set\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.speed_rpm }}\",\"min\":0,\"max\":65535,\"step\":1,\"unit_of_measurement\":\"rpm\",\"icon\":\"mdi:fan\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("number", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_cfg_bus_count", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/bus_count", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Bus count set\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.bus_count | string }}\",\"options\":[\"1\",\"2\"],\"icon\":\"mdi:source-branch\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("select", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_relay_mode", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/relay_mode", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Relay mode\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.relay_mode }}\",\"options\":[\"no\",\"nc\"],\"icon\":\"mdi:electric-switch\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("select", object_id, payload);

    snprintf(object_id, sizeof(object_id), "%s_error_code", g_mqtt_device_id);
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/cmd/error_code", g_mqtt_base_topic);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Error preset\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"value_template\":\"{{ value_json.error_label }}\",\"options\":[\"NOERR - No error\",\"WARN1 - Standzeit Filter endet in X Tagen / Filter lifetime ends in X days\",\"WARN2 - Der Filter ist kurz davor zu Verstopfen / The filter is about to clog\",\"STOP1 - Standzeit Filter ist abgelaufen / Filter lifetime expired\",\"STOP2 - Der Filter ist Verstopft / The filter is clogged\",\"STOP3 - Kein Filter entdeckt! / No filter detected!\",\"STOP4 - Filter Abdeckung offen / Filter cover open\",\"STOP5 - Gebläse beschädigt / Blower damaged\",\"STOP7 - Ventilfehler / Valve error\",\"STOP8 - Überstrom am Hilfsport entdeckt / Overcurrent detected on auxiliary port\",\"STOP9 - Pedalfehler / Pedal error\",\"STOP10 - Systemfehler FAE / FAE system error\",\"STOP11 - Systemfehler FAE / FAE system error\"],\"icon\":\"mdi:alert-octagon\",\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"IceCube20\",\"model\":\"JBC FAE Emulator\",\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
      object_id, state_topic, cmd_topic, availability_topic, MQTT_DEVICE_NAME, g_mqtt_device_id, USER_FW_VERSION, cfg_url.c_str());
    mqtt_publish_discovery_entity("select", object_id, payload);
  }
  {
    char obsolete_topic[192];
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/sensor/%s_stat_error/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/switch/%s_work_active/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/switch/%s_stand_active/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/binary_sensor/%s_work_active/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/binary_sensor/%s_stand_active/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/number/%s_cfg_suction_level/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/number/%s_cfg_pedal_act/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/number/%s_cfg_pedal_mode/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/select/%s_cfg_pedal_act/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/select/%s_cfg_pedal_mode/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/number/%s_cfg_actual_flow/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
    snprintf(obsolete_topic, sizeof(obsolete_topic), "%s/number/%s_cfg_bus_count/config", MQTT_DISCOVERY_PREFIX, g_mqtt_device_id);
    g_mqtt.publish(obsolete_topic, "", true);
  }
  g_mqtt_discovery_sent = true;
}
static void mqtt_callback(char* topic, byte* payload, unsigned int length){
  char msg[256];
  size_t n = (length < sizeof(msg)-1) ? length : (sizeof(msg)-1);
  memcpy(msg, payload, n);
  msg[n] = 0;

  char prefix[128];
  snprintf(prefix, sizeof(prefix), "%s/cmd/", g_mqtt_base_topic);
  const size_t prefix_len = strlen(prefix);
  if (strncmp(topic, prefix, prefix_len) != 0) return;

  const char* suffix = topic + prefix_len;
  if (mqtt_apply_command(suffix, msg)){
#if MQTT_ENABLE
    g_mqtt_force_publish = true;
#endif
  } else if (dbg_show_err) {
    DBG.print(F("[MQTT] invalid command "));
    DBG.print(topic);
    DBG.print(F(" payload="));
    DBG.println(msg);
  }
}
#endif
static void mqtt_configure_transport(){
#if MQTT_ENABLE
  g_mqtt_tls_ready = false;
  g_mqtt.disconnect();
  g_wifi_client.stop();
  g_wifi_client_tls.stop();

  if (!g_netcfg.mqtt_host[0] || g_netcfg.mqtt_port == 0){
    g_mqtt.setServer(g_netcfg.mqtt_host, g_netcfg.mqtt_port);
    return;
  }

  if (g_netcfg.mqtt_tls_enable){
    if (g_netcfg.mqtt_tls_insecure){
      g_wifi_client_tls.setInsecure();
      g_mqtt.setClient(g_wifi_client_tls);
      g_mqtt_tls_ready = true;
    } else if (strlen(g_netcfg.mqtt_ca_cert) > 0){
      g_wifi_client_tls.setCACert(g_netcfg.mqtt_ca_cert);
      g_mqtt.setClient(g_wifi_client_tls);
      g_mqtt_tls_ready = true;
    } else {
      g_mqtt_tls_ready = false;
    }
  } else {
    g_mqtt.setClient(g_wifi_client);
    g_mqtt_tls_ready = true;
  }

  g_mqtt.setServer(g_netcfg.mqtt_host, g_netcfg.mqtt_port);
#endif
}

static void net_start_config_portal(bool keep_sta){
#if WIFI_ENABLE
  net_init_identity();

  if (keep_sta) WiFi.mode(WIFI_AP_STA);
  else          WiFi.mode(WIFI_AP);

  IPAddress ap_ip(192,168,4,1), ap_gw(192,168,4,1), ap_mask(255,255,255,0);
  WiFi.softAPConfig(ap_ip, ap_gw, ap_mask);

  const char* ap_pwd = netcfg_ap_password();
  bool ok;
  if (strlen(ap_pwd) >= 8)
    ok = WiFi.softAP(g_cfg_ap_ssid, ap_pwd, WIFI_CONFIG_AP_CHANNEL);
  else
    ok = WiFi.softAP(g_cfg_ap_ssid, nullptr, WIFI_CONFIG_AP_CHANNEL);

  g_cfg_portal_active = ok;
#if HTTP_ENABLE
  if (g_dns_started) {
    g_dns.stop();
    g_dns_started = false;
  }
  if (ok) {
    g_dns.setTTL(60);
    g_dns.start(53, "*", WiFi.softAPIP());
    g_dns_started = true;
  }
#endif

  DBG.print(F("[AP] ssid="));
  DBG.print(g_cfg_ap_ssid);
  DBG.print(F(" ip="));
  DBG.print(WiFi.softAPIP());
  DBG.print(F(" started="));
  DBG.println(ok ? F("yes") : F("no"));

  net_start_http_if_needed();
#endif
}
static void net_stop_config_portal(){
#if WIFI_ENABLE
  if (!g_cfg_portal_active) return;
#if HTTP_ENABLE
  if (g_dns_started) {
    g_dns.stop();
    g_dns_started = false;
  }
#endif
  WiFi.softAPdisconnect(true);
  g_cfg_portal_active = false;
  WiFi.mode(WIFI_STA);
#endif
}
static void net_start_sta(){
#if WIFI_ENABLE
  if (!netcfg_has_wifi()) return;
  const uint32_t now = millis();
  const bool new_connect_window = (!g_wifi_sta_connecting || g_wifi_connect_started_ms == 0);
  if (new_connect_window) {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_MODE_NULL);
    cooperative_wait_ms(20);
  }
  if (g_netcfg.wifi_hostname[0]) WiFi.setHostname(g_netcfg.wifi_hostname);
  WiFi.mode(g_cfg_portal_active ? WIFI_AP_STA : WIFI_STA);
  if (!net_apply_sta_ip_config() && !g_netcfg.use_dhcp){
    if (dbg_show_err) DBG.println(F("[WIFI] invalid static IP config, falling back to DHCP at runtime"));
    IPAddress zero(0,0,0,0);
    WiFi.config(zero, zero, zero);
  }
  WiFi.begin(g_netcfg.wifi_ssid, g_netcfg.wifi_password);
  g_wifi_sta_connecting = true;
  if (new_connect_window) g_wifi_connect_started_ms = now;
  g_wifi_last_attempt_ms = now;
#endif
}
static void net_setup(){
  net_init_identity();
  netcfg_load();

#if MQTT_ENABLE
  g_mqtt.setCallback(mqtt_callback);
  g_mqtt.setBufferSize(2048);
  g_mqtt.setSocketTimeout(1);
#endif

#if HTTP_ENABLE
  g_http.on("/", HTTP_GET, web_handle_root);
  g_http.on("/api/state", HTTP_GET, web_handle_state);
  g_http.on("/config", HTTP_GET, web_handle_config);
  g_http.on("/control", HTTP_GET, web_handle_control);
  g_http.on("/cli", HTTP_GET, web_handle_cli);
  g_http.on("/cli/exec", HTTP_POST, web_handle_cli_exec);
  g_http.on("/cli/run", HTTP_POST, web_handle_cli_run);
  g_http.on("/cli/log", HTTP_GET, web_handle_cli_log);
  g_http.on("/cli/clearlog", HTTP_POST, web_handle_cli_clearlog);
  g_http.on("/update", HTTP_GET, web_handle_update);
  g_http.on("/save", HTTP_POST, web_handle_save);
  g_http.on("/apply", HTTP_POST, web_handle_apply);
  g_http.on("/update", HTTP_POST, web_handle_update_post, web_handle_update_upload);
  g_http.on("/reboot", HTTP_GET, web_handle_reboot);
  g_http.on("/generate_204", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/gen_204", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/hotspot-detect.html", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/canonical.html", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/redirect", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/success.txt", HTTP_GET, web_handle_captive_success);
  g_http.on("/ncsi.txt", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/connecttest.txt", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/library/test/success.html", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/fwlink", HTTP_GET, web_handle_captive_probe_redirect);
  g_http.on("/wpad.dat", HTTP_GET, web_handle_wpad);
  g_http.onNotFound(web_handle_not_found);
  g_http_started = false;
#endif

#if WIFI_ENABLE
  net_refresh_wifi_scan(true);
  if (netcfg_has_wifi()) {
    net_start_sta();
    net_start_http_if_needed();
  } else {
    net_start_config_portal(false);
  }
#else
  net_start_http_if_needed();
#endif
}

static void net_publish_state(bool force){
#if MQTT_ENABLE
  if (!g_netcfg.mqtt_enable) return;
  if (!g_mqtt.connected()) return;
  const uint32_t now = millis();
  const uint32_t hash = net_state_hash();
  if (!force && !g_mqtt_force_publish && (hash == g_mqtt_last_state_hash) && (now - g_mqtt_last_publish_ms < MQTT_PUBLISH_INTERVAL_MS)) return;
  static char topic[128];
  String json = net_build_state_json_string();
  snprintf(topic, sizeof(topic), "%s/state", g_mqtt_base_topic);
  mqtt_publish_json(topic, json.c_str(), true);
  g_mqtt_last_state_hash = hash;
  g_mqtt_last_publish_ms = now;
  g_mqtt_force_publish = false;
#endif
}
static void net_task(){
#if HTTP_ENABLE
  net_wifi_scan_task();
  if (g_dns_started) {
    service_bus_io_once();
    g_dns.processNextRequest();
  }
  if (g_http_started) {
    service_bus_io_once();
    g_http.handleClient();
  }
#endif
  if (g_net_restart_deadline_ms && (int32_t)(millis() - g_net_restart_deadline_ms) >= 0){
#if MQTT_ENABLE
    if (g_mqtt.connected()) mqtt_publish_availability(false);
#endif
    cooperative_wait_ms(50);
    ESP.restart();
  }
#if WIFI_ENABLE
  const uint32_t now = millis();
  if (WiFi.isConnected()) {
    g_wifi_sta_connecting = false;
    g_wifi_connect_started_ms = 0;
    if (g_cfg_portal_active) net_stop_config_portal();
  } else {
    if (netcfg_has_wifi()) {
      if (!g_wifi_sta_connecting) {
        net_start_sta();
      } else {
        if (!g_cfg_portal_active && (now - g_wifi_connect_started_ms >= WIFI_PORTAL_FALLBACK_MS)) {
          net_start_config_portal(true);
        }
        if (now - g_wifi_last_attempt_ms >= WIFI_RETRY_MS) {
          g_wifi_last_attempt_ms = now;
          WiFi.disconnect();
          net_start_sta();
        }
      }
    } else if (!g_cfg_portal_active) {
      net_start_config_portal(false);
    }
  }
#endif
#if MQTT_ENABLE
  if (!g_netcfg.mqtt_enable) {
    if (g_mqtt.connected()) g_mqtt.disconnect();
    return;
  }
  if (!WiFi.isConnected()) return;
  if (!g_mqtt.connected()){
    const uint32_t now = millis();
    if (!g_netcfg.mqtt_tls_enable && g_netcfg.mqtt_port == 8883){
      if (dbg_show_err && (now - g_mqtt_last_cfg_warn_ms >= 5000UL)){
        g_mqtt_last_cfg_warn_ms = now;
        DBG.println(F("[MQTT] TLS is OFF but port is 8883. Switch broker port to 1883 or re-enable TLS."));
      }
      return;
    }
    if (g_netcfg.mqtt_tls_enable && g_netcfg.mqtt_port == 1883){
      if (dbg_show_err && (now - g_mqtt_last_cfg_warn_ms >= 5000UL)){
        g_mqtt_last_cfg_warn_ms = now;
        DBG.println(F("[MQTT] TLS is ON but port is 1883. Switch broker port to 8883 or disable TLS."));
      }
      return;
    }

    mqtt_configure_transport();
    if (!g_mqtt_tls_ready) return;

    if (now - g_mqtt_last_attempt_ms >= MQTT_RETRY_MS){
      g_mqtt_last_attempt_ms = now;
      char avail_topic[128];
      snprintf(avail_topic, sizeof(avail_topic), "%s/availability", g_mqtt_base_topic);
      const char* client_id = g_mqtt_device_id[0] ? g_mqtt_device_id : MQTT_CLIENT_ID;
      bool ok;
      if (strlen(g_netcfg.mqtt_username) > 0) ok = g_mqtt.connect(client_id, g_netcfg.mqtt_username, g_netcfg.mqtt_password, avail_topic, 0, true, "offline");
      else ok = g_mqtt.connect(client_id, avail_topic, 0, true, "offline");
      if (ok){
        mqtt_publish_availability(true);
        mqtt_publish_discovery();
        char cmd_topic_wildcard[128];
        snprintf(cmd_topic_wildcard, sizeof(cmd_topic_wildcard), "%s/cmd/#", g_mqtt_base_topic);
        g_mqtt.subscribe(cmd_topic_wildcard);
        g_mqtt_force_publish = true;
      }
    }
    return;
  }
  service_bus_io_once();
  g_mqtt.loop();
  service_bus_io_once();
  if (!g_mqtt_discovery_sent) mqtt_publish_discovery();
  net_publish_state(false);
#endif
}
// ===== Setup / Loop ==========================================================
void setup(){
  init_runtime_state();
  Serial.begin(115200);

#ifndef SERIAL_STARTUP_WAIT_MS
#define SERIAL_STARTUP_WAIT_MS 1200
#endif

#if defined(ARDUINO_USB_CDC_ON_BOOT)
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < SERIAL_STARTUP_WAIT_MS) { delay(10); }
#else
  delay(SERIAL_STARTUP_WAIT_MS);
#endif

  persist_begin();
  g_relay_inverted = persist_load_relay_invert();
  deviceid_init_defaults();
  g_bus_count = persist_load_buscount();
  if (g_bus_count < 1 || g_bus_count > MAX_BUSES) g_bus_count = BUS_COUNT;

  for(uint8_t i=0;i<MAX_BUSES;i++){
    (void)persist_load_devid(i);
    uint8_t a=0; if(persist_load_addr(i,a)) g_bus[i].my_addr=a;
  }

  init_cfg_defaults();
  if (persist_load_cfg()) apply_cfg_to_state();
  else                    apply_cfg_to_state();
  g_led_brightness = persist_load_led_brightness();

  print_startup_banner();

  relay_init();
#if LED_ENABLE
  leds_begin();
  for(uint8_t i=0;i<g_bus_count;i++) g_bus[i].t_last_intake_ms = millis();
  for(uint8_t idx=g_bus_count*2; idx<LED_COUNT_MAX; ++idx) set_led_idx(idx,0,0,0,0);
  safe_pixels_show();
#endif

  cpu_load_init();
  net_setup();

  for(uint8_t i=0;i<g_bus_count;i++) JBC_BEGIN(i);
  if (dbg_show_proto) { DBG.println(F("\n[JBC FAE Emulator – Base-Link + P02 (SRC,DST), Dual-Bus]")); for(uint8_t i=0;i<g_bus_count;i++) DBG_PRINTF("[UART bus%u] @ 250000, 8E1\n", i); }
}

void loop(){
  cli_poll();

  service_bus_io_once();

  cpu_load_task();
  net_task();
  service_bus_io_once();

  if(dbg_show_state && !dbg_state_on_write_only && dbg_state_period_ms > 0){
    const uint32_t now = millis();
    if(now - t_last_state >= dbg_state_period_ms){ dump_state(); t_last_state = now; }
  }


  if (st.continuous) {
    if (!g_relay_on) relay_set(true);
    g_afterrun_owner = -1;
    if (g_relay_owner < 0) g_relay_owner = pick_active_bus_owner();
    g_relay_off_deadline = 0;
  } else {
    if (any_work_active()) {
      if (!g_relay_on) relay_set(true);
      if (g_relay_off_deadline) g_relay_off_deadline=0;
      g_afterrun_owner = -1;
      if (g_relay_owner < 0 || !bus_active((uint8_t)g_relay_owner)) g_relay_owner = pick_active_bus_owner();
    } else {
      if (g_relay_off_deadline) {
        if ((int32_t)(millis() - g_relay_off_deadline) >= 0) {
          relay_set(false);
          if (dbg_show_state) { dbg_time_prefix(); relay_log_prefix(); DBG.println(F("afterrun expired")); }
          g_relay_off_deadline=0;
          g_afterrun_owner = -1;
          g_relay_owner = -1;
        }
      } else {
        if (g_relay_on) relay_set(false);
        g_relay_owner = -1;
      }
    }
  }

#if LED_ENABLE
  leds_task();
#endif

#if AUTO_SAVE_CFG_ON_JBC_WRITES
  if ((cfg_dirty || led_brightness_dirty) && (int32_t)(millis() - (int32_t)cfg_save_deadline) >= 0) {
    if (cfg_dirty) persist_save_cfg();
    if (led_brightness_dirty) persist_save_led_brightness(g_led_brightness);
    cfg_dirty = false;
    led_brightness_dirty = false;
    if (dbg_show_state) { dbg_time_prefix(); DBG.println(F("[CFG] auto-saved")); }
  }
#endif

  const uint32_t now=millis();
  for(uint8_t i=0;i<g_bus_count;i++){
    if(now - g_bus[i].t_last_rx > 1000){
      if (dbg_show_proto && g_bus[i].baseState==BASE_P02_ACTIVE) DBG_PRINTF("[LINK b%u] timeout → BASE reset\n", i);
      work_set(i, false);
      memset(g_bus_intakeAct[i], DEF_ACTIVATION, sizeof(g_bus_intakeAct[i]));
      intake_sensor_clear_bus(i);
      mark_state_dirty();
      if (g_afterrun_owner == (int8_t)i) g_afterrun_owner = -1;
      if (g_relay_owner == (int8_t)i) g_relay_owner = pick_active_bus_owner();
      g_bus[i].baseState=BASE_IDLE;
      g_bus[i].linkState=LINK_DOWN;
      g_bus[i].rxState=WAIT_DLE;
      g_bus[i].payLen=0;
      g_bus[i].t_last_rx=now;
      g_bus[i].t_last_rx_us=micros();
      g_bus[i].addr_locked=false;
    }
  }
}
