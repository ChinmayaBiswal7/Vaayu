// ============================================================
//  test_control.cpp — Unity tests for Vaayu Hub control.cpp
//
//  Compile target: [env:native] in platformio.ini
//  Run: pio test -e native
//
//  Strategy:
//    - control_update() calls millis(), ledcWrite(), tone(),
//      ir_send_ac_command() — all stubbed below so the core
//      logic runs on a plain PC without hardware.
//    - Each test sets up a fresh ControlState + SensorData,
//      manipulates internal timestamp fields, then calls
//      control_update() and asserts expectations.
// ============================================================

#include <unity.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// ── Stub / shim declarations ─────────────────────────────────
// These replace Arduino / ESP32 / library symbols for native build.

// millis() shim — tests override g_fake_millis to simulate time
static unsigned long g_fake_millis = 0;
unsigned long millis() { return g_fake_millis; }

// ledcWrite stub — record last call so we can assert duty
static int    g_last_ledc_channel = -1;
static uint32_t g_last_ledc_duty  = 0;
void ledcWrite(uint8_t ch, uint32_t duty) {
    g_last_ledc_channel = ch;
    g_last_ledc_duty    = duty;
}
void ledcSetup(uint8_t, uint32_t, uint8_t) {}
void ledcAttachPin(uint8_t, uint8_t) {}

// tone stub
void tone(uint8_t, unsigned int, unsigned long) {}

// pinMode / digitalWrite stubs
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}

// Serial stub
struct FakeSerial {
    void println(const char*) {}
    void printf(const char*, ...) {}
} Serial;

// ir_send_ac_command stub — track last call
#include "ir_ac.h"
static AcMode   g_last_ac_mode   = AC_MODE_OFF;
static int      g_last_ac_setpt  = 0;
static AcFanSpeed g_last_ac_fan  = AC_FAN_AUTO;
void ir_send_ac_command(AcMode m, int sp, AcFanSpeed f) {
    g_last_ac_mode  = m;
    g_last_ac_setpt = sp;
    g_last_ac_fan   = f;
}
bool ir_check_override() { return false; }
void ir_init() {}

// ── Include the module under test ────────────────────────────
#include "control.h"

// ── Helpers ──────────────────────────────────────────────────

// Return a fully-valid SensorData so individual tests only
// change the fields they care about.
static SensorData default_sensor() {
    SensorData sd;
    sd.roomTemp         = 24.0f;
    sd.roomHumidity     = 50.0f;
    sd.roomTempValid    = true;
    sd.roomHumidValid   = true;
    sd.bathTemp         = 25.0f;
    sd.bathHumidity     = 50.0f;
    sd.bathTempValid    = true;
    sd.bathHumidValid   = true;
    sd.acOutletTemp     = 25.0f;
    sd.acOutletValid    = true;
    sd.presenceDetected = true;
    sd.targetDistanceCm = 200;
    sd.radarValid       = true;
    // lastXxxMs fields default to 0 — control_update() checks
    // health only via roomTempValid and lastRoomTempMs vs now.
    // We set lastRoomTempMs to "now" (g_fake_millis) in each test.
    sd.lastRoomTempMs   = 0;
    sd.lastRoomHumidMs  = 0;
    sd.lastBathHumidMs  = 0;
    sd.lastAcOutletMs   = 0;
    sd.lastRadarMs      = 0;
    return sd;
}

// Stamp all lastXxxMs to current fake time so sensor health
// check passes (sensor freshness window is roomTempValid flag).
static void stamp_fresh(SensorData& sd) {
    sd.lastRoomTempMs  = g_fake_millis;
    sd.lastRoomHumidMs = g_fake_millis;
    sd.lastBathHumidMs = g_fake_millis;
    sd.lastAcOutletMs  = g_fake_millis;
    sd.lastRadarMs     = g_fake_millis;
}

