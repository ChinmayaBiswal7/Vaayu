#pragma once
// ============================================================
//  control.h — State machine types and control API
// ============================================================
#include <Arduino.h>
#include "sensors.h"
#include "config.h"

// ── System states ────────────────────────────────────────────
enum SystemState {
    STATE_IDLE,          // Temp OK, nothing running
    STATE_FAN_ONLY,      // Fan running, monitoring for AC need
    STATE_AC_RUNNING,    // AC + fan both running
    STATE_EMPTY_COOLDOWN,// No presence — winding down
    STATE_ALARM          // Sensor failure alarm
};

// ── Full control state (persisted across loop cycles) ────────
struct ControlState {
    SystemState state          = STATE_IDLE;
    int         acSetpoint     = AC_SETPOINT_DEFAULT;  // current AC setpoint °C
    uint8_t     fanDuty        = 0;      // 0-255 PWM
    bool        acOn           = false;
    bool        exhaustOn      = false;
    bool        monsoonMode    = false;
    bool        irOverride     = false;  // human pressed remote
    bool        alarmActive    = false;

    // Timestamps for timer-based decisions
    unsigned long fullFanSinceMs      = 0;  // when fan first hit 100%
    unsigned long emptyDetectedMs     = 0;  // when room first went empty
    unsigned long lastCreepMs         = 0;  // last AC setpoint creep time
    unsigned long irOverrideStartMs   = 0;  // when override started
    unsigned long exhaustLowHumidMs   = 0;  // when humidity first dropped below OFF threshold
    unsigned long sensorAlarmStartMs  = 0;  // when sensor went bad
    unsigned long acCommandSentMs     = 0;  // when last AC command was sent

    bool         acConfirmPending   = false;  // waiting for DS18B20 confirmation
    float        acConfirmBaseline  = 0.0f;   // outlet temp before AC command
    bool         acConfirmed        = false;
    uint8_t      acResendCount      = 0;

    // Human-readable reason for last action (shown on OLED + published)
    char reason[80] = "Initialising...";
};

// ── Public API ───────────────────────────────────────────────

void control_init(ControlState& cs);

// Main loop — call every TASK_CONTROL_MS
// Writes fan PWM, AC commands, exhaust PWM.
// Updates cs.reason.
void control_update(ControlState& cs, const SensorData& sd);

// Called by IR task when a human remote press is detected
void control_notify_ir_override(ControlState& cs);

// Apply manual fan speed from dashboard (0-100%)
void control_set_manual_fan(ControlState& cs, uint8_t pct);

// Apply manual AC setpoint from dashboard
void control_set_manual_ac_setpoint(ControlState& cs, int setpoint);

// Apply manual AC on/off from dashboard
void control_set_manual_ac_state(ControlState& cs, bool on);
