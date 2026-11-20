# Vaayu Hub: project context for building the Round 3 prototype

Give this whole file to your coding agent before starting. It holds every decision made so far. Where it gives an exact number or rule, use it as written; those numbers already appear in the team's submitted abstract and must match.

---

## 1. The situation in one paragraph

Team **Vaayu** (KIIT Bhubaneswar) is in **atomQUEST 2026**, a product challenge run by Atomberg. We are in **Track 1 (Fan)**. We passed Round 1 (written abstract) and Round 2 (video of a 3D software simulation). **Round 3 needs a working hardware prototype** and closes on **26 November 2026, 11:59 PM IST**. No hardware exists yet and no firmware has been written. Parts are being ordered around 20 November. The job is to build the prototype, write the firmware, run the measurements, film a 2-minute video and write a 4-page report.

**Team:** Manish Sarkar (team leader), Chinmaya Biswal, Mishti Ghosh, Kavya Agarwal.

---

## 2. What Round 3 requires (from the official brief)

- **Same track and area as the abstract**, or the entry is disqualified. Our areas: *Fan as an enabler for smart home* (primary) and *sensor-enabled exhaust fans with auto ON/OFF* (secondary).
- **Hardware focus.** "The prototype must work. A concept, rendering or partly built model will not be enough."
- **Video: max 2 minutes, max 50 MB.** It must show:
  1. Team details
  2. The problem and how the prototype solves it
  3. The complete prototype in operation, doing its core function as claimed in the abstract
  4. **A live test of the fail-safe, triggered on camera**
  5. **The measured result against the target claim, shown on camera where possible**
- **Report: PDF, max 4 pages.** It must cover:
  1. Team name, college, track and area
  2. Problem, solution and how it works
  3. Test results against the target claim, and how they were measured
  4. The fail-safe: how it works and how it was tested
  5. Cost: parts list, prototype cost, estimated cost per unit at scale
  6. Power consumption
  7. Key design decisions, what failed, what changed
  8. Reliability observations and known limitations
- **Changes since Round 2 must be listed in the report** (see section 9).
- **File names, exactly:**
  - `atomQUEST2026_Vaayu_KIIT_Track1_R3_Video.mp4`
  - `atomQUEST2026_Vaayu_KIIT_Track1_R3_Report.pdf`
- Upload both on the GradPartners platform. One video and one report per team.
- **The report is checked against the video.** Every claim in the report must be visible in the video or backed by logged data.

---

## 3. What we are building

**Vaayu Hub** is a small low-voltage box that sits by the fan and makes the **fan, AC and bathroom exhaust work as one system**. The idea is "cool people, not rooms": use moving air first, run the AC only when the fan alone cannot keep the person comfortable, then raise the AC's set point step by step while the fan makes up the difference. That last step is where the energy saving comes from.

### Features, in priority order
1. **Fan first.** When it gets warm, the fan speeds up. The AC comes on only after the fan has been at maximum for 10 minutes and it is still warm, starting at 26 °C.
2. **AC set point creeps up.** While the person stays comfortable, the AC setting rises 1 °C at a time to a 28 °C cap.
3. **Empty-room shut-off.** The mmWave radar sees people even when they sit or lie still, which a normal PIR motion sensor cannot. Nobody there for 5 minutes → fan off. For 10 minutes → AC off. When someone returns, the previous state is restored, respecting the AC's restart lockout.
4. **Exhaust from bathroom humidity.** A humidity sensor in the bathroom switches the exhaust fan on when humidity jumps and off when it recovers. It is wired to the hub for this prototype; wireless is future work.
5. **Fail-safes**, see section 6.
6. **Extras, only if the core is finished:** monsoon mode (AC to Dry in humid, mild weather). Distance-aware fan speed is **dropped** for Round 3, because we have no anemometer to calibrate it; list it as future work.

### What is NOT in this prototype (on purpose)
- No mains (230 V) wiring anywhere. The AC is controlled only through its normal IR remote protocol. Nobody opens or rewires a 230 V device.
- No camera and no microphone. Radar gives presence only.
- No cloud. Everything runs locally on the ESP32.

