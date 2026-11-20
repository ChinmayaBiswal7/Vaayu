#pragma once
// ============================================================
//  config.h — All tuneable constants & pinouts for Vaayu Hub
//  Strictly conforms to VAAYU_CONTEXT.md (AtomQuest 2026 - Round 3)
// ============================================================

// ── Pin assignments (ESP32 DevKit V1 / WROOM-32) ─────────────
// Section 5 of VAAYU_CONTEXT.md

// OLED + AHT20 (shared I2C bus at 3.3V)
#define PIN_I2C_SDA        21   // Standard ESP32 I2C SDA
#define PIN_I2C_SCL        22   // Standard ESP32 I2C SCL
#define AHT20_ADDR         0x38 // Default AHT20 I2C address
#define OLED_ADDR          0x3C // Default SSD1306 0.96" OLED address
#define OLED_WIDTH         128
#define OLED_HEIGHT        64

// Bathroom DHT11 (wired to hub on a 2–3 m wire)
#define PIN_DHT11          4    // 10 kΩ pull-up to 3.3V

// DS18B20 (taped at AC air outlet) — 1-Wire
#define PIN_DS18B20        27   // 4.7 kΩ pull-up to 3.3V

// HLK-LD2410C mmWave radar — UART2 (Serial2)
#define PIN_RADAR_RX       16   // ESP32 RX2 (GPIO16) ← radar TX
#define PIN_RADAR_TX       17   // ESP32 TX2 (GPIO17) → radar RX
#define RADAR_BAUD         256000

// 940 nm IR transmitter LED (via BC547 NPN transistor)
#define PIN_IR_SEND        25   // GPIO 25 → 1 kΩ → BC547 base

// VS1838B IR receiver (38 kHz)
#define PIN_IR_RECV        26   // 3.3V supply

// Main room fan — L9110S dual motor driver (A-IA PWM, A-IB tied to GND)
#define PIN_FAN_PWM        32   // L9110S A-IA (PWM speed)
#define FAN_PWM_CHANNEL    0
#define FAN_PWM_FREQ       22000 // 22 kHz — inaudible PWM
#define FAN_PWM_BITS       8    // 0-255 duty

// Exhaust fan — L9110S dual motor driver (B-IA PWM, B-IB tied to GND)
#define PIN_EXHAUST_PWM    14   // L9110S B-IA (PWM speed)
#define EXHAUST_PWM_CH     1
#define EXHAUST_PWM_FREQ   22000
#define EXHAUST_PWM_BITS   8

// Buzzer (audible fail-safe alarm)
#define PIN_BUZZER         19
#define BUZZER_FREQ_HZ     2000

// Optional PIR backup (if radar is unavailable)
#define PIN_PIR_BACKUP     34   // Input-only pin

// ── Fan Level & Airspeed Definitions ─────────────────────────
// Section 6 & 7: 6 discrete fan levels (0 to 5)
#define FAN_LEVELS_COUNT   6

// Air velocity estimates at ~1.5 m (m/s) for levels 0–5
static const float FAN_VELOCITIES[FAN_LEVELS_COUNT] = { 0.0f, 0.30f, 0.55f, 0.80f, 1.05f, 1.35f };

// PWM Duty cycle values (0–255) for levels 0–5: 0%, 35%, 50%, 65%, 80%, 100%
static const uint8_t FAN_DUTIES[FAN_LEVELS_COUNT]   = { 0, 89, 128, 166, 204, 255 };

// ── Comfort Index & Profiles ─────────────────────────────────
// Formula: CI = T + 0.05 * (RH - 50.0) - credit(v)
// credit(v) = 2.5 * min(v / 0.8, 1.2)
enum ComfortProfile {
    PROFILE_NORMAL = 0, // Default: Target 25.5°C, Max fan 5, AC 26–28°C, Monsoon OFF
    PROFILE_SLEEP  = 1, // Sleep:   Target 25.5°C, Max fan 3, AC 26–28°C, Monsoon OFF
    PROFILE_HUMID  = 2, // Humid:   Target 25.5°C, Max fan 5, AC 26–28°C, Monsoon ON
    PROFILE_ECO    = 3  // Eco:     Target 26.0°C, Max fan 5, AC 26–29°C, Monsoon OFF
};

struct ProfileConfig {
    float   targetCI;
    uint8_t maxFanLevel;
    int     acStartTemp;
    int     acCapTemp;
    bool    monsoonMode;
};

static const ProfileConfig PROFILES[4] = {
    { 25.5f, 5, 26, 28, false }, // PROFILE_NORMAL
    { 25.5f, 3, 26, 28, false }, // PROFILE_SLEEP
    { 25.5f, 5, 26, 28, true  }, // PROFILE_HUMID
    { 26.0f, 5, 26, 29, false }  // PROFILE_ECO
};

// ── Control Timings & Thresholds ─────────────────────────────
// Normal: 1 control tick = 1 minute (60,000 ms).
// Fast-test mode: 1 control tick = 5 seconds (5,000 ms).
#define NORMAL_TICK_MS         60000UL
#define FAST_TEST_TICK_MS       5000UL

