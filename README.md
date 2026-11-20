# Vaayu Hub

> **Intelligent Room Climate Optimization Ecosystem**  
> AtomQuest 2026 — Team Vaayu (KIIT)

Vaayu Hub synchronizes ceiling fans, split air conditioners, and ventilation systems into a single energy-efficient cooling engine. Instead of chilling empty volumes of air, Vaayu focuses cooling on humans in the room, prioritizing high airflow before invoking power-hungry refrigeration cycles.

---

## Key System Behaviors

1. **Airflow Priority (Fan First)**  
   When ambient temperatures exceed 24 °C, fan velocity increases proportionally. The air conditioner engages only if the fan has operated at 100% capacity for at least 10 minutes and temperature remains at or above 26 °C.

2. **Thermodynamic Setpoint Drift**  
   While the occupied space maintains comfort (<= 25 °C), the AC temperature target automatically climbs by 1 °C every 15 minutes up to 28 °C. Increased fan speed offsets the sensation of warmth via convective cooling, delivering major electrical power savings.

3. **Autonomous Vacancy Isolation**  
   An onboard 24 GHz mmWave radar detects human presence even when stationary or sleeping. Upon vacancy, the fan disengages after 5 minutes and the AC shuts down after 10 minutes.

4. **Moisture & Exhaust Management**  
   Relative humidity above 70% activates ventilation. AC operation automatically switches to Dry (dehumidification) mode during monsoon conditions (> 75% RH).

5. **Human Override Respect**  
   An onboard infrared receiver intercepts signals from standard AC remotes. Any manual adjustment suspends automated control loops for 30 minutes.

---

## Architectural Layout

```
                   +------------------------+
                   |  ESP32-S3 Microhub     |
                   |  (Vaayu Core Brain)    |
                   +-----------+------------+
                               |
         +---------------------+---------------------+
         |                     |                     |
         v                     v                     v
+------------------+  +-----------------+  +-------------------+
|  Sensor Array    |  |  Actuators      |  |  Connectivity     |
|  - LD2410C Radar |  |  - IR Emitter   |  |  - Wi-Fi (MQTT)   |
|  - SHT41 Temp/RH |  |  - L298N PWM    |  |  - WebSocket:9001 |
+------------------+  +-----------------+  +---------+---------+
                                                     |
                                                     v
                                           +-------------------+
                                           | Mobile Dashboard  |
                                           | (React Native)    |
                                           +-------------------+
```

---

## Repository Structure

```
.
├── firmware/              # ESP32-S3 source code (PlatformIO / FreeRTOS)
│   ├── src/               # State machine, sensor drivers, IR protocols
│   ├── test/              # Unity unit tests
│   └── platformio.ini     # Build parameters and library dependencies
├── dashboard/             # Cross-platform mobile app (React Native / Expo)
│   ├── src/               # Live gauges, context providers, control screens
│   └── package.json       # App manifests and dependencies
├── testing/               # Simulation framework for hardware-free evaluation
│   ├── broker.py          # Standalone Python MQTT/WebSocket server
│   ├── simulate.py        # 30-minute state transition scenario runner
│   └── START_TESTING.bat  # Multi-process test harness launcher
└── RUN_TESTS.bat          # Master 1-click test launcher
```

---

## Quick Start (Hardware-Free Simulation)

Experience the full control loop without connecting any physical microcontrollers:

1. Double-click `RUN_TESTS.bat` in the project root.
2. In the terminal session that opens, press `w` to open the live web dashboard at `http://localhost:8081`.
3. Observe live simulated presence, temperature transitions, fan PWM modulation, and AC setpoint adjustments.

---

## Flashing Firmware to Hardware

When assembling the physical hardware:

```bash
cd firmware
pio run --target upload
pio device monitor
```