static ControlState fresh_cs() {
    ControlState cs;
    // Reset to defaults (struct uses in-class initializers but we
    // reset manually for clarity across all compilers).
    cs.state           = STATE_IDLE;
    cs.acSetpoint      = AC_SETPOINT_DEFAULT;
    cs.fanDuty         = 0;
    cs.acOn            = false;
    cs.exhaustOn       = false;
    cs.monsoonMode     = false;
    cs.irOverride      = false;
    cs.alarmActive     = false;
    cs.fullFanSinceMs  = 0;
    cs.emptyDetectedMs = 0;
    cs.lastCreepMs     = 0;
    cs.irOverrideStartMs  = 0;
    cs.exhaustLowHumidMs  = 0;
    cs.sensorAlarmStartMs = 0;
    cs.acCommandSentMs    = 0;
    cs.acConfirmPending   = false;
    cs.acConfirmBaseline  = 0.0f;
    cs.acConfirmed        = false;
    cs.acResendCount      = 0;
    snprintf(cs.reason, sizeof(cs.reason), "Initialising...");
    return cs;
}

// ── Unity lifecycle ──────────────────────────────────────────
void setUp() {
    g_fake_millis       = 0;
    g_last_ledc_channel = -1;
    g_last_ledc_duty    = 0;
    g_last_ac_mode      = AC_MODE_OFF;
    g_last_ac_setpt     = 0;
}
void tearDown() {}

// ═════════════════════════════════════════════════════════════
//  TESTS
// ═════════════════════════════════════════════════════════════

// 1. Fan starts above 24°C
void test_fan_starts_above_24c() {
    g_fake_millis = 1000;
    ControlState cs = fresh_cs();
    SensorData   sd = default_sensor();
    sd.roomTemp        = 25.0f;
    sd.roomTempValid   = true;
    sd.presenceDetected = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_EQUAL(STATE_FAN_ONLY, cs.state);
    TEST_ASSERT_GREATER_THAN(0, cs.fanDuty);
}

// 2. AC does NOT turn on after only 9 min at full fan
void test_ac_requires_10min_full_fan() {
    // temp=27 gives full fan (above TEMP_FAN_MAX=30 is 255,
    // at 27 it's proportional ~128; we want FULL fan so use 31).
    // Use 30.5°C to guarantee full fan (temp >= TEMP_FAN_MAX=30).
    g_fake_millis = 0;
    ControlState cs = fresh_cs();
    cs.state          = STATE_FAN_ONLY;
    // Simulate fan has been at full for 9 minutes
    cs.fullFanSinceMs = 0;                       // started at t=0
    g_fake_millis     = 9UL * 60 * 1000;         // now = 9 min
    SensorData sd     = default_sensor();
    sd.roomTemp        = 30.5f;   // above TEMP_FAN_MAX → full fan
    sd.roomTempValid   = true;
    sd.presenceDetected = true;
    // radarValid false so distance blend is skipped → pure temp duty
    sd.radarValid = false;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_FALSE(cs.acOn);
    TEST_ASSERT_NOT_EQUAL(STATE_AC_RUNNING, cs.state);
}

// 3. AC DOES turn on after 11 min at full fan
void test_ac_turns_on_after_10min() {
    g_fake_millis = 0;
    ControlState cs   = fresh_cs();
    cs.state          = STATE_FAN_ONLY;
    cs.fullFanSinceMs = 0;
    g_fake_millis     = 11UL * 60 * 1000;   // 11 min
    SensorData sd     = default_sensor();
    sd.roomTemp        = 30.5f;   // full fan + >= TEMP_AC_ON(26)
    sd.roomTempValid   = true;
    sd.presenceDetected = true;
    sd.radarValid = false;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_TRUE(cs.acOn);
    TEST_ASSERT_EQUAL(STATE_AC_RUNNING, cs.state);
}

// 4. Setpoint creeps +1°C after 16 min comfortable in AC_RUNNING
void test_setpoint_creep() {
    g_fake_millis = 0;
    ControlState cs   = fresh_cs();
    cs.state          = STATE_AC_RUNNING;
    cs.acOn           = true;
    cs.acSetpoint     = 24;
    cs.lastCreepMs    = 0;        // creep timer starts at 0
    g_fake_millis     = 16UL * 60 * 1000;   // 16 min later
    SensorData sd     = default_sensor();
    sd.roomTemp        = 24.5f;   // comfortable (< TEMP_AC_CREEP=25)
    sd.roomTempValid   = true;
    sd.presenceDetected = true;
    sd.radarValid = false;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_EQUAL(25, cs.acSetpoint);  // was 24, should be 25
}

