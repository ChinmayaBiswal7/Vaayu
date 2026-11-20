#pragma once
// ============================================================
//  display.h — OLED display manager (SSD1306 128×64)
// ============================================================
#include <Arduino.h>
#include "sensors.h"
#include "control.h"

void display_init();

// Call every TASK_DISPLAY_MS (1 s)
// Renders current readings + reason string.
// Long reason strings scroll horizontally.
void display_update(const SensorData& sd, const ControlState& cs);
