# Arduino UNO + HC-05 + Android → Firebase

The UNO has no internet, so your **Android phone is the gateway**:

```
Sensors → Arduino UNO → HC-05 ──Bluetooth──▶ Android app → Firebase → Vercel dashboard
```

The app writes to the **same** `/wateringSystem` path your dashboard already reads,
so nothing on the web side changes.

---

## 1. Wiring (UNO ↔ HC-05)

| HC-05 pin | Connects to | Notes |
|-----------|-------------|-------|
| VCC       | UNO **5V**  | |
| GND       | UNO **GND** | |
| TXD       | UNO **pin 10** | direct (3.3V signal is fine for the UNO) |
| RXD       | UNO **pin 11** *through a voltage divider* | see below |

**Voltage divider on HC-05 RXD** (UNO TX is 5V, HC-05 RX wants ~3.3V):

```
UNO pin 11 ──[ 1kΩ ]──┬── HC-05 RXD
                      │
                    [ 2kΩ ]
                      │
                     GND
```

Sensors (match the sketch, change pins if you like):
- **DHT11** data → pin **2**
- **Soil** sensor digital out → pin **7** (HIGH = dry)
- **Water/tank** sensor digital out → pin **8** (HIGH = ok)
- **Battery** divider mid-point → **A0**
- **Relay** IN → pin **9** (active-LOW; power the relay from its own 5V, share GND)

## 2. Flash the UNO

Upload [arduino_uno_hc05.ino](arduino_uno_hc05.ino). Open Serial Monitor at **9600**
baud — you should see `Sent: 20,28.5,65,80,3.90,0` lines every 2 seconds. That
confirms the sensors + sketch work *before* you involve Bluetooth.

> The HC-05 LED blinks fast (~2/sec) when unpaired, slow/solid once connected.

## 3. Pair the HC-05 with the phone (once)

Android **Settings → Bluetooth → Pair new device → HC-05**. PIN is usually
**1234** or **0000**. You only pair once; the app connects to the paired device.

## 4. Build the Android app (MIT App Inventor)

Go to <https://appinventor.mit.edu> → **Create Apps** → **Start new project**.

### Designer — drop in these components
- `ListPicker1` — button to pick the HC-05 (Palette → Layout/Basic)
- `BluetoothClient1` — Palette → **Connectivity** (non-visible)
- `Web1` — Palette → **Connectivity** (non-visible)
- `Clock1` — Palette → **Sensors** (TimerInterval = `1000`, TimerEnabled = checked)
- a couple of `Label`s (`lblStatus`, `lblData`) to show what's happening

### Blocks

**Screen1.Initialize** — read until newline:
```
set BluetoothClient1.DelimiterByte to 10        // 10 = '\n'
```

**ListPicker1.BeforePicking** — list paired devices:
```
set ListPicker1.Elements to BluetoothClient1.AddressesAndNames
```

**ListPicker1.AfterPicking** — connect:
```
if  call BluetoothClient1.Connect address ListPicker1.Selection
then set lblStatus.Text to "Connected"
else set lblStatus.Text to "Failed"
```

**Clock1.Timer** — receive a line and forward it to Firebase:
```
if BluetoothClient1.IsConnected
and (BluetoothClient1.BytesAvailableToReceive > 0)
then
    set global line  to  call BluetoothClient1.ReceiveText  numberOfBytes = -1
    set global parts to  split  text = global line  at = ","

    if  length of list (global parts) ≥ 6  then
        set global json to  join
            "{\"soil\":"          ,  select list item global parts index 1
            ",\"temperature\":"   ,  select list item global parts index 2
            ",\"humidity\":"      ,  select list item global parts index 3
            ",\"waterLevel\":"    ,  select list item global parts index 4
            ",\"battery\":"       ,  select list item global parts index 5
            ",\"pump\":"          ,  select list item global parts index 6
            ",\"solar\":0}"

        set Web1.Url to
            "https://solar-automated-water-system-default-rtdb.firebaseio.com/wateringSystem.json"
        call Web1.PutText  text = global json

        set lblData.Text to global line
```

That's it. `ReceiveText` with `numberOfBytes = -1` returns one full line (because
the delimiter is `\n`). The `Web1.PutText` does an HTTP **PUT** to your Realtime
Database — and since your rules are still in **test mode (open)**, no login is
needed.

## 5. Run it

1. Power the UNO, make sure the HC-05 is paired.
2. Open the app → tap **ListPicker → HC-05** → "Connected".
3. Within ~2s your **Vercel dashboard** shows live values and flips to
   **ESP32 Online** (the badge label still says ESP32 — cosmetic).

---

## Notes & limits

- **PUT overwrites the whole `/wateringSystem` node** each push, so it omits
  `mode`/`manualPump`. That's fine here because control is one-way for now.
- **Online detection:** the dashboard marks online whenever fresh sensor data
  arrives (it falls back to that when no `lastSeen` is present), and flips to
  Offline after ~15s with no update — i.e. if the phone or UNO stops.
- **The phone must stay on, paired, and near the UNO.** That's the trade-off of
  a phone gateway vs. a standalone ESP32/ESP8266.
- **iPhones won't work** with HC-05 (no Bluetooth-SPP support) — Android only.
- **Phase 2 (remote MANUAL control):** to let the dashboard's AUTO/MANUAL
  buttons drive the relay, the app would also `Web1.Get` `…/mode.json` and
  `…/manualPump.json`, then `BluetoothClient.SendText` them back to the UNO,
  which reads them with `BT.available()` and overrides its local AUTO logic.
