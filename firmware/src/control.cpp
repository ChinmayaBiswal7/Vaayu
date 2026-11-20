// ============================================================
//  control.cpp — Full Vaayu Hub state machine
//
//  Rules implemented (see config.h for all thresholds):
//  1. Fan First: fan proportional to temp, AC after 10 min full fan
//  2. AC Setpoint Creep: +1°C every 15 min while comfortable, max 28°C
//  3. Empty Room Shut-off: fan off @5 min, AC off @10 min absence
//  4. Exhaust Fan: bath humidity-driven with 2-min off delay
//  5. Fail-safes: OOR hold + 30 s alarm, AC confirm via DS18B20, IR override
//  6. Distance-aware fan, Monsoon mode (Dry AC mode)
// ============================================================
#include "control.h"
#include "ir_ac.h"
#include <Arduino.h>
#include <esp32-hal-ledc.h>

// ── PWM helpers ──────────────────────────────────────────────
static void fan_set(uint8_t duty) {
    ledcWrite(FAN_PWM_CHANNEL, duty);
}
static void exhaust_set(uint8_t duty) {
    ledcWrite(EXHAUST_PWM_CH, duty);
}
static void buzzer_beep(uint16_t durationMs) {
    tone(PIN_BUZZER, BUZZER_FREQ_HZ, durationMs);
}

// ── Map distance to a fan speed blend factor (0.0–1.0) ───────
// Closer target → closer to 1.0 (full speed from temp calc)
// Further target → down to 0.6 (60% of temp-driven speed)
static float distance_blend(uint16_t distCm) {
    if (distCm <= RADAR_DISTANCE_NEAR_CM) return 1.0f;
    if (distCm >= RADAR_DISTANCE_FAR_CM)  return 0.6f;
    float ratio = (float)(distCm - RADAR_DISTANCE_NEAR_CM) /
                  (float)(RADAR_DISTANCE_FAR_CM - RADAR_DISTANCE_NEAR_CM);
    return 1.0f - ratio * 0.4f;  // 1.0 → 0.6
}

// ── Compute temp-driven fan duty (0-255) ─────────────────────
static uint8_t temp_to_fan_duty(float temp) {
    if (temp <= TEMP_FAN_ON)  return 0;
    if (temp >= TEMP_FAN_MAX) return 255;
    float ratio = (temp - TEMP_FAN_ON) / (TEMP_FAN_MAX - TEMP_FAN_ON);
    return (uint8_t)(ratio * 255.0f);
}

// ── Sensor alarm check ───────────────────────────────────────
static bool check_sensor_health(ControlState& cs, const SensorData& sd) {
    unsigned long now = millis();
    bool roomBad = !sd.roomTempValid ||
                   (now - sd.lastRoomTempMs > DELAY_SENSOR_ALARM_MS + 5000UL);

    if (roomBad) {
        if (cs.sensorAlarmStartMs == 0) cs.sensorAlarmStartMs = now;
        if (now - cs.sensorAlarmStartMs >= DELAY_SENSOR_ALARM_MS) {
            return true;  // trigger alarm
        }
    } else {
        cs.sensorAlarmStartMs = 0;
    }
    return false;
}

// ── Exhaust fan control ───────────────────────────────────────
static void update_exhaust(ControlState& cs, const SensorData& sd) {
    unsigned long now = millis();

    if (!cs.exhaustOn) {
        if (sd.bathHumidValid && sd.bathHumidity > HUMIDITY_EXHAUST_ON) {
            cs.exhaustOn = true;
            cs.exhaustLowHumidMs = 0;
            exhaust_set(255);
            Serial.println("[control] Exhaust ON (bath humidity high)");
        }
    } else {
        // Exhaust is on — wait for humidity to drop AND stay low
        if (sd.bathHumidValid && sd.bathHumidity < HUMIDITY_EXHAUST_OFF) {
            if (cs.exhaustLowHumidMs == 0) cs.exhaustLowHumidMs = now;
            if (now - cs.exhaustLowHumidMs >= DELAY_EXHAUST_OFF_MS) {
                cs.exhaustOn = false;
                cs.exhaustLowHumidMs = 0;
                exhaust_set(0);
                Serial.println("[control] Exhaust OFF (humidity normal)");
            }
        } else {
            cs.exhaustLowHumidMs = 0;  // reset timer if humidity rises again
        }
    }
}

