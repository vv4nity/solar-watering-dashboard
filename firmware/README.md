# ESP32 Firmware — Solar Watering System

`solar_watering_esp32.ino` reads the sensors, controls the pump relay, and
syncs with the dashboard through **Firebase Realtime Database**.

## Data path

```
ESP32 ──WiFi──> Firebase RTDB (/wateringSystem) ──> dashboard (index.html)
   ▲                                                      │
   └──────── reads /mode and /manualPump <───────── AUTO/MANUAL buttons
```

The ESP32 **writes** sensor values and **reads** the control values the
dashboard writes, so the MANUAL pump button and AUTO/MANUAL toggle work.

## 1. Libraries (Arduino IDE → Tools → Manage Libraries)

- **Firebase Arduino Client Library for ESP8266 and ESP32** (by Mobizt)
- **DHT sensor library** (by Adafruit)
- **Adafruit Unified Sensor**

## 2. Fill in the placeholders in the `.ino`

- `WIFI_SSID`, `WIFI_PASSWORD`
- `API_KEY` — Firebase console → Project settings → Web API Key
- `DATABASE_URL` — your `...-default-rtdb.firebaseio.com` URL
- `USER_EMAIL` / `USER_PASSWORD` — create this user under
  Authentication → Sign-in method → **Email/Password**

Use the **same Firebase project** in both the firmware and the
`firebaseConfig` block in `../index.html`.

## 3. Database rules (the easy-to-miss step)

The dashboard reads/writes **without signing in**, so for a demo set the
Realtime Database rules to open:

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

⚠️ This is fine for a classroom demo but is **insecure** for real deployment —
anyone with the URL can read/write. Lock it down before going public.

## Data the firmware publishes to `/wateringSystem`

| Field         | Type  | Source                                             |
|---------------|-------|----------------------------------------------------|
| `temperature` | float | DHT11 °C                                           |
| `humidity`    | float | DHT11 %                                            |
| `soil`        | int   | digital sensor mapped: **DRY→20%**, WET→70%        |
| `waterLevel`  | int   | digital sensor mapped: OK→80%, **LOW→10%**         |
| `battery`     | float | 18650 voltage via 100k+100k divider                |
| `solar`       | int   | **0** — no solar sensor fitted yet                 |
| `pump`        | bool  | actual relay state                                 |

## Known limitations / upgrades

- **Soil & tank are digital**, so they only report two levels. The dashboard
  ring/bar will jump between the mapped values rather than showing a smooth
  curve. To get a real gradient, wire the soil sensor's **analog (AO)** output
  to an ADC1 pin (GPIO32/33/35/36/39 — *not* the battery pin 34) and switch
  the firmware to `analogRead()`.
- **No solar measurement.** The Solar card will read 0 W. For real watts you
  need a panel voltage divider **and** a current sensor (e.g. INA219), then
  `power = volts × amps`.
