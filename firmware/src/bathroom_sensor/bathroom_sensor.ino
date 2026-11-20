// ============================================================
//  bathroom_sensor.ino — Wemos D1 Mini sketch
//
//  Hardware:
//    Wemos D1 Mini (ESP8266) + SHT41 on I2C (SDA=D2, SCL=D1)
//
//  Publishes to MQTT every 30 s:
//    vaayu/bathroom/temp     → float string (°C)
//    vaayu/bathroom/humidity → float string (%RH)
//
//  Dependencies (install via Arduino Library Manager):
//    - Adafruit SHT4x Library
//    - PubSubClient (knolleary)
//    - ESP8266WiFi (bundled with ESP8266 board package)
// ============================================================

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_SHT4x.h>

// ── Configuration — edit before flashing ────────────────────
const char* WIFI_SSID     = "YourSSID";       // ← change
const char* WIFI_PASSWORD = "YourPassword";    // ← change
const char* MQTT_BROKER   = "192.168.1.100";  // ← Vaayu Hub or broker IP
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT   = "vaayu-bathroom";

// MQTT topics
const char* TOPIC_TEMP    = "vaayu/bathroom/temp";
const char* TOPIC_HUMID   = "vaayu/bathroom/humidity";

// Publish interval (ms)
const unsigned long PUB_INTERVAL = 30000;

// ── Globals ──────────────────────────────────────────────────
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
Adafruit_SHT4x sht41;

unsigned long lastPubMs = 0;

// ── Wi-Fi connect ─────────────────────────────────────────────
void wifi_connect() {
    Serial.print("Connecting to Wi-Fi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
}

// ── MQTT reconnect ────────────────────────────────────────────
void mqtt_reconnect() {
    while (!mqtt.connected()) {
        Serial.print("Connecting MQTT...");
        if (mqtt.connect(MQTT_CLIENT)) {
            Serial.println("OK");
        } else {
            Serial.printf("failed (%d) — retry in 5s\n", mqtt.state());
            delay(5000);
        }
    }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Wire.begin();  // D1 Mini: SDA=D2(GPIO4), SCL=D1(GPIO5)

    if (!sht41.begin()) {
        Serial.println("SHT41 not found!");
        // Flash LED to indicate error — keep looping
        while (true) { delay(1000); }
    }
    sht41.setPrecision(SHT4X_HIGH_PRECISION);
    sht41.setHeater(SHT4X_NO_HEATER);
    Serial.println("SHT41 OK");

    wifi_connect();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt_reconnect();
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) wifi_connect();
    if (!mqtt.connected()) mqtt_reconnect();
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastPubMs >= PUB_INTERVAL) {
        lastPubMs = now;

        sensors_event_t hum_ev, temp_ev;
        if (!sht41.getEvent(&hum_ev, &temp_ev)) {
            Serial.println("SHT41 read failed");
            return;
        }

        float temp = temp_ev.temperature;
        float humid = hum_ev.relative_humidity;

        char buf[12];

        dtostrf(temp, 0, 1, buf);
        mqtt.publish(TOPIC_TEMP, buf, true);

        dtostrf(humid, 0, 1, buf);
        mqtt.publish(TOPIC_HUMID, buf, true);

        Serial.printf("Published: temp=%.1f°C  humidity=%.1f%%\n", temp, humid);
    }
}
