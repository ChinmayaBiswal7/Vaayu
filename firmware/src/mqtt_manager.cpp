// ============================================================
//  mqtt_manager.cpp — Wi-Fi + MQTT (PubSubClient)
// ============================================================
#include "mqtt_manager.h"
#include "config.h"
#include "control.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ── Static state ─────────────────────────────────────────────
static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

static SensorData*  g_sd = nullptr;
static ControlState* g_cs = nullptr;

static unsigned long lastReconnectMs = 0;
static const unsigned long RECONNECT_INTERVAL = 5000;

// ── MQTT callback — incoming messages ────────────────────────
static void on_message(char* topic, byte* payload, unsigned int length) {
    char msg[128] = {0};
    if (length >= sizeof(msg)) length = sizeof(msg) - 1;
    memcpy(msg, payload, length);

    Serial.printf("[mqtt] Received [%s]: %s\n", topic, msg);

    if (strcmp(topic, TOPIC_BATH_HUMIDITY) == 0 || strcmp(topic, TOPIC_BATH_TEMP) == 0) {
        // Bathroom sensor publishes JSON: {"temp":28.5,"humidity":72.3}
        // or we receive them as separate float strings
        float val = atof(msg);
        if (strcmp(topic, TOPIC_BATH_HUMIDITY) == 0)
            sensors_update_bathroom(g_sd->bathTemp, val, *g_sd);
        else
            sensors_update_bathroom(val, g_sd->bathHumidity, *g_sd);
        return;
    }

    if (strcmp(topic, TOPIC_CMD_OVERRIDE) == 0) {
        if (strcmp(msg, "MANUAL") == 0 || strcmp(msg, "1") == 0)
            control_notify_ir_override(*g_cs);
        // "AUTO" clears override — handled by ir_override expiry naturally
        return;
    }

    if (strcmp(topic, TOPIC_CMD_FAN_SPEED) == 0) {
        uint8_t pct = (uint8_t)atoi(msg);
        pct = constrain(pct, 0, 100);
        control_set_manual_fan(*g_cs, pct);
        return;
    }

    if (strcmp(topic, TOPIC_CMD_AC_SETPOINT) == 0) {
        int sp = atoi(msg);
        control_set_manual_ac_setpoint(*g_cs, sp);
        return;
    }

    if (strcmp(topic, TOPIC_CMD_AC_STATE) == 0) {
        bool on = (strcmp(msg, "ON") == 0 || strcmp(msg, "1") == 0);
        control_set_manual_ac_state(*g_cs, on);
        return;
    }
}

// ── Wi-Fi connection ─────────────────────────────────────────
static void wifi_connect() {
    if (WiFi.status() == WL_CONNECTED) return;
    Serial.printf("[mqtt] Connecting to Wi-Fi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[mqtt] Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[mqtt] Wi-Fi connect timeout");
    }
}

// ── MQTT reconnect ────────────────────────────────────────────
static bool mqtt_reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        wifi_connect();
        return false;
    }

    Serial.printf("[mqtt] Connecting to broker %s:%d\n", MQTT_BROKER, MQTT_PORT);
    if (mqtt.connect(MQTT_CLIENT_ID)) {
        Serial.println("[mqtt] Connected");
        // Subscribe to incoming topics
        mqtt.subscribe(TOPIC_BATH_TEMP);
        mqtt.subscribe(TOPIC_BATH_HUMIDITY);
        mqtt.subscribe(TOPIC_CMD_OVERRIDE);
        mqtt.subscribe(TOPIC_CMD_FAN_SPEED);
        mqtt.subscribe(TOPIC_CMD_AC_SETPOINT);
        mqtt.subscribe(TOPIC_CMD_AC_STATE);
        return true;
    }
    Serial.printf("[mqtt] Connect failed, state=%d\n", mqtt.state());
    return false;
}

// ── Public API ───────────────────────────────────────────────
void mqtt_init(SensorData* sd, ControlState* cs) {
    g_sd = sd;
    g_cs = cs;
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(on_message);
    mqtt.setBufferSize(512);
    wifi_connect();
    mqtt_reconnect();
}

void mqtt_loop() {
    if (!mqtt.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectMs >= RECONNECT_INTERVAL) {
            lastReconnectMs = now;
            mqtt_reconnect();
        }
    } else {
        mqtt.loop();
    }
}

bool mqtt_connected() {
    return mqtt.connected();
}

// ── Publish helpers ───────────────────────────────────────────
static void pub_float(const char* topic, float val, int decimals = 1) {
    char buf[16];
    dtostrf(val, 0, decimals, buf);
    mqtt.publish(topic, buf, true);  // retained
}
static void pub_str(const char* topic, const char* val) {
    mqtt.publish(topic, val, true);
}
static void pub_int(const char* topic, int val) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", val);
    mqtt.publish(topic, buf, true);
}

void mqtt_publish_all(const SensorData& sd, const ControlState& cs) {
    if (!mqtt.connected()) return;

    pub_float(TOPIC_ROOM_TEMP,     sd.roomTemp);
    pub_float(TOPIC_ROOM_HUMIDITY, sd.roomHumidity, 0);
    pub_str(TOPIC_ROOM_PRESENCE,   sd.presenceDetected ? "1" : "0");
    pub_int(TOPIC_ROOM_DISTANCE,   sd.targetDistanceCm);

    pub_int(TOPIC_FAN_SPEED,       cs.fanDuty * 100 / 255);
    pub_int(TOPIC_AC_SETPOINT,     cs.acSetpoint);
    pub_str(TOPIC_AC_STATE,        cs.acOn ? "ON" : "OFF");
    pub_str(TOPIC_EXHAUST_STATE,   cs.exhaustOn ? "ON" : "OFF");

    pub_str(TOPIC_STATE_REASON,    cs.reason);
    pub_str(TOPIC_ALARM,           cs.alarmActive ? "1" : "0");
}
