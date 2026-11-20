#pragma once
// ============================================================
//  ir_ac.h — AC IR command sender and override detector
//
//  Protocol: Daikin ARC480 (ARC480A14) by default.
//  To swap brand: replace ir_send_ac_command() body in ir_ac.cpp
//  with the appropriate IRac / IRsend call for your AC model.
//  IRremoteESP8266 supports 100+ protocols — see its wiki.
// ============================================================
#include <Arduino.h>

// AC operating modes passed to ir_send_ac_command()
enum AcMode {
    AC_MODE_COOL,
    AC_MODE_DRY,
    AC_MODE_FAN,
    AC_MODE_AUTO,
    AC_MODE_OFF
};

// Fan speed levels for IR commands
enum AcFanSpeed {
    AC_FAN_AUTO,
    AC_FAN_LOW,
    AC_FAN_MEDIUM,
    AC_FAN_HIGH
};

// ── Public API ───────────────────────────────────────────────

void ir_init();

// Send an IR command to the AC unit
// mode      — cool / dry / fan / off
// tempC     — setpoint (16–30°C, clamped inside)
// fanSpeed  — AUTO recommended; control.cpp passes AUTO
void ir_send_ac_command(AcMode mode, int tempC, AcFanSpeed fanSpeed);

// Returns true if the last IR receive event was from a human remote
// (not from our own transmitter). Call frequently from TaskIR.
// Sets irSignalDetected and irSignalTime internally.
bool ir_check_human_override();
