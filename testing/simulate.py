"""
Vaayu Hub — 30-minute room session simulator
Publishes realistic MQTT data to a local Mosquitto broker (localhost:1883)
covering all phases of the Vaayu control logic.

Usage:
    pip install paho-mqtt rich
    python simulate.py
"""

import time
import random
import math
import json
import threading
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    raise SystemExit("paho-mqtt not installed. Run: pip install paho-mqtt")

try:
    from rich.console import Console
    from rich.table import Table
    from rich.live import Live
    from rich import box
    HAS_RICH = True
except ImportError:
    HAS_RICH = False

# ─────────────────────────── MQTT config ───────────────────────────
BROKER_HOST = "127.0.0.1"
BROKER_PORT = 1883
PUBLISH_INTERVAL = 2.0          # seconds between publishes

# ─────────────────────────── Topics ────────────────────────────────
TOPICS = {
    "temp":           "vaayu/room/temp",
    "humidity":       "vaayu/room/humidity",
    "presence":       "vaayu/room/presence",
    "distance":       "vaayu/room/distance",
    "fan_speed":      "vaayu/fan/speed",
    "ac_state":       "vaayu/ac/state",
    "ac_setpoint":    "vaayu/ac/setpoint",
    "exhaust_state":  "vaayu/exhaust/state",
    "bath_humidity":  "vaayu/bathroom/humidity",
    "reason":         "vaayu/state/reason",
    "alarm":          "vaayu/alarm",
}

# ─────────────────────────── Simulation phases ─────────────────────
#   Each phase: (start_min, end_min, description, dict of target values)
PHASES = [
    (0,  5,  "Room empty — baseline idle",
     dict(temp=23.0, humidity=55.0, presence=False, distance=0,
          fan_speed=0, ac_state="OFF", ac_setpoint=26, exhaust_state="OFF",
          bath_humidity=55.0, reason="Room empty — all systems off", alarm="NONE")),

    (5,  10, "Person enters — fan ramps up",
     dict(temp=25.0, humidity=58.0, presence=True,  distance=180,
          fan_speed=40, ac_state="OFF", ac_setpoint=26, exhaust_state="OFF",
          bath_humidity=55.0, reason="Presence detected, temp rising — fan 40%", alarm="NONE")),

    (10, 20, "Temp rising — full fan, waiting for AC timer",
     dict(temp=27.5, humidity=62.0, presence=True,  distance=120,
          fan_speed=100, ac_state="OFF", ac_setpoint=26, exhaust_state="OFF",
          bath_humidity=57.0, reason="Full fan — waiting 10 min before AC", alarm="NONE")),

    (20, 25, "AC ON — fan backs off to 60%",
     dict(temp=26.5, humidity=60.0, presence=True,  distance=120,
          fan_speed=60, ac_state="ON",  ac_setpoint=26, exhaust_state="OFF",
          bath_humidity=58.0, reason="AC running at 26°C, fan assist 60%", alarm="NONE")),

    (25, 28, "Bathroom steam — exhaust fan kicks in",
     dict(temp=26.2, humidity=61.0, presence=True,  distance=100,
          fan_speed=60, ac_state="ON",  ac_setpoint=26, exhaust_state="ON",
          bath_humidity=78.0, reason="Bathroom humidity 78% — exhaust on", alarm="NONE")),

    (28, 30, "Person leaves — empty-room shutoff sequence",
     dict(temp=25.8, humidity=59.0, presence=False, distance=0,
          fan_speed=0, ac_state="OFF", ac_setpoint=26, exhaust_state="OFF",
          bath_humidity=62.0, reason="Empty room — fan off, AC off", alarm="NONE")),
]

# Fault injection window (seconds into the simulation)
FAULT_START_SEC = 15 * 60      # minute 15
FAULT_END_SEC   = FAULT_START_SEC + 10


def lerp(a, b, t):
    """Linear interpolate between a and b (t in [0,1])."""
    return a + (b - a) * t


def noise(magnitude=0.3):
    """Small random fluctuation."""
    return random.uniform(-magnitude, magnitude)


