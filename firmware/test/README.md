# Vaayu Hub — Unit Test Guide

This directory contains PlatformIO / Unity unit tests for the Vaayu Hub firmware.

## Directory Layout

```
test/
├── test_control/
│   └── test_control.cpp   # State-machine tests (15 tests)
└── test_sensors/
    └── test_sensors.cpp   # Sensor validation tests (8 tests)
```

## Running Tests

### On real hardware (ESP32-S3 DevKitC-1)

Flash the test binary and read results over USB-CDC serial:

```bash
pio test -e esp32-s3-devkitc-1
```

The board must be connected and the correct COM port auto-detected by PlatformIO.

### On PC (no hardware needed)

Build and run natively using the GCC native toolchain:

```bash
pio test -e native
```

This compiles the test files and all stubbed dependencies into a regular
executable and prints Unity results to your terminal. No ESP32 required.

> [!TIP]
> Run `pio test -e native -f test_control` to run only the control tests,
> or `-f test_sensors` for sensor tests only.

## What is Tested

### `test_control` (15 tests)

| # | Test | Rule covered |
|---|------|-------------|
| 1 | `test_fan_starts_above_24c` | Fan-first: fan on above 24 °C |
| 2 | `test_ac_requires_10min_full_fan` | AC NOT on before 10 min full fan |
| 3 | `test_ac_turns_on_after_10min` | AC turns on after 10 min full fan |
| 4 | `test_setpoint_creep` | Setpoint creeps +1 °C after 15 min comfortable |
| 5 | `test_setpoint_max_28` | Setpoint never exceeds 28 °C |
| 6 | `test_empty_room_fan_off_at_5min` | Fan off after 5 min empty |
| 7 | `test_empty_room_ac_off_at_10min` | AC off after 10 min empty |
| 8 | `test_exhaust_on_above_70_humidity` | Exhaust on above 70 % RH |
| 9 | `test_exhaust_off_below_60_with_delay` | Hysteresis: exhaust stays on < 2 min |
| 10 | `test_exhaust_off_after_delay` | Exhaust off after 2-min low-humidity delay |
| 11 | `test_alarm_on_sensor_oor` | Alarm after 30 s sensor fault |
| 12 | `test_monsoon_mode` | Monsoon: AC switches to DRY above 75 % RH |
| 13 | `test_distance_affects_fan_speed` | Closer = higher fan speed |
| 14 | `test_reason_string_not_empty` | OLED reason string always populated |
| 15 | `test_ir_override_pauses_automation` | IR override blocks automation for 30 min |

### `test_sensors` (8 tests)

| # | Test | What it checks |
|---|------|---------------|
| 1 | `test_outlier_rejection` | > 5 °C jump is rejected |
| 2 | `test_valid_reading_accepted` | Normal reading accepted |
| 3 | `test_oor_detection_high` | 999 °C flagged as OOR |
| 4 | `test_oor_detection_low` | −999 °C flagged as OOR |
| 5 | `test_boundary_min_temp_accepted` | −10 °C (boundary) accepted |
| 6 | `test_below_min_temp_rejected` | −10.1 °C rejected |
| 7 | `test_humidity_outlier_rejection` | > 20 %RH jump rejected |
| 8 | `test_valid_humidity_accepted` | Small humidity change accepted |

## Wokwi Simulation Notes

> [!IMPORTANT]
> **Uncomment `SIMULATE_SENSORS` in `src/config.h` before building for Wokwi**
> to bypass real hardware sensor reads and inject fake data instead.
> This lets the full firmware run in the Wokwi browser simulator without
> needing a physical SHT41, DS18B20, or LD2410C.

```cpp
// In src/config.h — uncomment this line before a Wokwi build:
#define SIMULATE_SENSORS
```

Then build normally:
```bash
pio run -e esp32-s3-devkitc-1
```

Open `wokwi.toml` in VS Code with the Wokwi extension, or upload to
[wokwi.com](https://wokwi.com) with `diagram.json` and the compiled `.elf`.

## Adding New Tests

1. Create `test/test_<module>/test_<module>.cpp`
2. Include `<unity.h>`, write `setUp()`, `tearDown()`, and test functions
3. Add a `main()` that calls `UNITY_BEGIN()`, `RUN_TEST(...)`, `UNITY_END()`
4. PlatformIO auto-discovers test directories — no `CMakeLists.txt` needed
