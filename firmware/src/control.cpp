// ============================================================
//  control.cpp — Reference implementation of Vaayu Hub rules
//  Ported directly from Section 6 of VAAYU_CONTEXT.md
// ============================================================
#include "control.h"
#include "ir_ac.h"
#include <Arduino.h>
#include <esp32-hal-ledc.h>

// ── Physical Actuator Drivers ────────────────────────────────
static void fan_set_level(uint8_t lvl) {
    if (lvl >= FAN_LEVELS_COUNT) lvl = FAN_LEVELS_COUNT - 1;
    ledcWrite(FAN_PWM_CHANNEL, FAN_DUTIES[lvl]);
}

static void exhaust_set(bool on) {
    ledcWrite(EXHAUST_PWM_CH, on ? 255 : 0);
}

static void buzzer_alert(bool on) {
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

// ── Comfort Index Formula ────────────────────────────────────
// CI = T + 0.05 * (RH - 50) - credit(v)
// credit(v) = 2.5 * min(v / 0.8, 1.2)
float control_compute_ci(float temp, float rh, uint8_t fanLvl) {
    if (fanLvl >= FAN_LEVELS_COUNT) fanLvl = FAN_LEVELS_COUNT - 1;
    float v = FAN_VELOCITIES[fanLvl];
    float ratio = v / 0.8f;
    float credit = 2.5f * (ratio < 1.2f ? ratio : 1.2f);
    return temp + 0.05f * (rh - 50.0f) - credit;
}

// ── Initialisation ────────────────────────────────────────────
void control_init(ControlState& cs) {
    // 1. Room Fan PWM (L9110S A-IA on GPIO 32)
    ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_BITS);
    ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
    fan_set_level(0);

    // 2. Exhaust Fan PWM (L9110S B-IA on GPIO 14)
    ledcSetup(EXHAUST_PWM_CH, EXHAUST_PWM_FREQ, EXHAUST_PWM_BITS);
    ledcAttachPin(PIN_EXHAUST_PWM, EXHAUST_PWM_CH);
    exhaust_set(false);

    // 3. Buzzer (GPIO 19)
    pinMode(PIN_BUZZER, OUTPUT);
    buzzer_alert(false);

    control_set_profile(cs, PROFILE_NORMAL);
    snprintf(cs.reason, sizeof(cs.reason), "System Boot: Normal Profile");
    Serial.println("[control] Initialised according to VAAYU_CONTEXT.md");
}

void control_set_profile(ControlState& cs, ComfortProfile cp) {
    cs.profile     = cp;
    cs.targetCI    = PROFILES[cp].targetCI;
    cs.acSetpoint  = PROFILES[cp].acStartTemp;
    cs.monsoonMode = PROFILES[cp].monsoonMode;
}

void control_set_fast_test(ControlState& cs, bool enable) {
    cs.fastTestMode = enable;
    Serial.printf("[control] Fast Test Mode %s (1 min = 5 s)\n", enable ? "ENABLED" : "DISABLED");
}

void control_set_manual_fan(ControlState& cs, uint8_t level) {
    cs.manualMode = true;
    cs.fanLevel = min((int)level, 5);
    fan_set_level(cs.fanLevel);
}

void control_set_manual_ac(ControlState& cs, bool on, int temp) {
    cs.manualMode = true;
    cs.acOn = on;
    cs.acSetpoint = constrain(temp, 18, 30);
    AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
    ir_send_ac_command(on ? mode : AC_MODE_OFF, cs.acSetpoint, AC_FAN_AUTO);
}

void control_set_manual_exhaust(ControlState& cs, bool on) {
    cs.exhaustOn = on;
    exhaust_set(on);
}

void control_notify_ir_override(ControlState& cs, bool cooler) {
    cs.overrideActive = true;
    cs.overrideMinutesLeft = MANUAL_OVERRIDE_PAUSE_MIN;
    cs.targetCI += cooler ? -MANUAL_OVERRIDE_SHIFT_CI : MANUAL_OVERRIDE_SHIFT_CI;
    snprintf(cs.reason, sizeof(cs.reason), "Manual IR override: paused 30 min (target %.2f°C)", cs.targetCI);
    Serial.println(cs.reason);
}

