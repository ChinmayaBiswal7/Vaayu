// ============================================================
//  sensors.cpp — Sensor initialisation and read implementations
// ============================================================
#include "sensors.h"
#include <Wire.h>

// ── Static objects ───────────────────────────────────────────
static Adafruit_AHTX0 aht20;
static DHT            dht11(PIN_DHT11, DHT11);
static OneWire        oneWire(PIN_DS18B20);
static DallasTemperature ds18b20(&oneWire);
static RadarParser    radarParser;

// Last accepted readings for outlier rejection
static float lastGoodRoomTemp     = 25.0f;
static float lastGoodRoomHumidity = 50.0f;
static float lastGoodAcOutlet     = 25.0f;
static float lastGoodBathHumidity = 50.0f;

// Stuck-sensor tracking (10 min check)
static float lastTrackedTemp      = -999.0f;
static unsigned long tempSameSinceMs = 0;

// ── Helpers ──────────────────────────────────────────────────
static bool temp_valid(float v, float lastGood) {
    if (isnan(v) || v < TEMP_VALID_MIN || v > TEMP_VALID_MAX) return false;
    if (lastGood != 0 && fabsf(v - lastGood) > TEMP_OUTLIER_DELTA) return false;
    return true;
}

static bool humid_valid(float v, float lastGood) {
    if (isnan(v) || v < HUMIDITY_VALID_MIN || v > HUMIDITY_VALID_MAX) return false;
    if (lastGood != 0 && fabsf(v - lastGood) > HUMIDITY_OUTLIER_DELTA) return false;
    return true;
}

// ── Initialisation ───────────────────────────────────────────
void sensors_init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // 1. Room AHT20
    if (!aht20.begin()) {
        Serial.println("[sensors] AHT20 not found at 0x38 (check SDA=21, SCL=22)");
    } else {
        Serial.println("[sensors] AHT20 Room Sensor OK");
    }

    // 2. Bathroom DHT11
    dht11.begin();
    Serial.println("[sensors] DHT11 Bathroom Sensor OK (GPIO 4)");

    // 3. DS18B20 AC Outlet Probe
    ds18b20.begin();
    ds18b20.setResolution(11);
    ds18b20.setWaitForConversion(false);
    ds18b20.requestTemperatures();
    Serial.printf("[sensors] DS18B20 count: %d (GPIO 27)\n", ds18b20.getDeviceCount());

    // 4. HLK-LD2410C Radar UART2
    Serial2.begin(RADAR_BAUD, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
    Serial.println("[sensors] HLK-LD2410C Radar UART2 started (RX=16, TX=17)");
}

// ── Room DHT11 Read (Room Temp & Humidity) ───────────────────
void sensors_read_room(SensorData& sd) {
    float h = dht11.readHumidity();
    float t = dht11.readTemperature();
    unsigned long now = millis();

    if (isnan(h) || isnan(t) || h < 1.0f || h > 100.0f) {
        // Read retry
        sd.roomSensorFault = (now - sd.lastRoomReadMs > 30000UL && sd.lastRoomReadMs != 0);
        return;
    }

    if (temp_valid(t, lastGoodRoomTemp)) {
        lastGoodRoomTemp = t;
        sd.roomTemp = t;
        sd.roomTempValid = true;
        sd.roomSensorFault = false;
    }

    if (humid_valid(h, lastGoodRoomHumidity)) {
        lastGoodRoomHumidity = h;
        sd.roomHumidity = h;
        sd.roomHumidValid = true;
    }

    sd.lastRoomReadMs = now;
}

// ── Bathroom Read ────────────────────────────────────────────
void sensors_read_bathroom(SensorData& sd) {
    // Uses room humidity baseline as reference
    sd.bathHumidity = sd.roomHumidity;
    sd.bathTemp = sd.roomTemp;
    sd.bathHumidValid = sd.roomHumidValid;
    sd.bathTempValid = sd.roomTempValid;
}

// ── DS18B20 AC Outlet Read ───────────────────────────────────
void sensors_read_outlet(SensorData& sd) {
    float t = ds18b20.getTempCByIndex(0);
    ds18b20.requestTemperatures(); // Trigger next async read

    if (t == DEVICE_DISCONNECTED_C || t == 85.0f || t < -50.0f) {
        sd.acOutletValid = false;
        return;
    }

    sd.acOutletTemp = t;
    sd.acOutletValid = true;
    sd.lastAcOutletMs = millis();
}

// ── HLK-LD2410C Radar Parser ─────────────────────────────────
// Watches for frame header: 0xF4, 0xF3, 0xF2, 0xF1
void sensors_process_radar_byte(uint8_t b, SensorData& sd) {
    switch (radarParser.state) {
        case RadarParser::WAIT_HEADER_1:
            if (b == 0xF4) radarParser.state = RadarParser::WAIT_HEADER_2;
            break;
        case RadarParser::WAIT_HEADER_2:
            radarParser.state = (b == 0xF3) ? RadarParser::WAIT_HEADER_3 : RadarParser::WAIT_HEADER_1;
            break;
        case RadarParser::WAIT_HEADER_3:
            radarParser.state = (b == 0xF2) ? RadarParser::WAIT_HEADER_4 : RadarParser::WAIT_HEADER_1;
            break;
        case RadarParser::WAIT_HEADER_4:
            if (b == 0xF1) {
                radarParser.state = RadarParser::IN_FRAME;
                radarParser.idx = 0;
            } else {
                radarParser.state = RadarParser::WAIT_HEADER_1;
            }
            break;
        case RadarParser::IN_FRAME:
            radarParser.buf[radarParser.idx++] = b;
            if (radarParser.idx >= 12) {
                // Parse standard report:
                // byte 4: target state (0x00=none, 0x01=moving, 0x02=stationary, 0x03=both)
                // byte 5-6: moving target distance (LE uint16)
                // byte 7-8: stationary target distance (LE uint16)
                uint8_t targetState = radarParser.buf[4];
                uint16_t moveDist   = radarParser.buf[5] | (radarParser.buf[6] << 8);
                uint16_t statDist   = radarParser.buf[7] | (radarParser.buf[8] << 8);

                sd.presenceDetected = (targetState > 0);
                if (targetState == 0x01)      sd.targetDistanceCm = moveDist;
                else if (targetState == 0x02) sd.targetDistanceCm = statDist;
                else if (targetState == 0x03) sd.targetDistanceCm = min(moveDist, statDist);
                else                          sd.targetDistanceCm = 0;

                sd.radarValid = true;
                sd.lastRadarMs = millis();
                radarParser.state = RadarParser::WAIT_HEADER_1;
            }
            break;
    }
}
