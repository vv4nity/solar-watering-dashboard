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

FirebaseData fbdo;        // for writes
FirebaseData ctrlFbdo;   // for reading control values
FirebaseAuth auth;
FirebaseConfig config;

bool pumpState = false;

// Control values mirrored from the dashboard
String mode       = "auto";   // "auto" | "manual"
bool   manualPump = false;

unsigned long lastPush = 0;
const unsigned long PUSH_INTERVAL = 2000;  // ms

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(WATER_PIN, INPUT);
  digitalWrite(RELAY_PIN, HIGH);   // pump OFF at boot

  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.printf("\nConnected. IP: %s\n", WiFi.localIP().toString().c_str());

  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email     = USER_EMAIL;
  auth.user.password  = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;  // from TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // --- Read control state from the dashboard ---
  if (Firebase.ready()) {
    if (Firebase.RTDB.getString(&ctrlFbdo, "/wateringSystem/mode"))
      mode = ctrlFbdo.stringData();
    if (Firebase.RTDB.getBool(&ctrlFbdo, "/wateringSystem/manualPump"))
      manualPump = ctrlFbdo.boolData();
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

  delay(100);
}
