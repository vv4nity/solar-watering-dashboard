# PC Bridge Setup — UNO → PC → Firebase

A small Python script ([pc_bridge.py](pc_bridge.py)) reads the UNO's sensor data
on your computer and forwards it to Firebase, which feeds the Vercel dashboard.

```
Sensors → Arduino UNO ──(USB cable OR HC-05 Bluetooth)──▶ PC (pc_bridge.py) → Firebase → Vercel
```

This is the most reliable gateway — **no app to build** — and it improves on the
phone version: it PATCHes (so your `mode`/`manualPump` control fields survive) and
writes a real `lastSeen` timestamp for accurate online status.

> The PC must stay on and running the script while you want live data — same
> trade-off as a phone gateway.

---

## 1. Install Python + dependencies

1. Install **Python 3** from <https://python.org> (tick *"Add Python to PATH"* on Windows).
2. In a terminal:
   ```
   pip install pyserial requests
   ```

## 2. Choose how to connect the UNO

**Option A — USB cable (simplest, no Bluetooth):**
Just leave the UNO plugged into the PC with its normal USB cable. Done.

**Option B — HC-05 wirelessly (UNO not tethered to the PC):**
Pair the HC-05 to your computer once; it then shows up as a serial/COM port.
- **Windows:** Settings → Bluetooth → *Add device* → **HC-05** (PIN `1234`/`0000`).
  Then Control Panel → *Devices and Printers* → HC-05 → *Properties → Hardware*
  to see its **COM port** (e.g. `COM5`). Use the **outgoing** one.
- **macOS:** System Settings → Bluetooth → pair **HC-05**. The port appears as
  `/dev/tty.HC-05-DevB` (or similar).
- **Linux:** `sudo rfcomm bind 0 <HC-05-MAC>` → port is `/dev/rfcomm0`.

## 3. Find the port name

Run the script with no port set — it lists everything it detects:
```
python pc_bridge.py
```
You'll see something like:
```
Detected serial ports:
  [0] COM3    Arduino Uno
  [1] COM5    Standard Serial over Bluetooth link
Pick a port number:
```
Type the number for your UNO (USB) or the HC-05 Bluetooth port.

> Tip: to skip the prompt every time, open [pc_bridge.py](pc_bridge.py) and set
> `SERIAL_PORT = "COM5"` (or your `/dev/...` path) near the top.

## 4. Run it

```
python pc_bridge.py
```
You should see lines like:
```
Connected. Forwarding to Firebase — press Ctrl+C to stop.
  -> soil 20%  28.5C  hum 65%  tank 80%  3.90V  pump=off
```
Within ~2s your **Vercel dashboard** shows live values and flips to **Online**.

---

## Troubleshooting

| Symptom | Fix |
|--------|-----|
| `could not open port` / `Access is denied` | Close the Arduino IDE **Serial Monitor** — only one program can hold the port. |
| `skipped malformed line` | Baud mismatch. The UNO sketch uses **9600**; keep `BAUD = 9600`. |
| Garbled characters | Wrong baud, or you picked the wrong port. |
| `Firebase push failed` | Check internet, and that `FIREBASE_URL` matches your project. |
| Dashboard stays Offline | Confirm the script prints `->` lines; check the browser console for Firebase errors. |
| HC-05 port keeps dropping | Move closer; the script auto-reconnects every 3s. |

## Config (top of pc_bridge.py)

```python
SERIAL_PORT  = ""        # "" = prompt;  else "COM5" / "/dev/ttyUSB0" / "/dev/tty.HC-05-DevB"
BAUD         = 9600
FIREBASE_URL = "https://solar-automated-water-system-default-rtdb.firebaseio.com"
DB_PATH      = "/wateringSystem"
```
