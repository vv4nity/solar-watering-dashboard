#!/usr/bin/env python3
"""
PC bridge: Arduino UNO (via USB cable or HC-05 Bluetooth COM port) -> Firebase.

The UNO prints one CSV line every 2 seconds:
    soil,temperature,humidity,tank,battery,pump      e.g.  20,28.5,65,80,3.90,0
This script reads that line and PATCHes it into your Firebase Realtime Database
at /wateringSystem, which feeds the Vercel dashboard.

Why PATCH (not PUT): it updates only the sensor keys, so the dashboard's
mode/manualPump control values are preserved. It also writes a server-side
`lastSeen` timestamp so the dashboard's "online" detection is accurate.

Setup:
    pip install pyserial requests
Run:
    python pc_bridge.py
(Leave SERIAL_PORT = "" to be prompted with a list of detected ports.)
See PC_BRIDGE_SETUP.md for finding the right port on Windows/macOS/Linux.
"""

import sys
import json
import time

import requests
import serial
import serial.tools.list_ports

# ---------------------------- CONFIG ----------------------------
SERIAL_PORT  = ""        # "" = pick interactively. Else e.g. "COM5", "/dev/ttyUSB0", "/dev/tty.HC-05-DevB"
BAUD         = 9600      # must match BT.begin()/Serial.begin() in the UNO sketch
FIREBASE_URL = "https://solar-automated-water-system-default-rtdb.firebaseio.com"
DB_PATH      = "/wateringSystem"
# ----------------------------------------------------------------


def pick_port():
    """Return the configured port, or let the user choose from detected ports."""
    ports = list(serial.tools.list_ports.comports())
    if SERIAL_PORT:
        return SERIAL_PORT
    if not ports:
        sys.exit("No serial ports found. Plug in the UNO (or pair the HC-05) and retry.")
    print("Detected serial ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device}    {p.description}")
    while True:
        choice = input("Pick a port number: ").strip()
        if choice.isdigit() and int(choice) < len(ports):
            return ports[int(choice)].device
        print("  invalid choice, try again.")


def parse_line(line):
    """Turn 'soil,temp,hum,tank,batt,pump' into the dict Firebase expects."""
    parts = line.strip().split(",")
    if len(parts) < 6:
        return None
    try:
        return {
            "soil":        int(float(parts[0])),
            "temperature": float(parts[1]),
            "humidity":    int(float(parts[2])),
            "waterLevel":  int(float(parts[3])),
            "battery":     round(float(parts[4]), 2),
            "pump":        parts[5].strip() in ("1", "true", "True"),
            "solar":       0,                       # no solar sensor on the UNO build
            "lastSeen":    {".sv": "timestamp"},    # Firebase server timestamp
        }
    except ValueError:
        return None


def push(data):
    """PATCH the sensor fields into /wateringSystem (open test-mode rules => no auth)."""
    url = f"{FIREBASE_URL}{DB_PATH}.json"
    r = requests.patch(url, data=json.dumps(data), timeout=10)
    r.raise_for_status()


def read_loop(ser):
    print("Connected. Forwarding to Firebase — press Ctrl+C to stop.\n")
    while True:
        raw = ser.readline().decode("utf-8", errors="ignore")
        if not raw.strip():
            continue
        data = parse_line(raw)
        if data is None:
            print(f"  skipped malformed line: {raw.strip()!r}")
            continue
        try:
            push(data)
            print(f"  -> soil {data['soil']}%  {data['temperature']}C  "
                  f"hum {data['humidity']}%  tank {data['waterLevel']}%  "
                  f"{data['battery']}V  pump={'ON' if data['pump'] else 'off'}")
        except requests.RequestException as e:
            print(f"  Firebase push failed: {e}")
            time.sleep(2)


def main():
    port = pick_port()
    print(f"Opening {port} @ {BAUD} baud...")
    while True:
        try:
            with serial.Serial(port, BAUD, timeout=5) as ser:
                read_loop(ser)
        except serial.SerialException as e:
            print(f"Serial error: {e}\nReconnecting in 3s... (is the UNO/HC-05 powered & in range?)")
            time.sleep(3)
        except KeyboardInterrupt:
            print("\nStopped.")
            break


if __name__ == "__main__":
    main()