def get_phase_values(elapsed_sec: float) -> dict:
    """
    Returns interpolated sensor/state values for a given elapsed time (seconds).
    Handles the fault injection window separately.
    """
    elapsed_min = elapsed_sec / 60.0

    # Find surrounding phases
    prev_phase = PHASES[0]
    next_phase = PHASES[0]
    for phase in PHASES:
        if elapsed_min >= phase[0]:
            prev_phase = phase
        if elapsed_min < phase[1]:
            next_phase = phase
            break
    else:
        next_phase = PHASES[-1]

    # Interpolation factor within the current phase
    p_start, p_end = prev_phase[0], prev_phase[1]
    phase_duration = max(p_end - p_start, 0.001)
    t = max(0.0, min(1.0, (elapsed_min - p_start) / phase_duration))

    pv = prev_phase[3]
    nv = next_phase[3]

    values = {}
    for key in pv:
        pval = pv[key]
        nval = nv[key]
        if isinstance(pval, (int, float)):
            values[key] = lerp(float(pval), float(nval), t)
        else:
            # boolean / string: switch at 50% through phase
            values[key] = pval if t < 0.5 else nval

    # Add noise to continuous readings
    values["temp"]          = round(values["temp"] + noise(0.3), 2)
    values["humidity"]      = round(max(0, min(100, values["humidity"] + noise(0.5))), 1)
    values["bath_humidity"] = round(max(0, min(100, values["bath_humidity"] + noise(0.4))), 1)
    values["fan_speed"]     = int(max(0, min(100, values["fan_speed"] + noise(2))))
    values["distance"]      = int(max(0, values["distance"] + noise(5)))

    # ── Fault injection at minute 15 (10 seconds) ──────────────────
    if FAULT_START_SEC <= elapsed_sec < FAULT_END_SEC:
        values["temp"]   = 999.0
        values["alarm"]  = "SENSOR_FAULT: temp OOR (999.0°C)"
        values["reason"] = "⚠ Sensor fault! Holding last good value."
    elif elapsed_sec == FAULT_END_SEC:
        values["alarm"]  = "CLEARED"

    return values


# ─────────────────────────── MQTT client ───────────────────────────
try:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id="vaayu-simulator", protocol=mqtt.MQTTv311)
except AttributeError:
    client = mqtt.Client(client_id="vaayu-simulator", protocol=mqtt.MQTTv311)


