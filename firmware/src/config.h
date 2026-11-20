#pragma once
// ============================================================
//  config.h — All tuneable constants for Vaayu Hub
//  Edit this file to adjust behaviour without touching logic.
// ============================================================

// ── Pin assignments ─────────────────────────────────────────

// I2C (shared bus: OLED + SHT41)
#define PIN_I2C_SDA        8    // SDA line
#define PIN_I2C_SCL        9    // SCL line

// SHT41 I2C address (default 0x44)
#define SHT41_ADDR         0x44

// LD2410C radar — UART2
#define PIN_RADAR_TX       17   // ESP32 TX → radar RX
#define PIN_RADAR_RX       18   // ESP32 RX ← radar TX
#define RADAR_BAUD         256000

// DS18B20 (AC outlet temp probe) — 1-Wire
#define PIN_DS18B20        10

// PWM — room fan (via L298N IN1/IN2, enable on PWM pin)
#define PIN_FAN_PWM        11   // L298N ENA
#define PIN_FAN_IN1        12   // L298N IN1 (direction — keep HIGH)
#define PIN_FAN_IN2        13   // L298N IN2 (keep LOW for forward)
#define FAN_PWM_CHANNEL    0
#define FAN_PWM_FREQ       25000  // 25 kHz — above hearing range
#define FAN_PWM_BITS       8      // 0-255 duty

// PWM — exhaust fan (via L298N second channel)
#define PIN_EXHAUST_PWM    14   // L298N ENB
#define PIN_EXHAUST_IN3    15
#define PIN_EXHAUST_IN4    16
#define EXHAUST_PWM_CH     1
#define EXHAUST_PWM_FREQ   25000
#define EXHAUST_PWM_BITS   8

// IR transmitter LED (via NPN transistor)
#define PIN_IR_SEND        4
// IR receiver module (TSOP or similar)
#define PIN_IR_RECV        5

// Buzzer (active, 3.3 V tolerant)
#define PIN_BUZZER         6

// ── Temperature thresholds ───────────────────────────────────

// Below this → fan stays off (comfortable)
#define TEMP_FAN_ON        24.0f   // °C

// Fan speed is proportional between TEMP_FAN_ON and TEMP_FAN_MAX
#define TEMP_FAN_MAX       30.0f   // °C → 100% fan speed

// If temp still ≥ this after DELAY_AC_ON_MS at full fan → turn AC on
#define TEMP_AC_ON         26.0f   // °C

// While AC is on and temp drops to this → start setpoint creep
#define TEMP_AC_CREEP      25.0f   // °C

// AC setpoint limits
#define AC_SETPOINT_MIN    18      // °C (absolute minimum)
#define AC_SETPOINT_DEFAULT 24     // °C (starting setpoint)
#define AC_SETPOINT_MAX    28      // °C (ceiling during creep)

// ── Humidity thresholds ──────────────────────────────────────

// Bathroom humidity above this → exhaust fan ON
#define HUMIDITY_EXHAUST_ON   70.0f  // %RH

// Exhaust fan stays ON until humidity has been below this for DELAY_EXHAUST_OFF_MS
#define HUMIDITY_EXHAUST_OFF  60.0f  // %RH

// Monsoon mode: if room humidity > this → send AC "Dry" mode
#define HUMIDITY_MONSOON      75.0f  // %RH

// ── Timing constants (all in milliseconds) ───────────────────

// How long at full fan speed before we consider turning AC on
#define DELAY_AC_ON_MS         (10UL * 60 * 1000)   // 10 min

// How often setpoint creeps up 1°C while comfortable
#define DELAY_AC_CREEP_MS      (15UL * 60 * 1000)   // 15 min

// No presence → fan off after this
#define DELAY_EMPTY_FAN_OFF_MS  (5UL * 60 * 1000)   // 5 min

// No presence → AC off after this (from last presence event)
#define DELAY_EMPTY_AC_OFF_MS  (10UL * 60 * 1000)   // 10 min

// Exhaust fan stays ON for at least this after humidity drops
#define DELAY_EXHAUST_OFF_MS    (2UL * 60 * 1000)   // 2 min

// Sensor out-of-range → alarm after this
#define DELAY_SENSOR_ALARM_MS  (30UL * 1000)         // 30 s

// After human IR remote press → pause automation for this long
#define DELAY_IR_OVERRIDE_MS   (30UL * 60 * 1000)   // 30 min

// DS18B20 temperature settling after AC command before we check
#define DELAY_AC_CONFIRM_MS    (3UL * 60 * 1000)    // 3 min

// How much the DS18B20 must drop to confirm AC is running
#define AC_CONFIRM_DROP        2.0f   // °C drop from ambient

// ── Sensor validation ranges ─────────────────────────────────

#define TEMP_VALID_MIN    -10.0f   // °C
#define TEMP_VALID_MAX     60.0f   // °C
#define HUMIDITY_VALID_MIN  0.0f   // %RH
#define HUMIDITY_VALID_MAX 100.0f  // %RH

// Outlier rejection: reject reading if delta from last good > this
#define TEMP_OUTLIER_DELTA    5.0f   // °C per reading cycle
#define HUMIDITY_OUTLIER_DELTA 20.0f // %RH per reading cycle

// ── Radar / presence ─────────────────────────────────────────

// Distance at which fan ramps to 100% (closest detected target)
#define RADAR_DISTANCE_NEAR_CM   100   // cm → max fan speed
#define RADAR_DISTANCE_FAR_CM    500   // cm → min fan speed (blended)

// ── MQTT / Wi-Fi ─────────────────────────────────────────────

#define WIFI_SSID       "YourSSID"          // ← change
#define WIFI_PASSWORD   "YourPassword"       // ← change
#define MQTT_BROKER     "192.168.29.208"     // User's laptop IP
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "vaayu-hub"

// Topics — published by hub
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

// Topics — published by bathroom node
#define TOPIC_BATH_HUMIDITY  "vaayu/bathroom/humidity"
#define TOPIC_BATH_TEMP      "vaayu/bathroom/temp"

// Topics — commands from dashboard
#define TOPIC_CMD_OVERRIDE   "vaayu/cmd/override"
#define TOPIC_CMD_FAN_SPEED  "vaayu/cmd/fan_speed"
#define TOPIC_CMD_AC_SETPOINT "vaayu/cmd/ac_setpoint"
#define TOPIC_CMD_AC_STATE   "vaayu/cmd/ac_state"

// ── FreeRTOS task periods ────────────────────────────────────

#define TASK_SENSORS_MS   5000    // sensor read interval
#define TASK_CONTROL_MS   10000   // control loop interval (10 s; counts internal timers)
#define TASK_DISPLAY_MS   1000    // OLED refresh

// ── Misc ─────────────────────────────────────────────────────

#define BUZZER_FREQ_HZ   2000   // Hz for tone() equivalent
#define OLED_ADDR        0x3C   // SSD1306 I2C address
#define OLED_WIDTH       128
#define OLED_HEIGHT       64

// ── Simulation / Testing ─────────────────────────────────────
// Uncomment to inject fake sensor data for Wokwi / PC testing
// #define SIMULATE_SENSORS

