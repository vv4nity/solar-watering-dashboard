# Firebase Setup — step by step

You'll end up filling in values in two files:
- `index.html` → the `firebaseConfig` object (the dashboard)
- `firmware/solar_watering_esp32.ino` → `API_KEY`, `DATABASE_URL`, `USER_EMAIL`, `USER_PASSWORD` (the ESP32)

Use the **same Firebase project** for both.

---

## 1. Create a Firebase project
1. Go to https://console.firebase.google.com and sign in with your Google account.
2. Click **Add project** → give it a name (e.g. `solar-watering`) → Continue.
3. You can **disable Google Analytics** (not needed) → Create project → wait → Continue.

## 2. Create the Realtime Database
> Important: it's **Realtime Database**, NOT "Firestore Database".
1. Left sidebar → **Build → Realtime Database**.
2. Click **Create Database**.
3. Pick a location (any; e.g. United States) → Next.
4. Choose **Start in test mode** → Enable.
5. You'll land on the data viewer. **Copy the URL at the top** — it looks like:
   - `https://solar-watering-default-rtdb.firebaseio.com/`  *(US default)*, or
   - `https://solar-watering-default-rtdb.<region>.firebasedatabase.app/` *(other regions)*

   This is your **`databaseURL` / `DATABASE_URL`**. Copy it exactly.

## 3. Set the database rules (open, for the demo)
1. In Realtime Database, open the **Rules** tab.
2. Replace the contents with:
   ```json
   {
     "rules": {
       ".read": true,
       ".write": true
     }
   }
   ```
3. Click **Publish**.

> ⚠️ Open rules = anyone with the URL can read/write. Fine for a class demo, but
> lock it down before any real/public deployment.

## 4. Register a Web App → get the dashboard config
1. Click the **gear icon** (top left) → **Project settings**.
2. Scroll to **Your apps** → click the **`</>` (Web)** icon.
3. App nickname (e.g. `dashboard`) → **Register app**. (Skip Firebase Hosting.)
4. You'll see a `firebaseConfig` object. Copy all of it.
5. Paste those values into the `firebaseConfig` block in **`index.html`**, e.g.:
   ```js
   const firebaseConfig = {
     apiKey: "AIza....",
     authDomain: "solar-watering.firebaseapp.com",
     databaseURL: "https://solar-watering-default-rtdb.firebaseio.com",
     projectId: "solar-watering",
     storageBucket: "solar-watering.appspot.com",
     messagingSenderId: "1234567890",
     appId: "1:1234567890:web:abc123"
   };
   ```
   > If the snippet has **no `databaseURL` line**, add it manually using the URL
   > you copied in step 2 — it's required for the dashboard.

## 5. Enable Email/Password auth (for the ESP32)
1. Left sidebar → **Build → Authentication** → **Get started**.
2. **Sign-in method** tab → click **Email/Password** → toggle **Enable** → Save.
3. Go to the **Users** tab → **Add user**.
4. Enter an email + password (e.g. `device@solar-watering.com` / a password you choose).
   These do **not** have to be real — they're just for the device.
5. Put these into the firmware as **`USER_EMAIL`** and **`USER_PASSWORD`**.

## 6. Get the Web API Key (for the ESP32)
1. **Project settings → General**.
2. Copy **Web API Key** → this is the firmware's **`API_KEY`** (same value as
   `apiKey` in the dashboard config).

## 7. Fill in the firmware
In `firmware/solar_watering_esp32.ino`:
```cpp
const char* WIFI_SSID     = "your wifi name";
const char* WIFI_PASSWORD = "your wifi password";

#define API_KEY       "AIza...."                                  // step 6
#define DATABASE_URL  "https://solar-watering-default-rtdb.firebaseio.com"  // step 2
#define USER_EMAIL    "device@solar-watering.com"                 // step 5
#define USER_PASSWORD "your_device_password"                      // step 5
```

## 8. Test
1. Upload the firmware. Open the Arduino **Serial Monitor** (115200 baud) —
   you should see it connect to WiFi, then print sensor lines every ~2s.
2. In the Firebase console → Realtime Database, you should see a
   `wateringSystem` node appear and update live.
3. Open the dashboard (`index.html`). The header pill should switch from
   **Connecting… → ESP32 Online**, and the cards should show live values.
4. Switch to **MANUAL** on the dashboard and toggle the pump — within ~2s the
   relay should follow (and the badge flips ON/OFF).

## Troubleshooting
- **Dashboard stays "Connecting…"** → check `databaseURL` is present and exact in
  `firebaseConfig`; check the Activity Log / browser console for a Firebase error.
- **"permission denied"** → the rules in step 3 weren't published.
- **ESP32 won't authenticate** → re-check `API_KEY`, and that the Email/Password
  user from step 5 exists and matches `USER_EMAIL`/`USER_PASSWORD`.
- **Wrong database region** → the `databaseURL` must match what step 2 showed
  (`.firebaseio.com` vs `.firebasedatabase.app`).
