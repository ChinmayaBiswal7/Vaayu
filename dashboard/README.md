# Vaayu Hub — Phone Dashboard

A React Native (Expo) dashboard for the **Vaayu Hub** smart room cooling controller.

---

## Features

| Screen | What it does |
|--------|-------------|
| **Home** | Live fan gauge, temp/humidity, presence, AC + exhaust status, reason string, manual override controls |
| **Log** | Last 50 reason-string change events with timestamps, newest first |
| **Settings** | Broker IP, temperature thresholds, reconnect button |

---

## Prerequisites

- [Node.js](https://nodejs.org/) ≥ 18
- [Expo CLI](https://docs.expo.dev/get-started/installation/) — `npm install -g expo-cli`
- [Expo Go](https://expo.dev/client) app on your Android/iOS phone
- **Vaayu Hub ESP32-S3** running and broadcasting its Wi-Fi AP (SSID: `Vaayu-Hub` or similar)

---

## Setup

### 1. Install dependencies

```bash
cd dashboard
npm install
```

### 2. Connect to the hub's network

On your phone, connect to the **same Wi-Fi network** as the ESP32-S3 hub.

If the hub is in **Access Point mode** (default):
- SSID: `Vaayu-Hub`
- Password: (set in `firmware/src/config.h`)
- Gateway IP: `192.168.4.1`

If the hub is connected to your **home router**, find its IP from your router's device list.

### 3. Start the app

```bash
npx expo start
```

Scan the QR code with **Expo Go** (Android) or the Camera app (iOS).

---

## Connecting to the Hub

1. Open the app → tap the **Settings** tab (⚙)
2. Enter the **Broker IP** (default: `192.168.4.1`)
3. Tap **Save & Connect**
4. The connection dot on the Home screen turns **teal** when connected

> The dashboard connects to the ESP32's Mosquitto broker on **WebSocket port 9001**.  
> Make sure the firmware has WebSocket listener enabled (see `mqtt_manager.cpp`).

---

## MQTT Topics

### Subscribed (hub → app)

| Topic | Payload |
|-------|---------|
| `vaayu/room/temp` | float, °C |
| `vaayu/room/humidity` | float, % |
| `vaayu/room/presence` | `1` / `0` |
| `vaayu/room/distance` | float, metres |
| `vaayu/fan/speed` | int 0–100 |
| `vaayu/ac/setpoint` | int, °C |
| `vaayu/ac/state` | `on` / `off` |
| `vaayu/exhaust/state` | `on` / `off` |
| `vaayu/state/reason` | string |
| `vaayu/alarm` | string or `CLEAR` |

### Published (app → hub)

| Topic | Payload |
|-------|---------|
| `vaayu/cmd/override` | `MANUAL` / `AUTO` |
| `vaayu/cmd/fan_speed` | int 0–100 |
| `vaayu/cmd/ac_setpoint` | int, °C |
| `vaayu/cmd/ac_state` | `1` / `0` |

---

## Development

```bash
# Android emulator
npx expo start --android

# iOS simulator (macOS only)
npx expo start --ios

# Type-check
npx tsc --noEmit
```

---

## Building for Production

```bash
# EAS Build (requires Expo account)
npm install -g eas-cli
eas build --platform android --profile preview
```

---

## Project Structure

```
dashboard/
├── App.tsx                        # Root: NavigationContainer + MqttProvider
├── app.json                       # Expo config
├── package.json
├── tsconfig.json
└── src/
    ├── context/
    │   └── MqttContext.tsx        # MQTT state, publish, auto-reconnect
    ├── screens/
    │   ├── HomeScreen.tsx         # Main dashboard
    │   ├── LogScreen.tsx          # Event log
    │   └── SettingsScreen.tsx     # Broker + thresholds
    └── components/
        ├── GaugeCircle.tsx        # Animated SVG circular gauge
        ├── AlarmBanner.tsx        # Dismissible red alarm
        └── StatusCard.tsx         # Reusable status tile
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| "Not connected" on home screen | Check phone is on hub's Wi-Fi, verify broker IP in Settings |
| Blank gauge values | Hub may not be publishing yet — check firmware UART logs |
| App crashes on startup | Run `npx expo start --clear` to clear Metro cache |
| WebSocket refused | Confirm Mosquitto is configured with `listener 9001` and `protocol websockets` |
