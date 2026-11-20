"""
Vaayu Hub — Built-in Python MQTT + WebSocket Broker
Runs on:
  - TCP port 1883 (Standard MQTT for firmware / python simulator)
  - WebSocket port 9001 (WebSockets for Expo phone dashboard)

Zero setup required. Just run: python broker.py
"""

import asyncio
import sys
import logging
from amqtt.broker import Broker

# Suppress noisy debug logs
logging.basicConfig(level=logging.INFO)
logging.getLogger("amqtt").setLevel(logging.WARNING)

config = {
    'listeners': {
        'default': {
            'type': 'tcp',
            'bind': '0.0.0.0:1883',
            'max_connections': 100,
        },
        'ws': {
            'type': 'ws',
            'bind': '0.0.0.0:9001',
            'max_connections': 100,
        }
    },
    'sys_interval': 0,
    'auth': {
        'allow-anonymous': True,
        'plugins': ['auth_anonymous']
    }
}

async def run_broker():
    broker = Broker(config)
    await broker.start()
    print("=" * 60)
    print("  [VAAYU HUB] Local MQTT Broker is ACTIVE")
    print("  - TCP Port:       1883 (firmware & simulation)")
    print("  - WebSocket Port: 9001 (mobile dashboard)")
    print("  Press Ctrl+C to stop.")
    print("=" * 60)
    try:
        while True:
            await asyncio.sleep(1)
    except (asyncio.CancelledError, KeyboardInterrupt):
        pass
    finally:
        print("\nShutting down MQTT broker...")
        await broker.shutdown()

if __name__ == '__main__':
    try:
        asyncio.run(run_broker())
    except KeyboardInterrupt:
        print("Broker stopped.")
        sys.exit(0)
