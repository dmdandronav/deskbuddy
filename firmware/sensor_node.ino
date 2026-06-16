/*
  Sensor Node — ESP32
  ---------------------
  Reads an HC-SR04 ultrasonic sensor (distance/presence) and a DHT22
  temperature/humidity sensor, then POSTs a JSON reading to your laptop's
  Flask dashboard every 2 seconds.

  WIRING (matches common ESP32 starter kits, e.g. DIYables ESP32 Starter Kit):
    HC-SR04   VCC -> 5V        DHT22  VCC -> 3.3V
              GND -> GND              GND -> GND
              TRIG -> GPIO 5           DATA -> GPIO 4 (with 10k pullup to 3.3V)
              ECHO -> GPIO 18

  LIBRARIES TO INSTALL (Arduino IDE > Tools > Manage Libraries):
    - "DHT sensor library" by Adafruit (also installs "Adafruit Unified Sensor")

  ADAPT THIS FOR YOUR PROMPT:
    - Swap HC-SR04 for a PIR motion sensor (HC-SR501) for presence/occupancy
      projects — just read digitalRead() on its OUT pin instead of pulseIn().
    - Add more sensors (light/LDR, gas, sound) — just add more keys to the
      JSON payload. The dashboard automatically charts any numeric field.
    - For a "GhostVision"-style privacy project, pair the distance reading
      with the ALERT_RULES dict in dashboard/app.py (e.g. alert if distance
      drops suddenly = possible fall).
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ---- CONFIGURE THESE ----
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Your laptop's local IP + dashboard port. Find your IP with `ipconfig`
// (Windows) or `ifconfig` / `ip a` (Mac/Linux). Both devices must be on the
// same WiFi network.
const char* SERVER_URL = "http://192.168.1.100:5000/api/ingest";
// --------------------------

#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define TRIG_PIN 5
#define ECHO_PIN 18

float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout (~5m range)
  if (duration == 0) return -1; // out of range / no echo
  return duration * 0.0343 / 2.0;
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float distance = readDistanceCM();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT read failed, skipping this cycle");
    delay(2000);
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"distance_cm\":" + String(distance, 1);
    payload += "}";

    int responseCode = http.POST(payload);
    Serial.printf("POST -> %d  %s\n", responseCode, payload.c_str());
    http.end();
  } else {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  delay(2000);
}
