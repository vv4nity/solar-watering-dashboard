/* ============================================================
   Solar-Powered Automated Watering System — ESP32 firmware
   Pushes sensor data to Firebase Realtime Database at
   /wateringSystem, and reads back /wateringSystem/mode and
   /wateringSystem/manualPump so the dashboard's AUTO/MANUAL
   buttons control the relay.

   Libraries (Arduino IDE → Library Manager):
     - "Firebase Arduino Client Library for ESP8266 and ESP32" (by Mobizt)
     - "DHT sensor library" (by Adafruit)
     - "Adafruit Unified Sensor"
   ============================================================ */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"   // ships with the Firebase library
#include "addons/RTDBHelper.h"    // ships with the Firebase library
#include <DHT.h>
#include "soc/soc.h"              // for the brownout-detector workaround
#include "soc/rtc_cntl_reg.h"

/* ---------- WiFi ---------- */
const char* WIFI_SSID     = "Eman";
const char* WIFI_PASSWORD = "12345678902";

/* ---------- Firebase ---------- */
// Firebase console → Project settings → General → Web API Key
#define API_KEY      "AIzaSyAzUQjAuWQOQFl8kEUSY7dKXcLMPB1uQ5Y"
// Realtime Database URL
#define DATABASE_URL "https://solar-automated-water-system-default-rtdb.firebaseio.com"
// Create this user under Firebase console → Authentication → Sign-in method → Email/Password
#define USER_EMAIL    "solarpoweredwatersystem@gmail.com"
#define USER_PASSWORD "JABONeman1!"

/* ---------- Pins ---------- */
#define DHTPIN     4
#define DHTTYPE    DHT11
#define RELAY_PIN  18    // active-LOW relay (LOW = pump ON)
#define SOIL_PIN   19    // digital soil sensor: HIGH = DRY, LOW = WET
#define WATER_PIN  21    // digital tank sensor: HIGH = OK,  LOW = LOW
#define BATTERY_PIN 34   // ADC1 — 18650 via 100k+100k divider

DHT dht(DHTPIN, DHTTYPE);

FirebaseData fbdo;        // for sensor writes
FirebaseData streamFbdo;  // dedicated stream for INSTANT control updates
FirebaseAuth auth;
FirebaseConfig config;

bool pumpState = false;

// Control values mirrored from the dashboard
String mode       = "auto";   // "auto" | "manual"
bool   manualPump = false;

unsigned long lastPush = 0;
const unsigned long PUSH_INTERVAL = 2000;  // ms

/* ---------- WiFi helpers ---------- */
const char* wifiStatusStr(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL (network not found — 5GHz? hidden? typo?)";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED (wrong password?)";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

// List nearby 2.4GHz networks so you can confirm the SSID is visible and which
// band/channel it's on (the ESP32 radio CANNOT see 5GHz networks at all).
void scanNetworks() {
  Serial.println("\nScanning for 2.4GHz networks...");
  int n = WiFi.scanNetworks();
  if (n <= 0) { Serial.println("  (none found — is your router 2.4GHz enabled?)"); return; }
  for (int i = 0; i < n; i++) {
    Serial.printf("  %2d) %-24s  RSSI %ddBm  ch%d  %s\n",
                  i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
  }
  WiFi.scanDelete();
}

void connectWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);          // station mode only
  WiFi.disconnect(true, true);  // clear any stale config
  WiFi.setSleep(false);         // keep radio awake (helps weak/flaky links)
  delay(200);

  scanNetworks();

  Serial.printf("\nConnecting to \"%s\"", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500); Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected. IP: %s  RSSI: %ddBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.printf("\nFailed: %s\n", wifiStatusStr(WiFi.status()));
    Serial.println("Checklist: 2.4GHz band, exact SSID/password, signal strength.");
    Serial.println("Restarting in 5s to retry...");
    delay(5000);
    ESP.restart();
  }
}