// ── Monsoon mode check ────────────────────────────────────────
static void update_monsoon(ControlState& cs, const SensorData& sd) {
    bool newMonsoon = sd.roomHumidValid && sd.roomHumidity > HUMIDITY_MONSOON;
    if (newMonsoon != cs.monsoonMode) {
        cs.monsoonMode = newMonsoon;
        if (cs.acOn) {
            // Resend AC command with updated mode
            AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
            ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
        }
    }
}

// ── AC confirmation via DS18B20 ───────────────────────────────
static void update_ac_confirm(ControlState& cs, const SensorData& sd) {
    if (!cs.acConfirmPending) return;
    unsigned long now = millis();
    if (now - cs.acCommandSentMs < DELAY_AC_CONFIRM_MS) return;  // too early

    if (!sd.acOutletValid) {
        Serial.println("[control] DS18B20 not available for AC confirm");
        cs.acConfirmPending = false;
        return;
    }

    float drop = cs.acConfirmBaseline - sd.acOutletTemp;
    if (drop >= AC_CONFIRM_DROP) {
        cs.acConfirmed      = true;
        cs.acConfirmPending = false;
        cs.acResendCount    = 0;
        Serial.printf("[control] AC confirmed (outlet dropped %.1f°C)\n", drop);
    } else {
        // Not confirmed
        if (cs.acResendCount == 0) {
            Serial.println("[control] AC not confirmed — resending command");
            AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
            ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
            cs.acCommandSentMs = now;
            cs.acResendCount   = 1;
        } else {
            // Already retried — buzzer alarm
            Serial.println("[control] AC still not confirmed after retry — buzzer!");
            buzzer_beep(2000);
            cs.acConfirmPending = false;
        }
    }
}

// ── IR override ───────────────────────────────────────────────
void control_notify_ir_override(ControlState& cs) {
    cs.irOverride          = true;
    cs.irOverrideStartMs   = millis();
    snprintf(cs.reason, sizeof(cs.reason), "Manual IR override (30 min)");
    Serial.println("[control] IR override engaged");
}

// ── Manual dashboard commands ─────────────────────────────────
void control_set_manual_fan(ControlState& cs, uint8_t pct) {
    if (!cs.irOverride) return;  // only in override mode
    cs.fanDuty = (uint8_t)(pct * 255U / 100U);
    fan_set(cs.fanDuty);
}
void control_set_manual_ac_setpoint(ControlState& cs, int sp) {
    cs.acSetpoint = constrain(sp, AC_SETPOINT_MIN, AC_SETPOINT_MAX);
    if (cs.acOn) {
        AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
        ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
    }
}
void control_set_manual_ac_state(ControlState& cs, bool on) {
    if (cs.acOn == on) return;
    cs.acOn = on;
    if (on) {
        AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
        ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
    } else {
        ir_send_ac_command(AC_MODE_OFF, cs.acSetpoint, AC_FAN_AUTO);
    }
}

// ── Initialisation ────────────────────────────────────────────
void control_init(ControlState& cs) {
    // L9110S Motor Driver: Room Fan (Channel A)
    pinMode(PIN_FAN_IB, OUTPUT);
    digitalWrite(PIN_FAN_IB, LOW);
    ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_BITS);
    ledcAttachPin(PIN_FAN_IA, FAN_PWM_CHANNEL);

    // L9110S Motor Driver: Exhaust Fan (Channel B)
    pinMode(PIN_EXHAUST_IB, OUTPUT);
    digitalWrite(PIN_EXHAUST_IB, LOW);
    ledcSetup(EXHAUST_PWM_CH, EXHAUST_PWM_FREQ, EXHAUST_PWM_BITS);
    ledcAttachPin(PIN_EXHAUST_IA, EXHAUST_PWM_CH);

    // Buzzer
    pinMode(PIN_BUZZER, OUTPUT);

    fan_set(0);
    exhaust_set(0);
    snprintf(cs.reason, sizeof(cs.reason), "Booting...");
    Serial.println("[control] Initialised for L9110S Motor Driver");
}

