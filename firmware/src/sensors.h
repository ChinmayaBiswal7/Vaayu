#pragma once
// ============================================================
//  sensors.h — Sensor data structures and read interfaces
// ============================================================
#include <Arduino.h>
#include <Adafruit_SHT4x.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include "config.h"

// ── SensorData ───────────────────────────────────────────────
struct SensorData {
    // Room SHT41
    float roomTemp      = 25.0f;   // °C
    float roomHumidity  = 50.0f;   // %RH
    bool  roomTempValid    = false;
    bool  roomHumidValid   = false;

    // Bathroom SHT41 (received via MQTT)
    float bathTemp      = 25.0f;
    float bathHumidity  = 50.0f;
    bool  bathTempValid    = false;
    bool  bathHumidValid   = false;

    // DS18B20 at AC air outlet
    float acOutletTemp  = 25.0f;
    bool  acOutletValid = false;

    // LD2410C radar
    bool  presenceDetected  = false;
    uint16_t targetDistanceCm = 300;  // last detected distance
    bool  radarValid        = false;

    // Timestamps of last valid read (millis)
    unsigned long lastRoomTempMs    = 0;
    unsigned long lastRoomHumidMs   = 0;
    unsigned long lastBathHumidMs   = 0;
    unsigned long lastAcOutletMs    = 0;
    unsigned long lastRadarMs       = 0;
};

// ── LD2410C frame parser state ───────────────────────────────
struct RadarParser {
    enum State { WAIT_AA, WAIT_FF1, WAIT_FF2, WAIT_FF3, IN_FRAME };
    State state = WAIT_AA;
    uint8_t buf[32];
    uint8_t idx = 0;
};

// ── Public API ───────────────────────────────────────────────

void sensors_init();

// Call once per TASK_SENSORS_MS cycle
void sensors_read_room(SensorData& sd);
void sensors_read_outlet(SensorData& sd);

// Call continuously from TaskIR / TaskSensors — feeds serial bytes
void sensors_process_radar_byte(uint8_t b, SensorData& sd);

// Update bathroom values from MQTT callback
void sensors_update_bathroom(float temp, float humidity, SensorData& sd);