// 5. Setpoint does NOT exceed 28°C
void test_setpoint_max_28() {
    g_fake_millis = 0;
    ControlState cs   = fresh_cs();
    cs.state          = STATE_AC_RUNNING;
    cs.acOn           = true;
    cs.acSetpoint     = AC_SETPOINT_MAX;  // already at 28
    cs.lastCreepMs    = 0;
    g_fake_millis     = 20UL * 60 * 1000;
    SensorData sd     = default_sensor();
    sd.roomTemp        = 24.5f;
    sd.roomTempValid   = true;
    sd.presenceDetected = true;
    sd.radarValid = false;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_EQUAL(AC_SETPOINT_MAX, cs.acSetpoint);  // must stay at 28
}

// 6. Fan turns off after 5 min with no presence
void test_empty_room_fan_off_at_5min() {
    g_fake_millis = 0;
    ControlState cs   = fresh_cs();
    cs.fanDuty        = 150;      // fan was running
    cs.emptyDetectedMs = 0;       // empty detected at t=0
    g_fake_millis     = 5UL * 60 * 1000 + 1;  // just past 5 min
    SensorData sd     = default_sensor();
    sd.presenceDetected = false;
    sd.roomTempValid    = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_EQUAL(0, cs.fanDuty);
}

// 7. AC turns off after 10 min with no presence
void test_empty_room_ac_off_at_10min() {
    g_fake_millis = 0;
    ControlState cs   = fresh_cs();
    cs.state          = STATE_AC_RUNNING;
    cs.acOn           = true;
    cs.fanDuty        = 150;
    cs.emptyDetectedMs = 0;
    g_fake_millis     = 10UL * 60 * 1000 + 1;
    SensorData sd     = default_sensor();
    sd.presenceDetected = false;
    sd.roomTempValid    = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_FALSE(cs.acOn);
}

// 8. Exhaust turns on when bathroom humidity > 70%
void test_exhaust_on_above_70_humidity() {
    g_fake_millis = 1000;
    ControlState cs = fresh_cs();
    cs.exhaustOn    = false;
    SensorData sd   = default_sensor();
    sd.bathHumidity  = 75.0f;
    sd.bathHumidValid = true;
    sd.roomTempValid  = true;
    sd.presenceDetected = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_TRUE(cs.exhaustOn);
}

// 9. Exhaust stays ON when humidity < 60% but delay not met (< 2 min)
void test_exhaust_off_below_60_with_delay() {
    g_fake_millis = 0;
    ControlState cs      = fresh_cs();
    cs.exhaustOn         = true;
    cs.exhaustLowHumidMs = 0;   // just started the low-humidity timer
    g_fake_millis        = 1UL * 60 * 1000;  // 1 min elapsed
    SensorData sd        = default_sensor();
    sd.bathHumidity      = 55.0f;  // below HUMIDITY_EXHAUST_OFF=60
    sd.bathHumidValid    = true;
    sd.roomTempValid     = true;
    sd.presenceDetected  = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    // Delay (2 min) not yet met — exhaust must still be ON
    TEST_ASSERT_TRUE(cs.exhaustOn);
}

// 10. Exhaust turns OFF after 2-min delay
void test_exhaust_off_after_delay() {
    g_fake_millis = 0;
    ControlState cs      = fresh_cs();
    cs.exhaustOn         = true;
    cs.exhaustLowHumidMs = 0;              // timer started at t=0
    g_fake_millis        = 3UL * 60 * 1000;  // 3 min — past the 2 min threshold
    SensorData sd        = default_sensor();
    sd.bathHumidity      = 55.0f;
    sd.bathHumidValid    = true;
    sd.roomTempValid     = true;
    sd.presenceDetected  = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_FALSE(cs.exhaustOn);
}

// 11. ALARM triggers when temp OOR for 35 s
void test_alarm_on_sensor_oor() {
    g_fake_millis = 0;
    ControlState cs      = fresh_cs();
    cs.sensorAlarmStartMs = 0;    // fault detected at t=0
    g_fake_millis         = 35000UL;   // 35 s — past 30 s threshold
    SensorData sd         = default_sensor();
    sd.roomTempValid = false;     // mark sensor as invalid
    sd.lastRoomTempMs = 0;        // stale timestamp (never updated)
    // Don't stamp fresh — lastRoomTempMs stays 0

    control_update(cs, sd);

    TEST_ASSERT_EQUAL(STATE_ALARM, cs.state);
    TEST_ASSERT_TRUE(cs.alarmActive);
}

