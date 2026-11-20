#pragma once
// ============================================================
//  config.h — All tuneable constants for Vaayu Hub
//  Edit this file to adjust behaviour without touching logic.
// ============================================================

// ── Pin assignments (ESP32 DevKit V1 / WROOM-32) ─────────────

// I2C (shared bus: 0.96" SSD1306 OLED + AHT20 sensor)
#define PIN_I2C_SDA        21   // Standard ESP32 I2C SDA
#define PIN_I2C_SCL        22   // Standard ESP32 I2C SCL
#define AHT20_ADDR         0x38 // Default AHT20 I2C address
#define OLED_ADDR          0x3C // Default SSD1306 I2C address

// HLK-LD2410C mmWave radar — UART2 (Serial2)
#define PIN_RADAR_RX       16   // ESP32 RX2 (GPIO16) ← radar TX
#define PIN_RADAR_TX       17   // ESP32 TX2 (GPIO17) → radar RX
#define RADAR_BAUD         256000

// DS18B20 (AC outlet temp probe) — 1-Wire (with 4.7 kΩ pull-up to 3.3V)
#define PIN_DS18B20        4

// PWM — L9110S Motor Driver: 5V Room Fan (Channel A)
#define PIN_FAN_IA         25   // L9110S A-IA (PWM speed control)
#define PIN_FAN_IB         26   // L9110S A-IB (direction / LOW)
#define FAN_PWM_CHANNEL    0
#define FAN_PWM_FREQ       5000 // 5 kHz PWM for 5V DC fan
#define FAN_PWM_BITS       8    // 0-255 duty

// PWM — L9110S Motor Driver: 5V Exhaust Fan (Channel B)
#define PIN_EXHAUST_IA     27   // L9110S B-IA (PWM speed control / ON-OFF)
#define PIN_EXHAUST_IB     14   // L9110S B-IB (direction / LOW)
#define EXHAUST_PWM_CH     1
#define EXHAUST_PWM_FREQ   5000
#define EXHAUST_PWM_BITS   8

// 940 nm IR transmitter LED (via BC547 NPN transistor)
#define PIN_IR_SEND        18

// VS1838B IR receiver module
#define PIN_IR_RECV        19

// Buzzer (active 5V buzzer)
#define PIN_BUZZER         23

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

