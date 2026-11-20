// ============================================================
//  bathroom_sensor.ino — Wemos D1 Mini sketch
//
//  Hardware:
//    Wemos D1 Mini (ESP8266) + DHT11 sensor on Pin D4 (GPIO 2)
//
//  Publishes to MQTT every 30 s:
//    vaayu/bathroom/temp     → float string (°C)
//    vaayu/bathroom/humidity → float string (%RH)
//
//  Dependencies (install via Arduino Library Manager):
//    - DHT sensor library (Adafruit)
//    - Adafruit Unified Sensor
//    - PubSubClient (knolleary)
//    - ESP8266WiFi (bundled with ESP8266 board package)
// ============================================================

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ── Pin definitions ──────────────────────────────────────────
#define DHTPIN        D4       // GPIO 2 on Wemos D1 Mini
#define DHTTYPE       DHT11    // DHT 11

// ── Configuration — edit before flashing ────────────────────
const char* WIFI_SSID     = "YourSSID";       // ← change
const char* WIFI_PASSWORD = "YourPassword";    // ← change
const char* MQTT_BROKER   = "192.168.29.208"; // ← Vaayu Hub or broker IP
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
DHT          dht(DHTPIN, DHTTYPE);

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
            Serial.printf("failed (rc=%d), retry in 5s\n", mqtt.state());
            delay(5000);
        }
    }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[Vaayu Bathroom Node] Booting with DHT11...");

    dht.begin();
    wifi_connect();

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    if (!mqtt.connected()) mqtt_reconnect();
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastPubMs >= PUB_INTERVAL || lastPubMs == 0) {
        lastPubMs = now;

        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (isnan(h) || isnan(t)) {
            Serial.println("[dht11] Failed to read from DHT11 sensor!");
            return;
        }

        Serial.printf("[dht11] Bathroom Temp: %.1f°C  Humidity: %.1f%%\n", t, h);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", t);
        mqtt.publish(TOPIC_TEMP, buf, true);

        snprintf(buf, sizeof(buf), "%.1f", h);
        mqtt.publish(TOPIC_HUMID, buf, true);
    }
}