def on_connect(c, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Connected to {BROKER_HOST}:{BROKER_PORT}")
    else:
        print(f"[MQTT] Connection failed: rc={rc}")

def on_disconnect(c, userdata, rc):
    print(f"[MQTT] Disconnected (rc={rc}). Reconnecting…")

client.on_connect    = on_connect
client.on_disconnect = on_disconnect


def publish_values(values: dict):
    """Publish all sensor values to their respective MQTT topics."""
    client.publish(TOPICS["temp"],          str(values["temp"]),           retain=True)
    client.publish(TOPICS["humidity"],      str(values["humidity"]),        retain=True)
    client.publish(TOPICS["presence"],      "true" if values["presence"] else "false", retain=True)
    client.publish(TOPICS["distance"],      str(values["distance"]),        retain=True)
    client.publish(TOPICS["fan_speed"],     str(values["fan_speed"]),       retain=True)
    client.publish(TOPICS["ac_state"],      str(values["ac_state"]),        retain=True)
    client.publish(TOPICS["ac_setpoint"],   str(int(values["ac_setpoint"])),retain=True)
    client.publish(TOPICS["exhaust_state"], str(values["exhaust_state"]),   retain=True)
    client.publish(TOPICS["bath_humidity"], str(values["bath_humidity"]),   retain=True)
    client.publish(TOPICS["reason"],        str(values["reason"]),          retain=True)
    client.publish(TOPICS["alarm"],         str(values["alarm"]),           retain=True)


# ─────────────────────────── Console display ───────────────────────
if HAS_RICH:
    console = Console()

    def make_table(values: dict, elapsed_sec: float, phase_desc: str) -> Table:
        mm = int(elapsed_sec // 60)
        ss = int(elapsed_sec % 60)
        t = Table(
            title=f"[bold cyan]Vaayu Hub Simulator[/bold cyan]  "
                  f"[dim]{mm:02d}:{ss:02d} / 30:00[/dim]  "
                  f"[yellow]{phase_desc}[/yellow]",
            box=box.ROUNDED,
            show_header=True,
            header_style="bold magenta",
        )
        t.add_column("Sensor / State",   style="cyan",   width=22)
        t.add_column("Value",            style="white",  width=20)
        t.add_column("Topic",            style="dim",    width=28)

        alarm_val = values.get("alarm", "NONE")
        temp_val  = values["temp"]
        temp_style = "[red]" if temp_val == 999.0 else "[green]" if temp_val <= 25 else "[yellow]"

        rows = [
            ("Room Temp",        f"{temp_style}{temp_val}°C",         TOPICS["temp"]),
            ("Room Humidity",    f"{values['humidity']}%",             TOPICS["humidity"]),
            ("Presence",         "✓ YES" if values["presence"] else "✗ NO", TOPICS["presence"]),
            ("Distance",         f"{values['distance']} cm",           TOPICS["distance"]),
            ("Fan Speed",        f"{values['fan_speed']}%",            TOPICS["fan_speed"]),
            ("AC State",         values["ac_state"],                   TOPICS["ac_state"]),
            ("AC Setpoint",      f"{int(values['ac_setpoint'])}°C",    TOPICS["ac_setpoint"]),
            ("Exhaust Fan",      values["exhaust_state"],              TOPICS["exhaust_state"]),
            ("Bathroom Humidity",f"{values['bath_humidity']}%",        TOPICS["bath_humidity"]),
            ("Reason",           values["reason"][:35],                TOPICS["reason"]),
            ("Alarm",            f"[red]{alarm_val}" if alarm_val not in ("NONE","CLEARED") else alarm_val, TOPICS["alarm"]),
        ]
        for name, val, topic in rows:
            t.add_row(name, val, topic)
        return t

else:
    def make_table(values: dict, elapsed_sec: float, phase_desc: str):  # noqa: F811
        return None


def print_plain(values: dict, elapsed_sec: float, phase_desc: str):
    mm = int(elapsed_sec // 60)
    ss = int(elapsed_sec % 60)
    sep = "=" * 60
    print(sep)
    print(f"  Vaayu Simulator  {mm:02d}:{ss:02d}/30:00  |  {phase_desc}")
    print(sep)
    print(f"  Temp:       {values['temp']}°C    Humidity: {values['humidity']}%")
    print(f"  Presence:   {'YES' if values['presence'] else 'NO':5}    Distance: {values['distance']} cm")
    print(f"  Fan:        {values['fan_speed']}%     AC: {values['ac_state']} @ {int(values['ac_setpoint'])}°C")
    print(f"  Exhaust:    {values['exhaust_state']}     BathHumid: {values['bath_humidity']}%")
    print(f"  Reason:     {values['reason']}")
    print(f"  Alarm:      {values['alarm']}")
    print()


# ─────────────────────────── Main loop ─────────────────────────────
def current_phase_desc(elapsed_sec: float) -> str:
    elapsed_min = elapsed_sec / 60.0
    for phase in PHASES:
        if phase[0] <= elapsed_min < phase[1]:
            return phase[2]
    return PHASES[-1][2]


def run_simulation():
    total_seconds = 30 * 60   # 30-minute session
    start_time    = time.time()

    print("=" * 60)
    print("  Vaayu Hub Simulator starting…")
    print(f"  Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"  Duration: 30 minutes (simulated), publishing every {PUBLISH_INTERVAL}s")
    print("  Press Ctrl+C to stop early")
    print("=" * 60)

    if HAS_RICH:
        with Live(console=console, refresh_per_second=1) as live:
            while True:
                elapsed = time.time() - start_time
                if elapsed > total_seconds:
                    break
                values      = get_phase_values(elapsed)
                phase_desc  = current_phase_desc(elapsed)
                publish_values(values)
                live.update(make_table(values, elapsed, phase_desc))
                time.sleep(PUBLISH_INTERVAL)
    else:
        while True:
            elapsed = time.time() - start_time
            if elapsed > total_seconds:
                break
            values     = get_phase_values(elapsed)
            phase_desc = current_phase_desc(elapsed)
            publish_values(values)
            print_plain(values, elapsed, phase_desc)
            time.sleep(PUBLISH_INTERVAL)

    print("\n[Simulator] 30-minute session complete. Final state published.")
    # Leave last values retained on broker so dashboard still shows something


def main():
    try:
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    except ConnectionRefusedError:
        raise SystemExit(
            f"\n[ERROR] Cannot connect to Mosquitto at {BROKER_HOST}:{BROKER_PORT}\n"
            "Make sure Mosquitto is running. Use START_TESTING.bat to launch it."
        )

    client.loop_start()
    time.sleep(0.5)   # wait for on_connect

    try:
        run_simulation()
    except KeyboardInterrupt:
        print("\n[Simulator] Stopped by user.")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
