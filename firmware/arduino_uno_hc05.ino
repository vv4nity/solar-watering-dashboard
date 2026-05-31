/* ============================================================
   Solar-Powered Automated Watering System — Arduino UNO + HC-05
   ------------------------------------------------------------
   The UNO cannot reach the internet, so it streams sensor data
   over an HC-05 Bluetooth module to an Android phone. An MIT
   App Inventor app on the phone forwards the data to Firebase,
   which feeds the Vercel dashboard. See ANDROID_GATEWAY_SETUP.md.

   Data sent every 2s as one CSV line ending in '\n':
       soil,temperature,humidity,tank,battery,pump
   e.g.  20,28.5,65,80,3.90,0

   Libraries (Arduino IDE → Library Manager):
     - "DHT sensor library" (by Adafruit)
     - "Adafruit Unified Sensor"
   (SoftwareSerial ships with the IDE.)
   ============================================================ */

#include <SoftwareSerial.h>
#include <DHT.h>

/* ---------- Pins ---------- */
#define DHTPIN      2
#define DHTTYPE     DHT11
#define SOIL_PIN    7    // digital soil sensor: HIGH = DRY, LOW = WET
#define WATER_PIN   8    // digital tank sensor: HIGH = OK,  LOW = LOW
#define BATTERY_PIN A0   // 18650 via 100k+100k divider
#define RELAY_PIN   9    // active-LOW relay (LOW = pump ON)

// HC-05: module TX -> Uno pin 10 (RX); Uno pin 11 (TX) -> module RX VIA DIVIDER.
SoftwareSerial BT(10, 11);   // RX, TX
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastPush = 0;
const unsigned long PUSH_INTERVAL = 2000;   // ms

void setup() {
  Serial.begin(9600);      // USB serial, for debugging
  BT.begin(9600);          // HC-05 default data-mode baud (try 38400 if garbled)

  pinMode(SOIL_PIN, INPUT);
  pinMode(WATER_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);   // pump OFF at boot

  dht.begin();
  Serial.println(F("UNO + HC-05 watering node started."));
}

void loop() {
  int soil  = digitalRead(SOIL_PIN);    // HIGH = DRY
  int water = digitalRead(WATER_PIN);   // HIGH = OK

  // AUTO control runs locally on the UNO: water when soil is dry AND the
  // tank still has water. (Remote MANUAL control would need a reverse
  // Bluetooth path — see the "Phase 2" note in ANDROID_GATEWAY_SETUP.md.)
  bool wantPump = (soil == HIGH && water == HIGH);
  digitalWrite(RELAY_PIN, wantPump ? LOW : HIGH);   // relay is active-LOW

  if (millis() - lastPush > PUSH_INTERVAL) {
    lastPush = millis();

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (isnan(temp)) temp = 0;
    if (isnan(hum))  hum  = 0;

    int   adc  = analogRead(BATTERY_PIN);        // 0..1023 over 0..5V
    float batt = (adc / 1023.0) * 5.0 * 2.0;     // x2 for the 100k+100k divider

    // Map the digital sensors to the percentages the dashboard gauges expect.
    int soilPercent = (soil  == HIGH) ? 20 : 70; // DRY -> below 35% threshold
    int tankPercent = (water == HIGH) ? 80 : 10; // LOW -> triggers refill warning

    // One CSV line: soil,temperature,humidity,tank,battery,pump
    BT.print(soilPercent);      BT.print(',');
    BT.print(temp, 1);          BT.print(',');
    BT.print(hum, 0);           BT.print(',');
    BT.print(tankPercent);      BT.print(',');
    BT.print(batt, 2);          BT.print(',');
    BT.print(wantPump ? 1 : 0);
    BT.print('\n');

    // Same clean CSV on USB serial, so pc_bridge.py works over a USB cable too
    // (not just the HC-05 Bluetooth port). No prefix — keep it machine-readable.
    Serial.print(soilPercent);  Serial.print(',');
    Serial.print(temp, 1);      Serial.print(',');
    Serial.print(hum, 0);       Serial.print(',');
    Serial.print(tankPercent);  Serial.print(',');
    Serial.print(batt, 2);      Serial.print(',');
    Serial.println(wantPump ? 1 : 0);
  }
}
