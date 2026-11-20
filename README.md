# Vaayu Hub

> **Intelligent Room Climate Optimization Ecosystem**  
> AtomQuest 2026 — Team Vaayu (KIIT)

Vaayu Hub synchronizes ceiling fans, split air conditioners, and ventilation systems into a single energy-efficient cooling engine. Instead of chilling empty volumes of air, Vaayu focuses cooling on humans in the room, prioritizing high airflow before invoking power-hungry refrigeration cycles.

---

## Final Bill of Materials (Budget: ~₹1,890)

The entire prototype is powered by a standard **5 V, 2 A phone charger** via USB — safe low-voltage operation with zero 230 V mains rewiring.

| # | Component | Qty | Approx. Price (₹) | Role |
|---|---|---|---|---|
| 1 | **ESP32 DevKit V1 (WROOM-32)** | 1 | ₹420 | Main microhub controller (FreeRTOS, Wi-Fi, MQTT) |
| 2 | **HLK-LD2410C mmWave Radar** | 1 | ₹380 | 24 GHz human presence & micro-motion detection |
| 3 | **0.96" I2C OLED Display (SSD1306)** | 1 | ₹180 | Live diagnostics, temperature, fan %, decision reasons |
| 4 | **AHT20 Temp & Humidity Module** | 1 | ₹150 | Room ambient temperature & relative humidity (I2C) |
| 5 | **DS18B20 Waterproof Probe** | 1 | ₹120 | Verifies AC vent drop when cooling commands fire |
| 6 | **DHT11 Sensor (Bathroom)** | 1 | ₹80 | Detects shower steam & triggers exhaust ventilation |
| 7 | **940 nm IR LEDs** | 3 | ₹15 | Emits AC remote infrared command packets |
| 8 | **BC547 Transistors** | 2 | ₹10 | Current amplification for IR blasting & buzzer |
| 9 | **Resistors (100 Ω, 1 kΩ, 4.7 kΩ, 10 kΩ)** | 1 set | ₹35 | Current limiting, pull-up, base biasing |
| 10 | **VS1838B IR Receiver** | 1 | ₹30 | Detects manual human AC remote overrides |
| 11 | **L9110S Motor Driver** | 1 | ₹60 | Dual H-bridge PWM motor driver (5 V DC fans) |
| 12 | **5 V DC Fans (80 mm)** | 2 | ₹240 | Scale demo fans for Room Ceiling Fan & Bathroom Exhaust |
| 13 | **Active Buzzer** | 1 | ₹20 | Failsafe alert buzzer |
| 14 | **Breadboard + Jumper Wires** | 1 set | ₹150 | Prototyping interconnects |
| | **Total** | | **~₹1,890** | *(Well within budget limit)* |

---

## Complete Pinout & Wiring Guide (ESP32 DevKit V1)

```
                              ESP32 DevKit V1 (WROOM-32)
                                   +--------------+
                              3V3 -|              |- VIN (5V from Phone Charger)
                              GND -|              |- GND
           (DS18B20 1-Wire) GPIO4 -|              |- GPIO13
                            GPIO0 -|              |- GPIO12
                            GPIO2 -|              |- GPIO14 (L9110S B-IB Ground)
                           GPIO15 -|              |- GPIO27 (L9110S B-IA Exhaust PWM)
       (L9110S A-IB Ground) GPIO26 -|              |- GPIO26
         (L9110S A-IA Fan) GPIO25 -|              |- GPIO25
                           GPIO33 -|              |- GPIO33
                           GPIO32 -|              |- GPIO32
                           GPIO35 -|              |- GPIO35
                           GPIO34 -|              |- GPIO34
                            VN/39 -|              |- GPIO23 (Active Buzzer)
                            VP/36 -|              |- GPIO22 (I2C SCL: OLED + AHT20)
       (LD2410C Radar TX) GPIO16 -|              |- GPIO21 (I2C SDA: OLED + AHT20)
       (LD2410C Radar RX) GPIO17 -|              |- GPIO1 (TX0)
          (940nm IR Send) GPIO18 -|              |- GPIO3 (RX0)
         (VS1838B IR Recv) GPIO19 -|              |- GPIO19
                                   +--------------+
```

### Subsystem Wiring Table

| Module | Pin on Module | ESP32 Pin / Power | Notes |
|---|---|---|---|
| **AHT20 Sensor** | VIN / GND / SDA / SCL | 3V3 / GND / **GPIO 21** / **GPIO 22** | Shares I2C bus with OLED |
| **0.96" OLED** | VCC / GND / SDA / SCL | 3V3 / GND / **GPIO 21** / **GPIO 22** | Address: 0x3C |
| **HLK-LD2410C Radar** | VCC / GND / TX / RX | VIN (5V) / GND / **GPIO 16** (RX2) / **GPIO 17** (TX2) | 256000 baud UART |
| **L9110S Fan Driver** | VCC / GND | VIN (5V) / GND | Connects to 5V phone charger rail |
| | A-IA / A-IB | **GPIO 25** (PWM) / **GPIO 26** (LOW) | Channel A: 5V Room Fan |
| | B-IA / B-IB | **GPIO 27** (PWM) / **GPIO 14** (LOW) | Channel B: 5V Exhaust Fan |
| **DS18B20 Probe** | VDD / GND / DATA | 3V3 / GND / **GPIO 4** | **4.7 kΩ resistor** between DATA and 3V3 |
| **IR Transmitter** | BC547 Base | **GPIO 18** via 1 kΩ resistor | Emitter to GND, Collector to LED Cathode |
| | 940 nm IR LED | 100 Ω resistor from Anode to VIN (5V) | Pulsed at 38 kHz |
| **VS1838B Receiver** | OUT / GND / VCC | **GPIO 19** / GND / 3V3 | Remote override detector |
| **Active Buzzer** | (+) / (-) | **GPIO 23** / GND | Fault alarm |
| **Bathroom Sensor** | DHT11 Data | Wemos D1 Mini **Pin D4 (GPIO 2)** | Publishes over Wi-Fi via MQTT |

---

## Control Rules Implemented

1. **Airflow Priority (Fan First)**  
   When ambient temperature > 24 °C, fan speed ramps up proportionally. The AC engages only after 10 continuous minutes at 100% fan speed and room temperature remains >= 26 °C.

2. **Thermodynamic Setpoint Drift**  
   While the occupied space maintains comfort (<= 25 °C), the AC temperature target automatically climbs by 1 °C every 15 minutes up to 28 °C. Increased fan speed offsets the sensation of warmth via convective cooling, delivering major electrical power savings.

3. **Autonomous Vacancy Isolation**  
   The 24 GHz mmWave radar detects human presence even when stationary or sleeping. Upon vacancy, the fan disengages after 5 minutes and the AC shuts down after 10 minutes.

4. **Moisture & Exhaust Management**  
   Bathroom humidity > 70% activates ventilation. AC operation automatically switches to Dry (dehumidification) mode during monsoon conditions (> 75% RH).

5. **Human Override Respect**  
   The VS1838B infrared receiver intercepts signals from standard AC remotes. Any manual adjustment suspends automated control loops for 30 minutes.

---

## Quick Start (Hardware-Free Testing)

Experience the full control loop without connecting any physical microcontrollers:

1. Double-click `RUN_TESTS.bat` in the project root.
2. In the terminal session that opens, press `w` to open the live web dashboard at `http://localhost:8081`.
3. Observe live simulated presence, temperature transitions, fan PWM modulation, and AC setpoint adjustments.

---

## Flashing Firmware to Hardware

When your hardware components arrive:

```bash
cd firmware
pio run --target upload
pio device monitor
```