// ── Main control update ───────────────────────────────────────
void control_update(ControlState& cs, const SensorData& sd) {
    unsigned long now = millis();

    // ── Exhaust and Monsoon are always evaluated ──────────────
    update_exhaust(cs, sd);
    update_monsoon(cs, sd);
    update_ac_confirm(cs, sd);

    // ── IR override window ────────────────────────────────────
    if (cs.irOverride) {
        if (now - cs.irOverrideStartMs >= DELAY_IR_OVERRIDE_MS) {
            cs.irOverride = false;
            Serial.println("[control] IR override expired — resuming auto");
        } else {
            snprintf(cs.reason, sizeof(cs.reason),
                     "Manual override (%lu min left)",
                     (DELAY_IR_OVERRIDE_MS - (now - cs.irOverrideStartMs)) / 60000UL);
            return;  // Skip all automation
        }
    }

    // ── Sensor alarm ──────────────────────────────────────────
    if (check_sensor_health(cs, sd)) {
        if (cs.state != STATE_ALARM) {
            cs.state       = STATE_ALARM;
            cs.alarmActive = true;
            buzzer_beep(500);
            snprintf(cs.reason, sizeof(cs.reason), "ALARM: Sensor failure!");
            Serial.println("[control] Entered ALARM state");
        }
        // Keep last fan/AC state; don't change hardware in alarm
        return;
    }
    if (cs.state == STATE_ALARM) {
        // Sensor recovered
        cs.state       = STATE_IDLE;
        cs.alarmActive = false;
        snprintf(cs.reason, sizeof(cs.reason), "Sensor recovered");
        buzzer_beep(100);
    }

    float temp     = sd.roomTemp;
    float humidity = sd.roomHumidity;
    bool  present  = sd.presenceDetected;

    // ── Empty room detection ──────────────────────────────────
    if (!present) {
        if (cs.emptyDetectedMs == 0) {
            cs.emptyDetectedMs = now;
        }
        unsigned long emptyFor = now - cs.emptyDetectedMs;

        if (emptyFor >= DELAY_EMPTY_AC_OFF_MS && cs.acOn) {
            cs.acOn = false;
            ir_send_ac_command(AC_MODE_OFF, cs.acSetpoint, AC_FAN_AUTO);
            snprintf(cs.reason, sizeof(cs.reason),
                     "Room empty >10 min — AC off");
            Serial.println("[control] AC off: empty room");
        }
        if (emptyFor >= DELAY_EMPTY_FAN_OFF_MS && cs.fanDuty > 0) {
            cs.fanDuty = 0;
            fan_set(0);
            cs.state   = STATE_EMPTY_COOLDOWN;
            if (!cs.acOn)
                snprintf(cs.reason, sizeof(cs.reason),
                         "Room empty >5 min — fan off");
            Serial.println("[control] Fan off: empty room");
        }
        return;  // Don't apply normal rules while empty
    }

    // Room is occupied — reset empty timer
    if (cs.emptyDetectedMs != 0) {
        cs.emptyDetectedMs = 0;
        Serial.println("[control] Presence restored");
    }

    // ── RULE 1: Fan-first logic ───────────────────────────────
    uint8_t targetDuty = temp_to_fan_duty(temp);

    // Distance-aware blend (only when presence is confirmed)
    if (sd.radarValid && sd.presenceDetected) {
        float blend = distance_blend(sd.targetDistanceCm);
        targetDuty  = (uint8_t)(targetDuty * blend);
    }

    if (targetDuty == 0) {
        // Comfortable — no fan needed
        if (cs.fanDuty > 0) {
            cs.fanDuty = 0;
            fan_set(0);
        }
        if (cs.acOn) {
            cs.acOn = false;
            ir_send_ac_command(AC_MODE_OFF, cs.acSetpoint, AC_FAN_AUTO);
            snprintf(cs.reason, sizeof(cs.reason),
                     "Temp OK (%.1f°C) — all off", temp);
        } else {
            snprintf(cs.reason, sizeof(cs.reason),
                     "Comfortable %.1f°C — idle", temp);
        }
        cs.state           = STATE_IDLE;
        cs.fullFanSinceMs  = 0;
        return;
    }

    // Fan is needed
    cs.fanDuty = targetDuty;
    fan_set(cs.fanDuty);

    bool atFullFan = (targetDuty == 255);

    if (atFullFan && cs.fullFanSinceMs == 0) {
        cs.fullFanSinceMs = now;
        Serial.println("[control] Full fan speed started");
    }
    if (!atFullFan) {
        cs.fullFanSinceMs = 0;  // Reset if fan drops below 100%
    }

    // Should AC turn on?
    if (!cs.acOn) {
        bool fanTimerMet = atFullFan &&
                           cs.fullFanSinceMs > 0 &&
                           (now - cs.fullFanSinceMs >= DELAY_AC_ON_MS);
        bool tempMet     = temp >= TEMP_AC_ON;

        if (fanTimerMet && tempMet) {
            cs.acOn          = true;
            cs.state         = STATE_AC_RUNNING;
            cs.acConfirmed   = false;
            cs.acResendCount = 0;
            cs.acConfirmPending  = true;
            cs.acCommandSentMs   = now;
            cs.acConfirmBaseline = sd.acOutletValid ? sd.acOutletTemp : temp;
            cs.fullFanSinceMs    = 0;

            AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
            ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);

            snprintf(cs.reason, sizeof(cs.reason),
                     "AC ON @%d°C (%.1f°C, 10min full fan)", cs.acSetpoint, temp);
            Serial.printf("[control] AC turned ON @%d°C\n", cs.acSetpoint);
        } else {
            cs.state = STATE_FAN_ONLY;
            unsigned long remaining = 0;
            if (atFullFan && cs.fullFanSinceMs > 0) {
                unsigned long elapsed = now - cs.fullFanSinceMs;
                if (elapsed < DELAY_AC_ON_MS)
                    remaining = (DELAY_AC_ON_MS - elapsed) / 60000UL;
            }
            if (remaining > 0)
                snprintf(cs.reason, sizeof(cs.reason),
                         "Fan 100%% (%.1f°C) — AC in %lu min", temp, remaining);
            else
                snprintf(cs.reason, sizeof(cs.reason),
                         "Fan %d%% (%.1f°C)", (int)(targetDuty * 100 / 255), temp);
        }
    } else {
        // ── RULE 2: AC is running — setpoint creep ───────────
        cs.state = STATE_AC_RUNNING;

        if (temp <= TEMP_AC_CREEP) {
            if (cs.lastCreepMs == 0) cs.lastCreepMs = now;
            if (now - cs.lastCreepMs >= DELAY_AC_CREEP_MS) {
                if (cs.acSetpoint < AC_SETPOINT_MAX) {
                    cs.acSetpoint++;
                    cs.lastCreepMs = now;
                    AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
                    ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
                    snprintf(cs.reason, sizeof(cs.reason),
                             "AC creep → %d°C setpoint (comfortable)", cs.acSetpoint);
                    Serial.printf("[control] AC setpoint crept to %d°C\n", cs.acSetpoint);
                } else {
                    snprintf(cs.reason, sizeof(cs.reason),
                             "AC %d°C (max setpoint, comfortable)", cs.acSetpoint);
                }
            } else {
                unsigned long nextCreep = (DELAY_AC_CREEP_MS - (now - cs.lastCreepMs)) / 60000UL;
                snprintf(cs.reason, sizeof(cs.reason),
                         "AC %d°C, creep in %lu min", cs.acSetpoint, nextCreep);
            }
        } else {
            cs.lastCreepMs = 0;
            snprintf(cs.reason, sizeof(cs.reason),
                     "AC %d°C, cooling (%.1f°C)", cs.acSetpoint, temp);
        }
    }
}