void setup() {
  // WORKAROUND: disable the brownout detector so a sagging USB supply doesn't
  // reset the board the instant the WiFi radio powers up. This is a diagnostic
  // band-aid — the real fix is a solid 5V supply / good data cable (see notes).
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(1500);                          // let USB-serial settle so we don't miss the banner
  Serial.println("\n\n=== Firmware booting ===");
  Serial.printf("Reset reason logged above. Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.flush();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(WATER_PIN, INPUT);
  digitalWrite(RELAY_PIN, HIGH);   // pump OFF at boot

  dht.begin();

  connectWiFi();

  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email     = USER_EMAIL;
  auth.user.password  = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;  // from TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Open a realtime stream on the control node. Firebase will PUSH changes to
  // mode/manualPump the instant the dashboard writes them — no slow polling.
  if (!Firebase.RTDB.beginStream(&streamFbdo, "/wateringSystem"))
    Serial.printf("Stream begin failed: %s\n", streamFbdo.errorReason().c_str());
}

void loop() {
  // --- Receive control changes instantly via the stream ---
  if (!Firebase.RTDB.readStream(&streamFbdo))
    Serial.printf("readStream error: %s\n", streamFbdo.errorReason().c_str());

  if (streamFbdo.streamTimeout())
    Serial.println("Stream timed out, resuming...");

  if (streamFbdo.streamAvailable()) {
    String path = streamFbdo.dataPath();
    if (path == "/mode") {
      mode = streamFbdo.stringData();
      Serial.printf("Control: mode -> %s\n", mode.c_str());
    } else if (path == "/manualPump") {
      manualPump = streamFbdo.boolData();
      Serial.printf("Control: manualPump -> %s\n", manualPump ? "ON" : "OFF");
    } else if (path == "/") {
      // Initial snapshot of the whole node — pull the control values out of it.
      FirebaseJson *json = streamFbdo.to<FirebaseJson *>();
      FirebaseJsonData r;
      if (json && json->get(r, "mode"))       mode = r.to<String>();
      if (json && json->get(r, "manualPump")) manualPump = r.to<bool>();
    }
  }

  // --- Read sensors ---
  int  soil  = digitalRead(SOIL_PIN);    // HIGH = DRY
  int  water = digitalRead(WATER_PIN);   // HIGH = OK

  // --- Decide pump state ---
  bool wantPump;
  if (mode == "manual") {
    wantPump = manualPump;
  } else {
    // AUTO: water when soil is dry AND the tank still has water
    wantPump = (soil == HIGH && water == HIGH);
  }
  digitalWrite(RELAY_PIN, wantPump ? LOW : HIGH);  // relay is active-LOW
  pumpState = wantPump;

  // --- Push readings to Firebase on an interval ---
  if (Firebase.ready() && millis() - lastPush > PUSH_INTERVAL) {
    lastPush = millis();

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();

    int   adc            = analogRead(BATTERY_PIN);
    float adcVoltage     = (adc / 4095.0) * 3.3;
    float batteryVoltage = adcVoltage * 2.0;       // 100k + 100k divider

    // Digital sensors mapped to percentages the dashboard's gauges expect.
    // (See note in README about upgrading soil to an analog pin.)
    int soilPercent = (soil  == HIGH) ? 20 : 70;   // DRY -> below 35% threshold
    int tankPercent = (water == HIGH) ? 80 : 10;   // LOW -> triggers refill warning

    if (!isnan(temp)) Firebase.RTDB.setFloat(&fbdo, "/wateringSystem/temperature", temp);
    if (!isnan(hum))  Firebase.RTDB.setFloat(&fbdo, "/wateringSystem/humidity", hum);
    Firebase.RTDB.setInt  (&fbdo, "/wateringSystem/soil",       soilPercent);
    Firebase.RTDB.setInt  (&fbdo, "/wateringSystem/waterLevel", tankPercent);
    Firebase.RTDB.setFloat(&fbdo, "/wateringSystem/battery",    batteryVoltage);
    Firebase.RTDB.setFloat(&fbdo, "/wateringSystem/solar",      0);  // no solar sensor yet
    Firebase.RTDB.setBool (&fbdo, "/wateringSystem/pump",       pumpState);

    // Heartbeat: server-side timestamp the dashboard uses to detect if the
    // device is online. Stops updating the moment the ESP32 loses power/WiFi.
    Firebase.RTDB.setTimestamp(&fbdo, "/wateringSystem/lastSeen");

    Serial.printf("soil=%d%% tank=%d%% temp=%.1f hum=%.0f batt=%.2fV pump=%s mode=%s\n",
                  soilPercent, tankPercent, temp, hum, batteryVoltage,
                  pumpState ? "ON" : "OFF", mode.c_str());
  }

  delay(20);   // keep the loop tight so the stream + relay react quickly
}
