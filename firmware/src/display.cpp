// ============================================================
//  display.cpp — SSD1306 OLED renderer
//  Layout conforming to Section 6 & 7 of VAAYU_CONTEXT.md
// ============================================================
#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Scroll state for the reason string
static int16_t  scrollX      = OLED_WIDTH;
static uint32_t lastScrollMs = 0;
static char     lastReason[96] = "";

void display_init() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[display] SSD1306 not found at 0x3C");
        return;
    }
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);
    oled.setTextSize(1);

    oled.setCursor(20, 24);
    oled.print("VAAYU HUB v3");
    oled.display();
    delay(1000);
    Serial.println("[display] OLED ready");
}

void display_update(const SensorData& sd, const ControlState& cs) {
    oled.clearDisplay();

    // ── Header / Title ───────────────────────────────────────
    oled.setTextSize(1);
    if (cs.fastTestMode) {
        oled.setCursor(4, 0);
        oled.print("* FAST TEST (5s) *");
    } else {
        oled.setCursor(18, 0);
        const char* pNames[] = { "NORM", "SLEEP", "HUMID", "ECO" };
        oled.printf("VAAYU HUB [%s]", pNames[cs.profile]);
    }
    oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // ── Row 1: Room Temp + Humidity ──────────────────────────
    oled.setCursor(0, 11);
    if (sd.roomTempValid) oled.printf("T:%.1f\xF8""C", sd.roomTemp);
    else                  oled.print("T:--.-");

    oled.setCursor(68, 11);
    if (sd.roomHumidValid) oled.printf("H:%.0f%%", sd.roomHumidity);
    else                   oled.print("H:--");

    // ── Row 2: Fan Level + AC Setpoint ───────────────────────
    oled.setCursor(0, 21);
    oled.printf("Fan:L%d (%d%%)", cs.fanLevel, (int)(FAN_DUTIES[cs.fanLevel] * 100 / 255));

    oled.setCursor(76, 21);
    if (cs.acOn) oled.printf("AC:%d\xF8""C", cs.acSetpoint);
    else         oled.print("AC:OFF");

    // ── Row 3: Comfort Index + Target ────────────────────────
    oled.setCursor(0, 31);
    oled.printf("CI:%.1f\xF8 (Tgt:%.1f\xF8)", cs.currentCI, cs.targetCI);

    // ── Row 4: Presence + Bathroom Exhaust ───────────────────
    oled.setCursor(0, 41);
    oled.printf("P:%s", sd.presenceDetected ? "YES" : "NO ");
    if (sd.presenceDetected && sd.targetDistanceCm > 0) {
        oled.printf("%dcm", sd.targetDistanceCm);
    }

    oled.setCursor(76, 41);
    oled.printf("Exh:%s", cs.exhaustOn ? "ON " : "OFF");

    // ── Row 5: Scrolling reason string ───────────────────────
    oled.drawFastHLine(0, 50, 128, SSD1306_WHITE);

    if (strcmp(cs.reason, lastReason) != 0) {
        strncpy(lastReason, cs.reason, sizeof(lastReason) - 1);
        scrollX = OLED_WIDTH;
    }

    uint32_t now = millis();
    if (now - lastScrollMs >= 50) {
        lastScrollMs = now;
        scrollX -= 2;
        int16_t textWidth = strlen(cs.reason) * 6;
        if (scrollX < -textWidth) scrollX = OLED_WIDTH;
    }

    oled.setCursor(scrollX, 54);
    oled.print(cs.reason);

    // ── Failsafe / Alarm Overlay ─────────────────────────────
    if (cs.acNotResponding) {
        oled.fillRect(0, 50, 128, 14, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(4, 54);
        oled.print("! AC NOT RESPONDING !");
        oled.setTextColor(SSD1306_WHITE);
    } else if (cs.sensorFaultHold) {
        oled.fillRect(0, 50, 128, 14, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(8, 54);
        oled.print("! SENSOR FAULT !");
        oled.setTextColor(SSD1306_WHITE);
    }

    oled.display();
}