// 12. Monsoon mode: room humidity > 75% → AC mode DRY
void test_monsoon_mode() {
    g_fake_millis = 1000;
    ControlState cs = fresh_cs();
    cs.state        = STATE_AC_RUNNING;
    cs.acOn         = true;
    cs.acSetpoint   = 24;
    cs.monsoonMode  = false;
    SensorData sd   = default_sensor();
    sd.roomHumidity  = 80.0f;    // above HUMIDITY_MONSOON=75
    sd.roomHumidValid = true;
    sd.roomTempValid  = true;
    sd.presenceDetected = true;
    sd.radarValid = false;
    // temp comfortable so we stay in AC_RUNNING
    sd.roomTemp = 24.5f;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_TRUE(cs.monsoonMode);
    TEST_ASSERT_EQUAL(AC_MODE_DRY, g_last_ac_mode);
}

// 13. Closer distance → higher fan speed (at same temperature)
void test_distance_affects_fan_speed() {
    // Test with distance=50 cm (near)
    g_fake_millis = 1000;
    ControlState cs_near = fresh_cs();
    SensorData sd_near   = default_sensor();
    sd_near.roomTemp          = 27.0f;
    sd_near.presenceDetected  = true;
    sd_near.radarValid        = true;
    sd_near.targetDistanceCm  = 50;    // very close
    stamp_fresh(sd_near);
    control_update(cs_near, sd_near);
    uint8_t duty_near = cs_near.fanDuty;

    // Test with distance=350 cm (far)
    g_fake_millis = 1000;
    ControlState cs_far = fresh_cs();
    SensorData sd_far   = default_sensor();
    sd_far.roomTemp          = 27.0f;
    sd_far.presenceDetected  = true;
    sd_far.radarValid        = true;
    sd_far.targetDistanceCm  = 350;   // further away
    stamp_fresh(sd_far);
    control_update(cs_far, sd_far);
    uint8_t duty_far = cs_far.fanDuty;

    TEST_ASSERT_GREATER_THAN(duty_far, duty_near);
}

// 14. reason string is not empty after any control_update
void test_reason_string_not_empty() {
    g_fake_millis = 1000;
    ControlState cs = fresh_cs();
    cs.reason[0]    = '\0';   // force empty
    SensorData sd   = default_sensor();
    sd.roomTemp        = 25.0f;
    sd.presenceDetected = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    TEST_ASSERT_TRUE(strlen(cs.reason) > 0);
}

// 15. IR override: control does NOT change state within 30 min
void test_ir_override_pauses_automation() {
    g_fake_millis = 0;
    ControlState cs      = fresh_cs();
    cs.irOverride        = true;
    cs.irOverrideStartMs = 0;    // override started at t=0
    SystemState stateBefore = cs.state;
    g_fake_millis        = 10UL * 60 * 1000;   // 10 min into override
    SensorData sd        = default_sensor();
    sd.roomTemp          = 30.5f;   // would normally trigger AC
    sd.presenceDetected  = true;
    stamp_fresh(sd);

    control_update(cs, sd);

    // Automation was skipped: AC must NOT have turned on
    TEST_ASSERT_FALSE(cs.acOn);
    // State must not have advanced to AC_RUNNING
    TEST_ASSERT_NOT_EQUAL(STATE_AC_RUNNING, cs.state);
    // irOverride flag must still be active (< 30 min elapsed)
    TEST_ASSERT_TRUE(cs.irOverride);
}

// ── Runner ───────────────────────────────────────────────────
int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_fan_starts_above_24c);
    RUN_TEST(test_ac_requires_10min_full_fan);
    RUN_TEST(test_ac_turns_on_after_10min);
    RUN_TEST(test_setpoint_creep);
    RUN_TEST(test_setpoint_max_28);
    RUN_TEST(test_empty_room_fan_off_at_5min);
    RUN_TEST(test_empty_room_ac_off_at_10min);
    RUN_TEST(test_exhaust_on_above_70_humidity);
    RUN_TEST(test_exhaust_off_below_60_with_delay);
    RUN_TEST(test_exhaust_off_after_delay);
    RUN_TEST(test_alarm_on_sensor_oor);
    RUN_TEST(test_monsoon_mode);
    RUN_TEST(test_distance_affects_fan_speed);
    RUN_TEST(test_reason_string_not_empty);
    RUN_TEST(test_ir_override_pauses_automation);

    return UNITY_END();
}