---

## 4. Final parts list (about ₹1,890)

| # | Part | Qty | Approx. ₹ | Job |
|---|---|---|---|---|
| 1 | ESP32 DevKit V1 (ESP-WROOM-32) | 1 | 420 | Hub brain, Wi-Fi, phone dashboard |
| 2 | HLK-LD2410C mmWave radar | 1 | 380 | Presence, including people sitting still |
| 3 | 0.96" OLED, I2C, SSD1306, address 0x3C | 1 | 180 | Readings and the reason for each action, on the device |
| 4 | AHT20 temperature/humidity module (I2C) | 1 | 150 | Room temperature and humidity |
| 5 | DS18B20 waterproof probe | 1 | 120 | Taped at the AC's air outlet; confirms the AC really turned on |
| 6 | DHT11 sensor | 1 | 80 | Bathroom humidity, on a 2–3 m wire |
| 7 | 940 nm IR LEDs | 3 | 15 | Send commands to the AC |
| 8 | BC547 transistors | 2 | 10 | Drive the IR LED |
| 9 | Resistors: 100 Ω, 1 kΩ, 4.7 kΩ, 10 kΩ | set | 35 | |
| 10 | VS1838B IR receiver (38 kHz) | 1 | 30 | Learns the AC remote; hears manual presses |
| 11 | L9110S dual motor driver | 1 | 60 | Speed control for both fans |
| 12 | **5 V DC** fans, 80 mm | 2 | 240 | Ceiling-fan stand-in and exhaust fan |
| 13 | Buzzer | 1 | 20 | Audible fail-safe alarm |
| 14 | Breadboard + jumper wires | set | 150 | |

