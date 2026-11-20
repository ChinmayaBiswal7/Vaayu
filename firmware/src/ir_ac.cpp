// ============================================================
//  ir_ac.cpp — IR send (Daikin) + human override detection
//
//  Hardware:
//    PIN_IR_SEND  → NPN base resistor → collector → IR LED anode
//                   emitter → GND; LED cathode through 10 Ω → GND
//    PIN_IR_RECV  → TSOP1838 (or similar) signal out
//
//  To swap to a different AC brand:
//    1. Find your protocol in IRremoteESP8266 src/ir_Protocols.md
//    2. Include its IRac or IRsend subclass header
//    3. Replace the sendDaikin() block in ir_send_ac_command()
// ============================================================
#include "ir_ac.h"
#include "config.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ir_Daikin.h>   // Daikin protocol encoder

// ── Static objects ───────────────────────────────────────────
static IRsend    irSend(PIN_IR_SEND);
static IRrecv    irRecv(PIN_IR_RECV, 1024, 15, true);
static decode_results irResults;

// Track our own transmissions so we don't confuse them with human presses
static volatile bool  weJustSent    = false;
static volatile unsigned long sentAt = 0;
// Ignore receiver for this many ms after we transmit (longest Daikin frame ~200 ms)
static const unsigned long TX_BLANKING_MS = 400;

// ── Initialisation ───────────────────────────────────────────
void ir_init() {
    irSend.begin();
    irRecv.enableIRIn();
    Serial.println("[ir] IR send/recv initialised (Daikin)");
}

// ── Send IR command ──────────────────────────────────────────
void ir_send_ac_command(AcMode mode, int tempC, AcFanSpeed fanSpeed) {
    tempC = constrain(tempC, 16, 30);

    IRDaikinESP daikin(PIN_IR_SEND);

    // --- Map AcMode ---
    switch (mode) {
    case AC_MODE_COOL:
        daikin.setMode(kDaikinCool);
        daikin.on();
        break;
    case AC_MODE_DRY:
        daikin.setMode(kDaikinDry);
        daikin.on();
        break;
    case AC_MODE_FAN:
        daikin.setMode(kDaikinFan);
        daikin.on();
        break;
    case AC_MODE_AUTO:
        daikin.setMode(kDaikinAuto);
        daikin.on();
        break;
    case AC_MODE_OFF:
        daikin.off();
        break;
    }

    daikin.setTemp(tempC);

    // --- Map fan speed ---
    switch (fanSpeed) {
    case AC_FAN_AUTO:   daikin.setFan(kDaikinFanAuto);   break;
    case AC_FAN_LOW:    daikin.setFan(kDaikinFanMin);    break;
    case AC_FAN_MEDIUM: daikin.setFan(kDaikinFanMed);    break;
    case AC_FAN_HIGH:   daikin.setFan(kDaikinFanMax);    break;
    }

    weJustSent = true;
    sentAt     = millis();

    daikin.send();

    Serial.printf("[ir] Sent Daikin: mode=%d temp=%d°C fan=%d\n",
                  (int)mode, tempC, (int)fanSpeed);
}

// ── Human override detection ─────────────────────────────────
bool ir_check_human_override() {
    if (!irRecv.decode(&irResults)) return false;
    irRecv.resume();

    // If we just transmitted, ignore echoes from our own LED
    if (weJustSent && (millis() - sentAt) < TX_BLANKING_MS) {
        return false;
    }
    weJustSent = false;

    // Any valid IR signal we didn't send → human pressed remote
    Serial.printf("[ir] Human IR detected: protocol=%s, bits=%d\n",
                  typeToString(irResults.decode_type, false).c_str(),
                  irResults.bits);
    return true;
}
