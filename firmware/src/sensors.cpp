// ============================================================
//  sensors.cpp — Sensor initialisation and read implementations
// ============================================================
#include "sensors.h"
#include <Wire.h>

// ── Static objects ───────────────────────────────────────────
static Adafruit_SHT4x sht41;
static OneWire        oneWire(PIN_DS18B20);
static DallasTemperature ds18b20(&oneWire);
static RadarParser    radarParser;

// Last accepted (good) readings for outlier rejection
static float lastGoodRoomTemp     = 25.0f;
static float lastGoodRoomHumidity = 50.0f;
static float lastGoodAcOutlet     = 25.0f;
static float lastGoodBathTemp     = 25.0f;
static float lastGoodBathHumidity = 50.0f;

// ── Helper: range + outlier check ───────────────────────────
static bool temp_valid(float v, float lastGood) {
    if (v < TEMP_VALID_MIN || v > TEMP_VALID_MAX)        return false;
    if (lastGood != 0 && fabsf(v - lastGood) > TEMP_OUTLIER_DELTA) return false;
    return true;
}
static bool humid_valid(float v, float lastGood) {
    if (v < HUMIDITY_VALID_MIN || v > HUMIDITY_VALID_MAX) return false;
    if (lastGood != 0 && fabsf(v - lastGood) > HUMIDITY_OUTLIER_DELTA) return false;
    return true;
}

// ── Initialisation ───────────────────────────────────────────
void sensors_init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // SHT41
    if (!sht41.begin()) {
        Serial.println("[sensors] SHT41 not found — check wiring");
    } else {
        sht41.setPrecision(SHT4X_HIGH_PRECISION);
        sht41.setHeater(SHT4X_NO_HEATER);
        Serial.println("[sensors] SHT41 OK");
    }

    // DS18B20
    ds18b20.begin();
    ds18b20.setResolution(12);
    ds18b20.setWaitForConversion(false);  // non-blocking
    ds18b20.requestTemperatures();
    Serial.printf("[sensors] DS18B20: %d device(s)\n", ds18b20.getDeviceCount());

    // Radar UART
    Serial2.begin(RADAR_BAUD, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
    Serial.println("[sensors] Radar UART2 started");
}

// ── Room SHT41 ───────────────────────────────────────────────
void sensors_read_room(SensorData& sd) {
    sensors_event_t hum_ev, temp_ev;
    if (!sht41.getEvent(&hum_ev, &temp_ev)) {
        Serial.println("[sensors] SHT41 read failed");
        return;
    }

    float t = temp_ev.temperature;
    float h = hum_ev.relative_humidity;

    if (temp_valid(t, lastGoodRoomTemp)) {
        lastGoodRoomTemp  = t;
        sd.roomTemp       = t;
        sd.roomTempValid  = true;
        sd.lastRoomTempMs = millis();
    } else {
        Serial.printf("[sensors] Room temp rejected: %.1f°C\n", t);
        // Keep last good value — flag still reflects prior success
    }

    if (humid_valid(h, lastGoodRoomHumidity)) {
        lastGoodRoomHumidity = h;
        sd.roomHumidity      = h;
        sd.roomHumidValid    = true;
        sd.lastRoomHumidMs   = millis();
    } else {
        Serial.printf("[sensors] Room humidity rejected: %.1f%%\n", h);
    }
}

// ── DS18B20 AC outlet ────────────────────────────────────────
void sensors_read_outlet(SensorData& sd) {
    // Collect previous async request
    float t = ds18b20.getTempCByIndex(0);

    // Kick off next conversion for next cycle
    ds18b20.requestTemperatures();

    if (t == DEVICE_DISCONNECTED_C || t == 85.0f) {
        Serial.println("[sensors] DS18B20 disconnected or power-on reset");
        sd.acOutletValid = false;
        return;
    }
    if (temp_valid(t, lastGoodAcOutlet)) {
        lastGoodAcOutlet    = t;
        sd.acOutletTemp     = t;
        sd.acOutletValid    = true;
        sd.lastAcOutletMs   = millis();
    } else {
        Serial.printf("[sensors] DS18B20 outlier rejected: %.1f°C\n", t);
    }
}

// ── LD2410C UART frame parser ────────────────────────────────
//
// Frame format (basic energy report):
//   Header:  0xF4 0xF3 0xF2 0xF1
//   Length:  2 bytes LE
//   Data:    variable
//   Footer:  0xF8 0xF7 0xF6 0xF5
//
// We watch for the simpler "basic" data frame:
//   0xAA  <data_type>  <head_flag>  <target_state>
//         <move_dist_H> <move_dist_L>  <move_energy>
//         <still_dist_H> <still_dist_L> <still_energy>
//         <detect_dist_H> <detect_dist_L>
//   0x55  0x00
//
// target_state: 0=no target, 1=moving, 2=stationary, 3=both
//
void sensors_process_radar_byte(uint8_t b, SensorData& sd) {
    RadarParser& p = radarParser;

    switch (p.state) {
    case RadarParser::WAIT_AA:
        if (b == 0xAA) { p.idx = 0; p.buf[p.idx++] = b; p.state = RadarParser::IN_FRAME; }
        break;

    case RadarParser::IN_FRAME:
        if (p.idx < sizeof(p.buf)) p.buf[p.idx++] = b;

        // Check for end-of-frame: 0x55 0x00 at bytes [13] [14]
        if (p.idx >= 15 && p.buf[p.idx - 2] == 0x55 && p.buf[p.idx - 1] == 0x00) {
            // Parse: buf[0]=0xAA, buf[1]=data_type, buf[2]=head, buf[3]=target_state
            uint8_t targetState = p.buf[3];
            uint16_t moveDist   = ((uint16_t)p.buf[4] << 8) | p.buf[5];
            uint16_t stillDist  = ((uint16_t)p.buf[7] << 8) | p.buf[8];
            uint16_t detDist    = ((uint16_t)p.buf[10] << 8) | p.buf[11];

            sd.presenceDetected = (targetState != 0);
            if (sd.presenceDetected) {
                // Use closest of moving / stationary distances
                uint16_t dist = moveDist;
                if (targetState == 2 || (targetState == 3 && stillDist < dist)) dist = stillDist;
                if (dist == 0) dist = detDist;
                sd.targetDistanceCm = dist;
            }
            sd.radarValid   = true;
            sd.lastRadarMs  = millis();
            p.state = RadarParser::WAIT_AA;
            p.idx   = 0;
        }

        // Safety reset if buffer fills without end marker
        if (p.idx >= sizeof(p.buf)) {
            p.state = RadarParser::WAIT_AA;
            p.idx   = 0;
        }
        break;

    default:
        p.state = RadarParser::WAIT_AA;
        break;
    }
}

// ── Bathroom MQTT update ─────────────────────────────────────
void sensors_update_bathroom(float temp, float humidity, SensorData& sd) {
    if (temp_valid(temp, lastGoodBathTemp)) {
        lastGoodBathTemp  = temp;
        sd.bathTemp       = temp;
        sd.bathTempValid  = true;
    }
    if (humid_valid(humidity, lastGoodBathHumidity)) {
        lastGoodBathHumidity = humidity;
        sd.bathHumidity      = humidity;
        sd.bathHumidValid    = true;
        sd.lastBathHumidMs   = millis();
    }
}