// ── Control Tick (Runs Once Per Minute or 5s in Fast-Test) ───
void control_tick(ControlState& cs, const SensorData& sd) {
    // ── 1. Sensor Fault Check ────────────────────────────────
    if (sd.roomSensorFault || !sd.roomTempValid || !sd.roomHumidValid) {
        cs.sensorFaultHold = true;
        cs.alarmActive = true;
        buzzer_alert(true);
        snprintf(cs.reason, sizeof(cs.reason), "SENSOR FAULT: Holding last state");
        Serial.println("[control] SENSOR FAULT DETECTED — holding state");
        return;
    } else {
        if (cs.sensorFaultHold) {
            buzzer_alert(false);
            cs.sensorFaultHold = false;
            cs.alarmActive = false;
        }
    }

    // ── 2. AC Command Check (DS18B20 Fail-safe) ──────────────
    if (cs.acConfirmPending) {
        cs.acConfirmElapsedMin++;
        if (cs.acConfirmElapsedMin >= AC_CONFIRM_WAIT_MIN) {
            bool confirmed = false;
            if (!sd.acOutletValid) {
                // If DS18B20 disconnected (-127), skip check per Section 6
                confirmed = true;
                Serial.println("[control] DS18B20 disconnected: skipping check");
            } else {
                if (cs.acConfirmExpectOn) {
                    float dropThreshold = cs.monsoonMode ? AC_CONFIRM_DRY_DROP : AC_CONFIRM_COOL_DROP;
                    confirmed = (sd.acOutletTemp <= sd.roomTemp - dropThreshold);
                } else {
                    confirmed = (sd.acOutletTemp >= sd.roomTemp - AC_CONFIRM_OFF_DELTA);
                }
            }

            if (confirmed) {
                cs.acConfirmPending = false;
                cs.acNotResponding = false;
                cs.acResendCount = 0;
                Serial.println("[control] AC command confirmed by DS18B20");
            } else {
                if (cs.acResendCount == 0) {
                    // Resend once
                    cs.acResendCount = 1;
                    cs.acConfirmElapsedMin = 0;
                    AcMode mode = cs.acOn ? (cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL) : AC_MODE_OFF;
                    ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
                    snprintf(cs.reason, sizeof(cs.reason), "AC unconfirmed: resending command");
                    Serial.println("[control] AC command unconfirmed — resending once");
                } else {
                    // Alarm: AC not responding
                    cs.acConfirmPending = false;
                    cs.acNotResponding = true;
                    cs.alarmActive = true;
                    buzzer_alert(true);
                    snprintf(cs.reason, sizeof(cs.reason), "ALARM: AC NOT RESPONDING");
                    Serial.println("[control] ALARM: AC NOT RESPONDING after resend");
                }
            }
        }
    }

    // ── 3. Occupancy Tracking ────────────────────────────────
    if (!sd.presenceDetected) {
        cs.emptyMinutes++;
        if (cs.emptyMinutes == OCCUPANCY_EMPTY_LOG_MIN) {
            snprintf(cs.reason, sizeof(cs.reason), "Room confirmed empty (3 min)");
            Serial.println("[control] Room confirmed empty");
        } else if (cs.emptyMinutes == OCCUPANCY_FAN_OFF_MIN) {
            cs.savedFanLevel = cs.fanLevel;
            cs.fanLevel = 0;
            fan_set_level(0);
            snprintf(cs.reason, sizeof(cs.reason), "Empty 5 min: Fan turned off");
            Serial.println(cs.reason);
        } else if (cs.emptyMinutes >= OCCUPANCY_AC_OFF_MIN && cs.acOn) {
            cs.savedAcOn = true;
            cs.savedAcSetpoint = cs.acSetpoint;
            cs.acOn = false;
            ir_send_ac_command(AC_MODE_OFF, cs.acSetpoint, AC_FAN_AUTO);
            cs.acConfirmPending = true;
            cs.acConfirmExpectOn = false;
            cs.acConfirmElapsedMin = 0;
            cs.acResendCount = 0;
            snprintf(cs.reason, sizeof(cs.reason), "Empty 10 min: AC shut down");
            Serial.println(cs.reason);
            cs.state = STATE_IDLE;
            return;
        }
    } else {
        if (cs.emptyMinutes >= OCCUPANCY_FAN_OFF_MIN) {
            // Person returned! Restore fan immediately
            cs.fanLevel = (cs.savedFanLevel > 0) ? cs.savedFanLevel : 1;
            fan_set_level(cs.fanLevel);
            snprintf(cs.reason, sizeof(cs.reason), "Presence returned: Fan restored (lvl %d)", cs.fanLevel);
            Serial.println(cs.reason);
        }
        cs.emptyMinutes = 0;
    }

    // ── 4. Manual Override Pause ─────────────────────────────
    if (cs.overrideActive) {
        cs.overrideMinutesLeft--;
        if (cs.overrideMinutesLeft <= 0) {
            cs.overrideActive = false;
            snprintf(cs.reason, sizeof(cs.reason), "Manual pause ended: resuming auto");
            Serial.println(cs.reason);
        } else {
            return; // Do nothing while manual pause active
        }
    }

    // Skip automated ladder if manual mode or empty shutoff
    if (cs.manualMode || cs.emptyMinutes >= OCCUPANCY_FAN_OFF_MIN) return;

    // ── 5. Fan-First Comfort Ladder ──────────────────────────
    uint8_t maxFan = PROFILES[cs.profile].maxFanLevel;
    cs.currentCI = control_compute_ci(sd.roomTemp, sd.roomHumidity, cs.fanLevel);

    if (cs.currentCI > cs.targetCI + 0.5f && cs.fanLevel < maxFan) {
        cs.fanLevel++;
        fan_set_level(cs.fanLevel);
        cs.currentCI = control_compute_ci(sd.roomTemp, sd.roomHumidity, cs.fanLevel);
        snprintf(cs.reason, sizeof(cs.reason), "Fan up to level %d (CI %.1f > %.1f)", cs.fanLevel, cs.currentCI, cs.targetCI);
    } else if (cs.fanLevel > 0) {
        float ciLower = control_compute_ci(sd.roomTemp, sd.roomHumidity, cs.fanLevel - 1);
        if (ciLower <= cs.targetCI + 0.2f) {
            cs.fanLevel--;
            fan_set_level(cs.fanLevel);
            cs.currentCI = control_compute_ci(sd.roomTemp, sd.roomHumidity, cs.fanLevel);
            snprintf(cs.reason, sizeof(cs.reason), "Fan down to level %d (CI %.1f comfortable)", cs.fanLevel, cs.currentCI);
        }
    }

    // ── 6. Monsoon Mode Check (Humid Profile) ────────────────
    if (cs.profile == PROFILE_HUMID) {
        if (sd.roomHumidity > 70.0f && sd.roomTemp >= 26.0f && sd.roomTemp <= 30.0f) {
            if (!cs.monsoonMode) {
                cs.monsoonMode = true;
                if (cs.acOn) ir_send_ac_command(AC_MODE_DRY, cs.acSetpoint, AC_FAN_AUTO);
            }
        } else if (cs.monsoonMode && (sd.roomTemp > 30.0f || sd.roomHumidity < 60.0f)) {
            cs.monsoonMode = false;
            if (cs.acOn) ir_send_ac_command(AC_MODE_COOL, cs.acSetpoint, AC_FAN_AUTO);
        }
    }

    // ── 7. AC Activation (Rule 1: 10 min at full fan) ────────
    if (!cs.acOn) {
        if (cs.fanLevel == maxFan && cs.currentCI > cs.targetCI + 0.5f) {
            cs.fullFanMinutes++;
            if (cs.fullFanMinutes >= AC_FULL_FAN_TRIGGER_MIN) {
                // Check compressor lockout
                cs.acOn = true;
                cs.acSetpoint = PROFILES[cs.profile].acStartTemp;
                AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
                ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);

                cs.acConfirmPending = true;
                cs.acConfirmExpectOn = true;
                cs.acConfirmElapsedMin = 0;
                cs.acResendCount = 0;
                cs.fullFanMinutes = 0;
                cs.comfortMinutes = 0;
                cs.state = STATE_AC_RUNNING;
                snprintf(cs.reason, sizeof(cs.reason), "Full fan 10 min: AC ON at %d°C", cs.acSetpoint);
                Serial.println(cs.reason);
            } else {
                snprintf(cs.reason, sizeof(cs.reason), "Fan at max: AC countdown (%d/10 min)", cs.fullFanMinutes);
            }
        } else {
            cs.fullFanMinutes = 0;
            cs.state = (cs.fanLevel > 0) ? STATE_FAN_ONLY : STATE_IDLE;
        }
    } else {
        // ── 8. AC Set-Point Creep (Rule 2: 15 min comfortable) ──
        cs.state = STATE_AC_RUNNING;
        if (cs.currentCI <= cs.targetCI + 0.5f) {
            cs.comfortMinutes++;
            if (cs.comfortMinutes >= AC_SETPOINT_CREEP_MIN) {
                int cap = PROFILES[cs.profile].acCapTemp;
                if (cs.acSetpoint < cap) {
                    cs.acSetpoint++;
                    AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
                    ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
                    snprintf(cs.reason, sizeof(cs.reason), "Comfortable 15 min: AC creep to %d°C", cs.acSetpoint);
                    Serial.println(cs.reason);
                } else {
                    snprintf(cs.reason, sizeof(cs.reason), "AC at %d°C cap (maximum energy saving)", cs.acSetpoint);
                }
                cs.comfortMinutes = 0;
            } else {
                snprintf(cs.reason, sizeof(cs.reason), "AC %d°C: creep countdown (%d/15 min)", cs.acSetpoint, cs.comfortMinutes);
            }
            cs.hotMinutes = 0;
        } else {
            cs.comfortMinutes = 0;
            // ── 9. Give Some Back (Rule 9: 10 min hot at max fan) ─
            if (cs.fanLevel == maxFan && cs.currentCI > cs.targetCI + 1.0f) {
                cs.hotMinutes++;
                if (cs.hotMinutes >= AC_GIVE_BACK_MIN) {
                    int startTemp = PROFILES[cs.profile].acStartTemp;
                    if (cs.acSetpoint > startTemp) {
                        cs.acSetpoint--;
                        AcMode mode = cs.monsoonMode ? AC_MODE_DRY : AC_MODE_COOL;
                        ir_send_ac_command(mode, cs.acSetpoint, AC_FAN_AUTO);
                        snprintf(cs.reason, sizeof(cs.reason), "Hot 10 min: AC lower to %d°C", cs.acSetpoint);
                    }
                    cs.hotMinutes = 0;
                }
            } else {
                cs.hotMinutes = 0;
            }
        }
    }

    // ── 10. Bathroom Exhaust Rules (DHT11) ────────────────────
    if (sd.bathHumidValid && !sd.bathSensorFault) {
        if (!cs.exhaustOn) {
            // Update rolling 30-min EMA baseline while exhaust is off
            cs.bathEmaBaseline += EXHAUST_EMA_ALPHA * (sd.bathHumidity - cs.bathEmaBaseline);

            // ON if humidity rises >= 8% or > 75% and >= 3% above baseline
            bool spike = (sd.bathHumidity >= cs.bathEmaBaseline + EXHAUST_TRIGGER_SPIKE);
            bool highHumid = (sd.bathHumidity >= EXHAUST_TRIGGER_HIGH_RH && sd.bathHumidity >= cs.bathEmaBaseline + EXHAUST_TRIGGER_HIGH_DELTA);

            if (spike || highHumid) {
                cs.exhaustOn = true;
                cs.exhaustRuntimeMin = 0;
                cs.exhaustLowMin = 0;
                exhaust_set(true);
                snprintf(cs.reason, sizeof(cs.reason), "Bathroom steam: Exhaust ON (%.1f%%)", sd.bathHumidity);
                Serial.println(cs.reason);
            }
        } else {
            cs.exhaustRuntimeMin++;
            // Hard 45 min runtime limit
            if (cs.exhaustRuntimeMin >= EXHAUST_MAX_RUNTIME_MIN) {
                cs.exhaustOn = false;
                exhaust_set(false);
                snprintf(cs.reason, sizeof(cs.reason), "Exhaust OFF: 45 min limit reached");
                Serial.println(cs.reason);
            } else {
                // OFF once within 3% of baseline for 2 min in a row
                if (sd.bathHumidity <= cs.bathEmaBaseline + EXHAUST_OFF_DELTA) {
                    cs.exhaustLowMin++;
                    if (cs.exhaustLowMin >= EXHAUST_OFF_STABLE_MIN) {
                        cs.exhaustOn = false;
                        exhaust_set(false);
                        snprintf(cs.reason, sizeof(cs.reason), "Bathroom recovered: Exhaust OFF");
                        Serial.println(cs.reason);
                    }
                } else {
                    cs.exhaustLowMin = 0;
                }
            }
        }
    }
}
