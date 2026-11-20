// ============================================================
//  display.cpp — SSD1306 OLED renderer
//
//  Layout (128×64 px, font 6×8):
//  Row 0 (y=0):  "Vaayu Hub"            [title]
//  Row 1 (y=10): "T:25.3  H:62%"
//  Row 2 (y=20): "Fan:72%  AC:26°C"
//  Row 3 (y=30): "Exhaust:ON  PR:YES"
//  Row 4 (y=40): "Bath H:68%"
//  Row 5 (y=52): [scrolling reason string]
// ============================================================
#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Scroll state for the reason string
static int16_t  scrollX     = OLED_WIDTH;
static uint32_t lastScrollMs = 0;
static char     lastReason[80] = "";

void display_init() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[display] SSD1306 not found");
        return;
    }
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);
    oled.setTextSize(1);  // 6×8 px per char

    oled.setCursor(28, 24);
    oled.print("Vaayu Hub");
    oled.display();
    delay(1500);
    Serial.println("[display] OLED ready");
}

void display_update(const SensorData& sd, const ControlState& cs) {
    oled.clearDisplay();

    // ── Title ────────────────────────────────────────────────
    oled.setTextSize(1);
    oled.setCursor(28, 0);
    oled.print("Vaayu Hub");

    // ── Divider ──────────────────────────────────────────────
    oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // ── Row 1: Temperature + Humidity ────────────────────────
    oled.setCursor(0, 11);
    oled.printf("T:");
    if (sd.roomTempValid)
        oled.printf("%.1f\xF8""C", sd.roomTemp);   // \xF8 = ° in cp437
    else
        oled.print("--.-");

    oled.setCursor(70, 11);
    oled.printf("H:");
    if (sd.roomHumidValid)
        oled.printf("%.0f%%", sd.roomHumidity);
    else
        oled.print("--");

    // ── Row 2: Fan + AC setpoint ─────────────────────────────
    oled.setCursor(0, 21);
    oled.printf("Fan:%d%%", (int)(cs.fanDuty * 100 / 255));

    oled.setCursor(66, 21);
    if (cs.acOn)
        oled.printf("AC:%d\xF8""C", cs.acSetpoint);
    else
        oled.print("AC:OFF");

    // ── Row 3: Exhaust + Presence ────────────────────────────
    oled.setCursor(0, 31);
    oled.printf("Exh:%s", cs.exhaustOn ? "ON " : "OFF");
    oled.setCursor(60, 31);
    oled.printf("Pr:%s", sd.presenceDetected ? "YES" : "NO ");

    // ── Row 4: Bathroom humidity ─────────────────────────────
    oled.setCursor(0, 41);
    if (sd.bathHumidValid)
        oled.printf("Bath H:%.0f%%", sd.bathHumidity);
    else
        oled.print("Bath H:--");

    // Monsoon / override indicator
    if (cs.monsoonMode) {
        oled.setCursor(72, 41);
        oled.print("MNSN");
    }
    if (cs.irOverride) {
        oled.setCursor(96, 41);
        oled.print("OVR");
    }

    // ── Row 5: Scrolling reason string ───────────────────────
    oled.drawFastHLine(0, 50, 128, SSD1306_WHITE);

    // Reset scroll position when reason changes
    if (strcmp(cs.reason, lastReason) != 0) {
        strncpy(lastReason, cs.reason, sizeof(lastReason) - 1);
        scrollX = OLED_WIDTH;
    }

    // Scroll at ~30 px/s (called every ~1 s, so advance 2 px)
    uint32_t now = millis();
    if (now - lastScrollMs >= 50) {
        lastScrollMs = now;
        scrollX -= 2;
        int16_t textWidth = strlen(cs.reason) * 6;  // 6 px per char at size 1
        if (scrollX < -textWidth) scrollX = OLED_WIDTH;
    }

    oled.setCursor(scrollX, 54);
    oled.print(cs.reason);

    // ── Alarm overlay ────────────────────────────────────────
    if (cs.alarmActive) {
        oled.fillRect(0, 50, 128, 14, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(8, 54);
        oled.print("!! SENSOR ALARM !!");
        oled.setTextColor(SSD1306_WHITE);
    }

    oled.display();
}
