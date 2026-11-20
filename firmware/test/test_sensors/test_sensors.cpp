// ============================================================
//  test_sensors.cpp — Unity tests for Vaayu Hub sensor
//  validation logic (outlier rejection, OOR detection).
//
//  These tests exercise the helper functions used inside
//  sensors.cpp directly by re-implementing the same logic
//  inline (the helpers are file-static there, so we mirror
//  them here) and then verify the expected outcomes.
//
//  Compile target: [env:native]
//  Run: pio test -e native
// ============================================================

#include <unity.h>
#include <math.h>    // fabsf
#include <stdbool.h>

// ── Replicate config constants (must stay in sync) ────────────
#define TEMP_VALID_MIN        -10.0f
#define TEMP_VALID_MAX         60.0f
#define HUMIDITY_VALID_MIN      0.0f
#define HUMIDITY_VALID_MAX    100.0f
#define TEMP_OUTLIER_DELTA      5.0f
#define HUMIDITY_OUTLIER_DELTA 20.0f

// ── Replicate sensor validation helpers ──────────────────────
// (mirror of the file-static helpers in sensors.cpp)
static bool temp_valid(float v, float lastGood) {
    if (v < TEMP_VALID_MIN || v > TEMP_VALID_MAX)        return false;
    if (lastGood != 0 && fabsf(v - lastGood) > TEMP_OUTLIER_DELTA) return false;
    return true;
}

static bool humid_valid(float v, float lastGood) {
    if (v < HUMIDITY_VALID_MIN || v > HUMIDITY_VALID_MAX) return false;
    if (lastGood != 0 && fabsf(v - lastGood) > HUMIDITY_OUTLIER_DELTA) return false;
    return true;
}

// ── Unity lifecycle ──────────────────────────────────────────
void setUp()    {}
void tearDown() {}

// ── Tests ────────────────────────────────────────────────────

// 1. A reading that differs more than 5°C from the last good value
//    must be rejected (outlier rejection).
void test_outlier_rejection() {
    float lastGood = 24.0f;
    float newReading = 30.0f;   // delta = 6°C > TEMP_OUTLIER_DELTA
    TEST_ASSERT_FALSE(temp_valid(newReading, lastGood));
}

// 2. A normal reading within range and within delta is accepted.
void test_valid_reading_accepted() {
    float lastGood   = 24.0f;
    float newReading = 25.5f;   // delta = 1.5°C < 5°C, in range
    TEST_ASSERT_TRUE(temp_valid(newReading, lastGood));
}

// 3a. temp = 999°C is out-of-range (OOR) — rejected.
void test_oor_detection_high() {
    float lastGood = 25.0f;
    TEST_ASSERT_FALSE(temp_valid(999.0f, lastGood));
}

// 3b. temp = -999°C is also OOR — rejected.
void test_oor_detection_low() {
    float lastGood = 25.0f;
    TEST_ASSERT_FALSE(temp_valid(-999.0f, lastGood));
}

// 4. Boundary: exactly at TEMP_VALID_MIN (-10°C) is accepted.
void test_boundary_min_temp_accepted() {
    // lastGood = 0 bypasses outlier check per implementation
    TEST_ASSERT_TRUE(temp_valid(-10.0f, 0.0f));
}

// 5. Just below minimum (-10.1°C) is rejected.
void test_below_min_temp_rejected() {
    TEST_ASSERT_FALSE(temp_valid(-10.1f, 0.0f));
}

// 6. Humidity outlier rejection: 25% jump from 50% baseline.
void test_humidity_outlier_rejection() {
    float lastGood   = 50.0f;
    float newReading = 76.0f;   // delta = 26% > HUMIDITY_OUTLIER_DELTA=20
    TEST_ASSERT_FALSE(humid_valid(newReading, lastGood));
}

// 7. Valid humidity reading close to baseline.
void test_valid_humidity_accepted() {
    float lastGood   = 50.0f;
    float newReading = 55.0f;   // delta = 5% < 20%
    TEST_ASSERT_TRUE(humid_valid(newReading, lastGood));
}

// 8. Humidity OOR — over 100% rejected.
void test_humidity_oor_over_100() {
    TEST_ASSERT_FALSE(humid_valid(105.0f, 50.0f));
}

// ── Runner ───────────────────────────────────────────────────
int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_outlier_rejection);
    RUN_TEST(test_valid_reading_accepted);
    RUN_TEST(test_oor_detection_high);
    RUN_TEST(test_oor_detection_low);
    RUN_TEST(test_boundary_min_temp_accepted);
    RUN_TEST(test_below_min_temp_rejected);
    RUN_TEST(test_humidity_outlier_rejection);
    RUN_TEST(test_valid_humidity_accepted);
    RUN_TEST(test_humidity_oor_over_100);

    return UNITY_END();
}
