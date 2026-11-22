#pragma once
// ============================================================
//  sensors.h — Sensor data structures and read interfaces
// ============================================================
#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <DHT.h>
#include "config.h"

// ── SensorData ───────────────────────────────────────────────
struct SensorData {
    // Room DHT11 sensor (GPIO 4)
    float roomTemp          = 25.0f;   // °C
    float roomHumidity      = 50.0f;   // %RH
    bool  roomTempValid     = false;
    bool  roomHumidValid    = false;
    bool  roomSensorFault   = false;

    // Bathroom / Virtual Humidity
    float bathTemp          = 25.0f;
    float bathHumidity      = 50.0f;
    bool  bathTempValid     = true;
    bool  bathHumidValid    = true;
    bool  bathSensorFault   = false;

    // DS18B20 at AC air outlet (GPIO 27)
    float acOutletTemp      = 25.0f;
    bool  acOutletValid     = false;

    // HLK-LD2410 radar (GPIO 16 / 17)
    bool  presenceDetected  = false;
    uint16_t targetDistanceCm = 150;
    bool  radarValid        = false;

    // Timestamps of last valid read (millis)
    unsigned long lastRoomReadMs    = 0;
    unsigned long lastBathReadMs    = 0;
    unsigned long lastAcOutletMs    = 0;
    unsigned long lastRadarMs       = 0;
};

// ── LD2410C frame parser state ───────────────────────────────
struct RadarParser {
    enum State { WAIT_HEADER_1, WAIT_HEADER_2, WAIT_HEADER_3, WAIT_HEADER_4, IN_FRAME };
    State state = WAIT_HEADER_1;
    uint8_t buf[32];
    uint8_t idx = 0;
};

// ── Public API ───────────────────────────────────────────────
void sensors_init();
void sensors_read_room(SensorData& sd);
void sensors_read_bathroom(SensorData& sd);
void sensors_read_outlet(SensorData& sd);
void sensors_process_radar_byte(uint8_t b, SensorData& sd);
