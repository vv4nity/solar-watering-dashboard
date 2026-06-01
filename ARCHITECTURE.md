# System Architecture — Solar-Powered Automated Watering System

A small-scale farming IoT system: an ESP32 in the field senses soil/water/climate
and drives a pump, syncing in real time with a web dashboard through Firebase.
There is **no custom backend server** — Firebase is the cloud layer.

---

## 1. High-level data flow

```
   ┌──────────────────────── EDGE / DEVICE ────────────────────────┐
   │  Sensors                ESP32 (firmware)            Actuator   │
   │  • DHT11 (temp/hum) ───►                                       │
   │  • Soil moisture    ───►  read sensors                         │
   │  • Water level      ───►  AUTO / MANUAL logic  ──► Relay ──► Pump
   │  • Battery divider  ───►                                       │
   └───────────────┬───────────────────────────▲──────────────────┘
       HTTPS PATCH  │ telemetry every 2s        │ stream (instant control)
                    ▼                           │
   ┌────────────────────── CLOUD (serverless) ─────────────────────┐
   │   Firebase Realtime Database  +  Firebase Authentication       │
   │   /wateringSystem           → telemetry (soil, temp, pump…)    │
   │   /wateringSystem/control   → mode, manualPump (commands)      │
   └───────────────┬───────────────────────────▲──────────────────┘
   live WebSocket   │ onValue() subscribe       │ set() writes (commands)
                    ▼                           │
   ┌──────────────────────── FRONTEND ─────────────────────────────┐
   │  Dashboard (index.html) — static SPA running in the browser    │
   │  served by Vercel · source controlled on GitHub                │
   └────────────────────────────────────────────────────────────────┘
```

**Two independent real-time channels:**

1. **Device → Cloud → Dashboard (telemetry):** the ESP32 PATCHes sensor data
   every 2 s; the dashboard subscribes over a WebSocket (`onValue`) and re-renders
   live.
2. **Dashboard → Cloud → Device (control):** AUTO/MANUAL and pump toggles write to
   `/control`; the ESP32 **streams** that node and reacts almost instantly.

---

## 2. Layers

| Layer | Responsibility | Key components |
|-------|----------------|----------------|
| **Edge / hardware** | Sense the field, switch the pump | ESP32, DHT11, soil sensor, water-level sensor, battery divider, relay → pump, 18650 + solar |
| **Firmware** | Read sensors, run pump logic, sync with cloud | Arduino C++; Wi-Fi; Firebase streaming + batched PATCH; local AUTO/MANUAL |
| **Cloud (backend)** | Single source of truth + message bus | Firebase Realtime Database, Firebase Auth — serverless |
| **Frontend** | Live monitoring + control UI | Static HTML/CSS/JS SPA, Firebase JS SDK, Canvas chart |
| **DevOps** | Source control + hosting | GitHub → Vercel auto-deploy |

---

## 3. Tech stack

**Hardware**
- ESP32 (Wi-Fi microcontroller)
- DHT11 (air temperature + humidity)
- Resistive soil-moisture sensor (digital out)
- Analog water-level sensor (tank depth)
- 100 kΩ / 100 kΩ battery voltage divider
- Active-LOW relay module → DC water pump
- 18650 Li-ion battery + solar charging

**Firmware**
- Arduino (C++)
- Libraries: `Firebase_ESP_Client` (Mobizt), `DHT` (Adafruit), `Adafruit Unified Sensor`
- Connectivity: ESP32 Wi-Fi (2.4 GHz, WPA2)
- Patterns: **RTDB streaming** for instant control, **batched PATCH** (`updateNode`)
  for telemetry, **server-timestamp heartbeat** (`lastSeen`) for online detection

**Cloud / backend**
- Firebase Realtime Database (NoSQL JSON tree)
- Firebase Authentication (email/password, for the device)
- Serverless — no application server to run or maintain

**Frontend**
- HTML5 / CSS3 / vanilla JavaScript (no framework)
- Firebase JS SDK v10 (modular) over WebSocket
- Canvas 2D for the rolling 60-second chart
- Single-file responsive SPA (`index.html`)

**Hosting / DevOps**
- GitHub (version control)
- Vercel (static hosting + automatic deploy on every push)

---

## 4. Data model — `/wateringSystem`

```jsonc
{
  // ---- Telemetry: written by the device, read by the dashboard ----
  "soil":          70,        // % moisture
  "temperature":   30.4,      // °C
  "humidity":      69,        // %
  "waterLevel":    37,        // % tank capacity
  "waterHeightCm": 1.66,      // cm of water in the tank
  "battery":       3.88,      // V (18650)
  "solar":         0,         // W (no sensor yet)
  "pump":          false,     // actual relay/pump state
  "lastSeen":      1780318618383, // server timestamp (heartbeat)

  // ---- Control: written by the dashboard, read (streamed) by the device ----
  "control": {
    "mode":       "auto",     // "auto" | "manual"
    "manualPump": false       // pump on/off when in manual mode
  }
}
```

The `control` subtree is deliberately separate from telemetry: the device only
**reads** it (never writes there), so the device's frequent sensor writes can't
flood or drop the control stream.

---

## 5. Control logic

- **AUTO:** the pump runs when the soil is dry **and** the tank still has water
  (`soil dry && tankPercent > 20`).
- **MANUAL:** the pump follows the dashboard's `manualPump` toggle.
- The ESP32 owns the real logic; the dashboard only displays state and sends
  commands. The dashboard updates optimistically on tap, then reconciles with the
  device's reported `pump` state.

---

## 6. Alternative gateways (when not using a Wi-Fi ESP32)

The cloud + dashboard are unchanged; only how data reaches Firebase differs:

- **Arduino UNO + HC-05 Bluetooth → Android app** (MIT App Inventor / Kodular)
  forwards serial data to Firebase. See `firmware/ANDROID_GATEWAY_SETUP.md`.
- **Arduino UNO → Python PC bridge** (`pyserial` + `requests`) reads serial over
  USB or a Bluetooth COM port and PATCHes Firebase. See `firmware/PC_BRIDGE_SETUP.md`.

---

## 7. Notes / current limitations

- **Battery % and solar are mocked on the dashboard** (no sensors wired for them);
  solar follows a real time-of-day curve (none at night, peak at noon).
- Firebase Realtime Database rules are in **test mode (open)** for development —
  lock them down before any public/production deployment.