// Occupancy counters (in minutes)
#define OCCUPANCY_EMPTY_LOG_MIN     3   // "room confirmed empty"
#define OCCUPANCY_FAN_OFF_MIN       5   // save fan level, fan off
#define OCCUPANCY_AC_OFF_MIN       10   // save AC state, AC off

// AC Compressor & Command Rules
#define AC_COMMAND_RATE_LIMIT_MIN  10   // at most one command every 10 min
#define AC_RESTART_LOCKOUT_MIN      3   // at least 3 min between OFF and ON
#define AC_FULL_FAN_TRIGGER_MIN    10   // 10 min at full fan & CI > target+0.5 -> AC ON
#define AC_SETPOINT_CREEP_MIN      15   // 15 min at CI <= target+0.5 -> setpoint +1°C
#define AC_GIVE_BACK_MIN           10   // 10 min at full fan & CI > target+1.0 -> setpoint -1°C

// DS18B20 AC Confirmation Check (The Failsafe)
#define AC_CONFIRM_WAIT_MIN         3   // wait 3 min after ON/OFF before reading DS18B20
#define AC_CONFIRM_COOL_DROP        3.0f // outlet <= roomTemp - 3.0°C
#define AC_CONFIRM_DRY_DROP         2.0f // outlet <= roomTemp - 2.0°C in Dry mode
#define AC_CONFIRM_OFF_DELTA        2.0f // outlet within 2.0°C of room temp

// Bathroom Exhaust Humidity Rules (DHT11)
#define EXHAUST_EMA_ALPHA           0.033f // 30-min time constant EMA (1/30)
#define EXHAUST_TRIGGER_SPIKE       8.0f   // RH rises >= 8% above baseline
#define EXHAUST_TRIGGER_HIGH_RH     75.0f  // RH > 75% while >= 3% above baseline
#define EXHAUST_TRIGGER_HIGH_DELTA  3.0f
#define EXHAUST_OFF_DELTA           3.0f   // within 3% of baseline
#define EXHAUST_OFF_STABLE_MIN      2      // for 2 consecutive minutes
#define EXHAUST_MAX_RUNTIME_MIN    45      // hard limit: shut off after 45 min
#define EXHAUST_SENSOR_TIMEOUT_MIN 10      // if sensor dead for 10 min -> manual mode

// Manual Override
#define MANUAL_OVERRIDE_PAUSE_MIN  30      // pause automation for 30 minutes
#define MANUAL_OVERRIDE_SHIFT_CI    0.15f  // shift comfort target by 0.15°C

// Sensor Outlier & Range Validation
#define TEMP_VALID_MIN             -10.0f
#define TEMP_VALID_MAX              60.0f
#define HUMIDITY_VALID_MIN           0.0f
#define HUMIDITY_VALID_MAX         100.0f
#define TEMP_OUTLIER_DELTA           5.0f  // max jump per cycle
#define HUMIDITY_OUTLIER_DELTA      20.0f  // max jump per cycle
#define SENSOR_STUCK_TIMEOUT_MIN    10     // exact same value for > 10 min -> fault

// ── Networking & Wi-Fi Access Point ──────────────────────────
// Section 7: Standalone SoftAP (no external router needed)
#define SOFTAP_SSID        "Vaayu-Hub"
#define SOFTAP_PASSWORD    "vaayuhub123"
#define HTTP_PORT          80

// Optional Station Mode / MQTT
#define WIFI_STA_SSID      "YourSSID"
#define WIFI_STA_PASSWORD  "YourPassword"
#define MQTT_BROKER        "192.168.29.208"
#define MQTT_PORT          1883
#define MQTT_CLIENT_ID     "vaayu-hub"

// Topics
#define TOPIC_ROOM_TEMP      "vaayu/room/temp"
#define TOPIC_ROOM_HUMIDITY  "vaayu/room/humidity"
#define TOPIC_ROOM_PRESENCE  "vaayu/room/presence"
#define TOPIC_ROOM_DISTANCE  "vaayu/room/distance"
#define TOPIC_FAN_SPEED      "vaayu/fan/speed"
#define TOPIC_AC_SETPOINT    "vaayu/ac/setpoint"
#define TOPIC_AC_STATE       "vaayu/ac/state"
#define TOPIC_EXHAUST_STATE  "vaayu/exhaust/state"
#define TOPIC_STATE_REASON   "vaayu/state/reason"
#define TOPIC_ALARM          "vaayu/alarm"
#define TOPIC_BATH_HUMIDITY  "vaayu/bathroom/humidity"
#define TOPIC_BATH_TEMP      "vaayu/bathroom/temp"

// FreeRTOS Task intervals
#define TASK_SENSORS_MS    2000  // read AHT20 & DHT11 every 2s
#define TASK_DISPLAY_MS     500  // OLED refresh every 500ms
