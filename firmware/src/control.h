#pragma once
// ============================================================
//  control.h — State machine types and control API
//  Strictly conforms to Section 6 of VAAYU_CONTEXT.md
// ============================================================
#include <Arduino.h>
#include "sensors.h"
#include "config.h"

// ── System states ────────────────────────────────────────────
enum SystemState {
    STATE_IDLE,          // Comfortable, fan/AC idle
    STATE_FAN_ONLY,      // Fan ladder active, monitoring for AC need
    STATE_AC_RUNNING,    // AC + fan operating in sync
    STATE_EMPTY_COOLDOWN,// Vacancy detected — auto-shutdown sequence
    STATE_ALARM          // Sensor or AC confirmation fault
};

// ── Full control state ───────────────────────────────────────
struct ControlState {
    SystemState    state            = STATE_IDLE;
    ComfortProfile profile          = PROFILE_NORMAL;
    float          targetCI         = 25.5f;
    float          currentCI        = 25.0f;
    uint8_t        fanLevel         = 0;     // 0 to 5
    uint8_t        fanDuty          = 0;     // 0 to 255 PWM
    int            acSetpoint       = 26;    // °C
    bool           acOn             = false;
    bool           exhaustOn        = false;
    bool           monsoonMode      = false;
    bool           fastTestMode     = false; // 1 min = 5s for demo video
    bool           manualMode       = false;

    // Failsafe & Alarms
    bool           alarmActive      = false;
    bool           acNotResponding  = false;
    bool           sensorFaultHold  = false;

    // Occupancy & Timers
    int            emptyMinutes     = 0;
    uint8_t        savedFanLevel    = 0;
    bool           savedAcOn        = false;
    int            savedAcSetpoint  = 26;

    // Rule counters (in virtual minutes)
    int            fullFanMinutes   = 0;
    int            comfortMinutes   = 0;
    int            hotMinutes       = 0;
    unsigned long  lastAcCommandMin = 0;
    unsigned long  lastAcOffMin     = 0;

    // AC DS18B20 verification
    bool           acConfirmPending = false;
    int            acConfirmElapsedMin = 0;
    bool           acConfirmExpectOn= false;
    uint8_t        acResendCount    = 0;

    // Exhaust tracking
    float          bathEmaBaseline  = 50.0f;
    int            exhaustLowMin    = 0;
    int            exhaustRuntimeMin= 0;

    // Manual override
    bool           overrideActive   = false;
    int            overrideMinutesLeft = 0;

    // Reason log for OLED and Dashboard
    char reason[96] = "Vaayu Hub Initialised";
};

// ── Public API ───────────────────────────────────────────────
void control_init(ControlState& cs);
void control_tick(ControlState& cs, const SensorData& sd);
inline void control_update(ControlState& cs, const SensorData& sd) { control_tick(cs, sd); }
void control_set_profile(ControlState& cs, ComfortProfile cp);
void control_set_fast_test(ControlState& cs, bool enable);
void control_set_manual_fan(ControlState& cs, uint8_t level);
void control_set_manual_ac(ControlState& cs, bool on, int temp);
void control_set_manual_exhaust(ControlState& cs, bool on);
void control_notify_ir_override(ControlState& cs, bool cooler);
float control_compute_ci(float temp, float rh, uint8_t fanLvl);
