# Pin Assignments & Wiring — Arduino UNO + HC-05 Build

Step-by-step wiring for [arduino_uno_hc05.ino](arduino_uno_hc05.ino). Do the steps
**in order**, with the UNO **unplugged** from USB while wiring. Power it only when a
step says to.

> ⚠️ **Common ground rule:** every component's GND must connect back to an UNO
> GND pin. The UNO has 3 GND pins — use a breadboard ground rail so they all
> share one common ground. Most "it doesn't work" problems are a missing GND.

---

## 1. Parts list

| Part | Typical module | Notes |
|------|----------------|-------|
| Arduino UNO | — | the controller |
| HC-05 | Bluetooth Classic (SPP) | the wireless link to your phone |
| DHT11 | 3-pin temp/humidity board | air temperature & humidity |
| Soil moisture sensor | YL-69 / FC-28 with comparator board | uses the **digital (DO)** output |
| Water-level / tank sensor | digital level module **or** float switch | HIGH = tank OK |
| Relay module | 1-channel, **active-LOW** | switches the pump |
| Resistors | **1 kΩ ×1, 2 kΩ ×1** (HC-05 divider), **100 kΩ ×2** (battery divider) | |
| Pump + its own power | e.g. 5–12V pump + supply | **never** powered from the UNO |
| Breadboard + jumper wires | — | |

---

## 2. Master pin assignment table

| UNO pin | Connects to | Direction | Purpose |
|--------:|-------------|-----------|---------|
| **5V**  | VCC of HC-05, DHT11, soil, water, relay | power out | 5V rail |
| **GND** | GND of everything (shared rail) | ground | common ground |
| **D2**  | DHT11 **DATA** | input | temperature + humidity |
| **D7**  | Soil sensor **DO** (digital out) | input | HIGH = dry |
| **D8**  | Water sensor **DO** (digital out) | input | HIGH = tank OK |
| **D9**  | Relay **IN** | output | LOW = pump ON (active-LOW) |
| **D10** | HC-05 **TXD** | input (RX) | data from HC-05 |
| **D11** | HC-05 **RXD** *(via divider)* | output (TX) | data to HC-05 |
| **A0**  | Battery divider mid-point | analog in | 18650 voltage |

Pins **0 and 1** are left free — they're the USB serial port used for uploading
and the 9600-baud debug monitor. The HC-05 is on D10/D11 (SoftwareSerial) on
purpose so it doesn't fight the USB.

---

## 3. Wire it up, component by component

### Step A — Breadboard power rails
1. Jump an UNO **5V** pin to the breadboard **`+`** rail.
2. Jump an UNO **GND** pin to the breadboard **`–`** rail.
3. Every VCC below goes to `+`; every GND goes to `–`.

### Step B — DHT11 (temperature & humidity)
1. DHT11 **VCC** → `+` (5V)
2. DHT11 **GND** → `–`
3. DHT11 **DATA** (sometimes labeled `OUT` or `S`) → **D2**
   - *Bare 4-pin sensor (no board)?* add a **10 kΩ** resistor between DATA and VCC.

### Step C — Soil moisture sensor
1. Sensor board **VCC** → `+` (5V)
2. Sensor board **GND** → `–`
3. Sensor board **DO** (digital out) → **D7**
   - Turn the board's potentiometer so its LED just switches at your "dry"
     point. `DO` reads **HIGH when dry**. (The `A0`/analog pin is unused.)

### Step D — Water / tank level sensor
1. **VCC** → `+` (5V)  2. **GND** → `–`  3. **DO** → **D8** (HIGH = tank OK)
   - *Using a float switch instead?* wire one leg to **D8** and the other to
     `–` (GND), and change `pinMode(WATER_PIN, INPUT)` to
     `pinMode(WATER_PIN, INPUT_PULLUP)` in the sketch (logic then inverts).

### Step E — Relay (pump switch)
1. Relay **VCC** → `+` (5V)  2. Relay **GND** → `–`  3. Relay **IN** → **D9**
4. Pump wiring through the relay contacts:
   - Pump's **own power supply (+)** → relay **COM**
   - Relay **NO** (normally-open) → pump **+**
   - Pump **–** → pump supply **–**
   > The pump is powered by its **own** supply, switched by the relay. Do **not**
   > run pump current through the UNO. Share grounds only if the datasheet calls
   > for it; an opto-isolated relay module usually doesn't need it.

### Step F — Battery sense (A0)
Build a 100k/100k divider so A0 sees **half** the battery voltage (the sketch
multiplies back by 2):
```
Battery + ──[ 100kΩ ]──┬── A0
                       │
                     [ 100kΩ ]
                       │
Battery – ────────────┴── GND (shared)
```
Skip this if you don't have a battery hooked up — A0 will just read noise and
the dashboard battery gauge will be meaningless, which is harmless.

### Step G — HC-05 Bluetooth (do this last)
1. HC-05 **VCC** → `+` (5V)
2. HC-05 **GND** → `–`
3. HC-05 **TXD** → **D10** (direct — the 3.3V signal is readable by the UNO)
4. HC-05 **RXD** → **D11 through a voltage divider** (UNO's 5V TX → ~3.3V):

```
UNO D11 ──[ 1kΩ ]──┬── HC-05 RXD
                   │
                 [ 2kΩ ]
                   │
                  GND (shared)
```
   - Leave **EN/KEY** and **STATE** unconnected for normal use.
   - The 1k/2k divider protects the HC-05's 3.3V RX input. **Don't skip it.**

---

## 4. Power-on order & test

1. Double-check **GND is shared** by everything and the two dividers are correct.
2. Plug the UNO into USB. The HC-05 LED should **blink fast (~2 Hz)** = powered,
   not yet paired.
3. Open **Serial Monitor at 9600 baud**. You should see, every 2 seconds:
   ```
   Sent: 20,28.5,65,80,3.90,0
   ```
   That's `soil,temperature,humidity,tank,battery,pump`.
4. Wave a wet finger / dip the soil probe → the `soil` number should change
   (70 wet ↔ 20 dry) and the relay should click in AUTO.
5. Once these lines look right, continue with **ANDROID_GATEWAY_SETUP.md** to
   pair the phone and bridge the data to Firebase.

---

## 5. Changing pins

All pins are `#define`d at the top of [arduino_uno_hc05.ino](arduino_uno_hc05.ino):

```cpp
#define DHTPIN      2
#define SOIL_PIN    7
#define WATER_PIN   8
#define RELAY_PIN   9
#define BATTERY_PIN A0
SoftwareSerial BT(10, 11);   // RX, TX  → HC-05 TXD=10, RXD=11
```

If you move a wire, change the matching number here and re-upload. Avoid D0/D1
(USB serial) and remember SoftwareSerial RX must stay on a pin that supports it
(D10 is fine on the UNO).