Also needed: a phone charger (5 V, 2 A or more), a USB cable for the ESP32, and an **energy meter** for the AC (borrowed from the electrical department, or the building's digital electricity meter with the AC as the only big load; buy a 16 A plug-in meter for about ₹800 only if neither works). **Optional backup:** HC-SR501 PIR (₹60) in case the radar arrives late.

**Note on the IR LED:** a BC547 handles at most 100 mA. Use a 68–100 Ω series resistor, about 50 mA. Expect 2–3 m range, so place the hub fairly close to the AC.

---

## 5. Wiring (ESP32 DevKit V1)

| Part | ESP32 pin | Notes |
|---|---|---|
| OLED + AHT20 (shared I2C bus) | SDA **21**, SCL **22** | Both at 3.3 V |
| Bathroom DHT11 | **4** | 10 kΩ pull-up to 3.3 V if the module has none; long wire is fine |
| DS18B20 | **27** | 4.7 kΩ resistor from data to 3.3 V |
| LD2410C radar | radar TX → **16** (ESP RX2), radar RX → **17** (ESP TX2) | Radar powered from 5 V; its UART is 3.3 V level. Default baud 256000. |
| IR LED | **25** → 1 kΩ → BC547 base | LED from 5 V through 100 Ω to collector; emitter to GND |
| IR receiver (VS1838B) | **26** | 3.3 V supply |
| Main fan | L9110S **A-IA → 32** (PWM speed) | Tie A-IB to GND |
| Exhaust fan | L9110S **B-IA → 14** (PWM speed) | Tie B-IB to GND |
| Buzzer | **19** | |
| Power | USB charger → ESP32 USB | ESP32 **VIN/5V** pin → radar 5 V and L9110S VCC. **All grounds connected together.** |

Avoid ESP32 strapping pins (0, 2, 5, 12, 15) for new connections. Pin 34–39 are input-only. If the PIR backup is used, put it on **34**.

---

## 6. Control rules (exact; these match the abstract)

The **reference implementation** of all these rules is the team's 3D simulator, `vaayu-hub-simulator.html` (ask Manish for the file). Its `control()`, `checkPlug()` and `exhaustRules()` JavaScript functions were tested and are the source of truth. Port them to C++, replacing the simulated physics with real sensor readings. Anything not covered below should behave like the simulator.

### Comfort index
`CI = T + 0.05 × (RH − 50) − credit(v)`, where `credit(v) = 2.5 × min(v / 0.8, 1.2)` °C and `v` is the air speed at the person for the current fan level.

Without an anemometer, use a fixed estimate per fan level at about 1.5 m: level 0–5 → **0, 0.30, 0.55, 0.80, 1.05, 1.35 m/s**. State in the report that these are estimates, not measurements.

### Comfort profiles
| Profile | Target CI | Max fan level | AC start | AC cap | Monsoon mode |
|---|---|---|---|---|---|
| Normal (default) | 25.5 °C | 5 | 26 °C | 28 °C | off |
| Sleep | 25.5 °C | 3 | 26 °C | 28 °C | off |
| Humid | 25.5 °C | 5 | 26 °C | 28 °C | **on** |
| Eco | 26.0 °C | 5 | 26 °C | 29 °C | off |

### Every control tick (once per minute)
1. **Sensor fault check first.** If the room reading is invalid (NaN, out of range, read failure, or the exact same value for over 10 minutes): log it, beep, show it on the OLED, and **stop sending any commands; hold the last state**. Resume automatically once readings are valid.
2. **AC command check.** See below.
3. **Occupancy.** Count consecutive minutes with no presence. At 3 → log "room confirmed empty". At 5 → save fan level, fan off. At 10 → save AC state, AC off. When presence returns → restore the fan immediately; restore the AC only once the restart rules allow it.
4. **Manual-override pause.** If a pause is active, do nothing until it ends.
5. **Fan-first ladder.** If CI > target + 0.5 and fan < max → fan up one level. Else if fan > 0 and CI at one level lower would be ≤ target + 0.2 → fan down one level.
6. **Monsoon mode** (Humid profile only). If RH > 70 % and 26 °C ≤ T ≤ 30 °C → AC on in Dry. If in Dry and (T > 30 °C or RH < 60 %) → back to Cool.
7. **AC on.** If the AC is off, count minutes where fan = max and CI > target + 0.5. At 10 → AC on in Cool at the profile's start temperature.
8. **Set-point creep.** If the AC is on in Cool, count minutes where CI ≤ target + 0.5. At 15 → set point +1 °C, up to the cap, and reset the count.
9. **Give some back.** If fan = max and CI > target + 1 for 10 minutes → set point −1 °C, not below the start temperature.
10. **AC off when outside is cooler.** Optional. The simulator used outdoor temperature, which this hardware does not measure, so skip it or use a fixed rule.

### AC command rules (safety)
- At most **one AC command every 10 minutes**.
- At least **3 minutes between AC off and AC on**, to protect the compressor.
- Send commands with the IRremoteESP8266 `IRac` class, using the protocol learned from the real remote (section 7).

### AC command check (the DS18B20 fail-safe)
IR is one-way, so after switching the AC **on or off**, wait 3 minutes and read the DS18B20 at the AC's air outlet:
- **After ON:** confirmed if outlet temperature ≤ room temperature − 3 °C (− 2 °C in Dry).
- **After OFF:** confirmed if the outlet has warmed back to within about 2 °C of room temperature.
- If not confirmed → **resend once** and wait 3 more minutes. Still not confirmed → **alarm**: buzzer pattern, OLED shows "AC NOT RESPONDING", dashboard flag.
- If the DS18B20 reads −127 (disconnected) → skip the check, log it.

### Exhaust rules (bathroom DHT11)
- Keep a 30-minute rolling baseline of bathroom humidity, as an EMA with a 30-minute time constant, updated only while the exhaust is off.
- **ON** if humidity rises ≥ 8 % above baseline, or exceeds 75 % while ≥ 3 % above baseline.
- **OFF** once within 3 % of baseline for 2 minutes in a row.
- **Hard limit:** off after 45 minutes of running, with a log line.
- If the bathroom sensor stops responding for 10 minutes → exhaust goes to manual (dashboard button), with a log line.

### Manual override
The VS1838B receiver listens all the time, except during the hub's own transmissions plus about 600 ms after. If it decodes the **same AC protocol** as the learned remote, someone used the normal remote:
- Adopt the decoded state (power, mode, temperature) as the current AC state.
- **Pause automation for 30 minutes.**
- Shift the comfort target by **0.15 °C**: lower if the person made it cooler, higher if warmer.
- Log it.

### Logging
Every automatic action writes a short plain-English reason, for example "Fan at max for 10 min and still warm, so AC on at 26 °C". It appears on the OLED (last line) and in the dashboard's "why it acted" list.

---

## 7. Firmware plan

**Toolchain:**
- Arduino IDE or arduino-cli, **esp32 board package 3.x** (tested environment had 3.3.11). Board: "ESP32 Dev Module".
- **Libraries:**
  - IRremoteESP8266 **2.9.0** (2.8.x does not build on esp32 core 3.x)
  - Adafruit AHTX0, Adafruit SSD1306, Adafruit GFX
  - OneWire, DallasTemperature
  - DHT sensor library (Adafruit) + Adafruit Unified Sensor
  - ld2410 (by ncmreynolds)

**Structure (one sketch, several tabs):**
- `config.h`: all pins, thresholds, profiles, Wi-Fi name and password.
- `sensors`: AHT20 every 2 s with validity checks; DHT11 every 2 s; DS18B20 (non-blocking conversion); radar `read()` every loop.
- `control`: the per-minute rules from section 6, plus the per-second exhaust rules.
- `ir`: learn mode, sending through `IRac`, override detection.
- `display`: OLED refresh every 500 ms.
- `web`: dashboard and JSON API.
- Hardware watchdog with a 10 s timeout.

**Time base and fast-test mode.** All rules use a minute counter. Add a **fast-test mode** where 1 minute = 5 seconds, so 10-minute rules can be shown in under a minute on camera. Show "FAST TEST 1 min = 5 s" on the OLED and dashboard whenever it is on. Use normal speed for all energy measurements.

**Fans.** L9110S inputs driven by LEDC PWM, about 20–25 kHz to avoid audible whine. Level → duty: **0, 35, 50, 65, 80, 100 %**. Small 5 V fans may not start below about 35 %, so tune the lowest level.

**Learn AC remote.** A dashboard button arms learn mode. Press any button on the real AC remote, then use `IRAcUtils::decodeToState()` and check `IRac::isProtocolSupported()`. Store the protocol and model in NVS with Preferences. If the AC isn't supported by `IRac`, record raw codes for ON-26, ON-27, ON-28, Dry and OFF, and replay those instead.

**Wi-Fi.** The ESP32 runs its own access point (SSID `Vaayu-Hub`, password at least 8 characters), so it needs no router or internet. A phone connects and opens `192.168.4.1`.

**Dashboard** (served by the ESP32):
- Live readings: room T and RH, bathroom RH, outlet temperature, radar state, fan level, AC state, exhaust state, comfort index vs target.
- The last 8 reasons.
- Buttons: profile, auto/manual, manual fan level, manual AC on/off and set point, manual exhaust, learn AC remote, fast-test toggle, resume automation.
- **Test logging:** start/stop a labelled session ("Baseline" or "Vaayu"), log one row per minute, enter the energy meter's kWh reading at start and end, and download everything as CSV.

**Simple style.** Black background, monospace numbers, matching the simulator's look. Keep it one HTML page stored as a string in the firmware.

---

## 8. Tests to run and film

| Test | How | What to record |
|---|---|---|
| **AC energy (the main claim)** | Same room, similar afternoons, same duration (about 2 hours each). Session A: AC at 24 °C, fan off. Session B: Vaayu Hub in control. | kWh from the energy meter at start and end, room T and RH log, comfort votes. **Target: ≥ 15 % less AC energy at the same comfort.** |
| Sitting still | Sit or lie still under the radar for 60 minutes | Number of wrong shut-offs. Target: 0. |
| Empty room | Leave the room | Time to fan off (5 min) and AC off (10 min) |
| Steam / exhaust | Hold the bathroom sensor over hot water or breathe on it | Time from humidity jump to exhaust on (target < 60 s), and off time |
| Fail-safe: bad sensor | Unplug the AHT20 on camera | Hub holds state, beeps, OLED shows the fault |
| Fail-safe: AC not responding | Cover the IR LED with a finger on camera and trigger an AC command | Resend, then alarm |
| Manual override | Press the real AC remote | Automation pauses 30 min, target shifts |
| Power draw | USB power meter, or the charger rating plus estimate | Hub power in watts (target < 1 W for the hub itself, excluding fans) |

**Weather warning:** late November in Bhubaneswar can be too mild to need the AC. Run the energy test in the warmest afternoon hours with doors and windows shut. If the numbers are inconclusive, report the real numbers and explain why. **Never invent or round results up.**

---

## 9. Changes since the abstract and Round 2 (must go in the report)

| Abstract said | Prototype has | Reason |
|---|---|---|
| ESP32-S3 | ESP32 DevKit V1 (WROOM-32) | Cheaper, easier to buy quickly |
| SHT41 sensors | AHT20 (room), DHT11 (bathroom) | Cost and local availability; DHT11 is less accurate |
| Wireless ESP32-C3 bathroom node | DHT11 wired to the hub | Cost and time; wireless is future work |
| Smart plug reads AC power to confirm commands | DS18B20 at the AC outlet confirms the air turns cold | Most Indian smart plugs are hard to read locally; this is cheap and low voltage |
| Smart plug switches the exhaust | 5 V demo fan through the L9110S | Keeps the prototype off mains; a real install would use a certified smart plug |
| 12 V PWM fan as the ceiling-fan stand-in | 5 V 80 mm fan | Runs from one phone charger, no adapter |
| Distance-aware fan speed | Dropped | No anemometer to calibrate it; future work |
| Round 2 delay (no hardware) | Hardware built for Round 3 | State plainly what was done between rounds |

Target claims stay the same: ≥ 15 % lower AC energy at the same comfort, 0 false switch-offs in a 60-minute still test, exhaust on within 60 s.

---

## 10. Timeline (deadline 26 Nov, 11:59 PM IST)

| Dates | Work |
|---|---|
| 20 Nov | Order the radar with the fastest delivery; buy everything else locally |
| 20–21 Nov | Write the firmware before parts arrive; compile it |
| 21–22 Nov | Bring up each part on the breadboard; learn the real AC remote |
| 22–23 Nov | Full integration; fail-safe tests; enclosure or neat mounting |
| 23–24 Nov | Measurements: AC energy sessions, sit-still test, steam test |
| 25 Nov | Film the 2-minute video |
| 26 Nov | Write the 4-page report; name files correctly; upload; check both open |

**Bring-up order** (test each alone before combining): OLED → AHT20 → DS18B20 → DHT11 → fans through the L9110S → IR receiver (print decoded codes from the real AC remote) → IR send (does the AC react?) → radar (print presence and distance) → full control loop in fast-test mode → dashboard.

---

## 11. Ground rules

- **Only report what we actually measured.** The 3D simulator explains the idea and is not evidence. The simulator's "31 % saving" is a modelled number and must never be presented as a result.
- If a target is missed, report the real number and what we learned.
- Keep the video and report consistent; judges compare them.
- Safety: no mains wiring, no opening the AC, no modified 230 V appliances.

---

## 12. Files the team already has

| File | What it is |
|---|---|
| `Vaayu_Track1.pdf` | Round 1 abstract (the claims we are judged against) |
| `atomQUEST2026_Vaayu_KIIT_Track1_Report.pdf` | Round 2 progress report |
| `atomQUEST2026_Vaayu_KIIT_Track1_Video.mp4` | Round 2 video (simulation) |
| `vaayu-hub-simulator.html` | 3D simulator; **the reference implementation of the control rules** |
| `r3.pdf` | The official Round 3 brief |
| Team brief page | https://claude.ai/artifact/X92D8T6iA21mHaShbpPLKV |
