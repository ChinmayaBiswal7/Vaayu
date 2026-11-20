#pragma once
// ============================================================
//  mqtt_manager.h — Wi-Fi + MQTT connection manager
// ============================================================
#include <Arduino.h>
#include "sensors.h"
#include "control.h"

void mqtt_init(SensorData* sd, ControlState* cs);

// Call from TaskMQTT — handles reconnect + loop
void mqtt_loop();

// Publish all current sensor + state data
void mqtt_publish_all(const SensorData& sd, const ControlState& cs);

// Returns true if MQTT is currently connected
bool mqtt_connected();
