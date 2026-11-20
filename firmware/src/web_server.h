#pragma once
// ============================================================
//  web_server.h — Built-in ESP32 SoftAP Web Dashboard & Logger
// ============================================================
#include <Arduino.h>
#include "sensors.h"
#include "control.h"

void web_server_init(ControlState& cs, SensorData& sd);
void web_server_handle();
void web_server_log_minute(const ControlState& cs, const SensorData& sd);
