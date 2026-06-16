#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Configuration
const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
const char* FLASK_URL = "http://192.168.1.100:5000/api/ingest";

// Pins
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define LDR_PIN 34
#define BUTTON_PIN 0   // Boot button or GPIO0
#define LED_PIN 2      // Built-in LED

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

// State
unsigned long lastBreakMillis = 0;
const unsigned long BREAK_INTERVAL = 25 * 60 * 1000UL; // 25 minutes
bool nudgeActive = false;
unsigned long lastPost = 0;
const unsigned long POST_INTERVAL = 5000;

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 not found");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("DeskBuddy");
  display.println("Starting...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  display.clearDisplay();
  display.println("WiFi OK");
  display.println(WiFi.localIP().toString());
  display.display();
  delay(1500);

  lastBreakMillis = millis();
}

void loop() {
  // Check break button (active low with pullup)
  if (digitalRead(BUTTON_PIN) == LOW) {
    lastBreakMillis = millis();
    nudgeActive = false;
    digitalWrite(LED_PIN, LOW);
    delay(200); // debounce
  }

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int lightRaw = analogRead(LDR_PIN);
  int lightPercent = map(lightRaw, 0, 4095, 0, 100);

  unsigned long elapsed = millis() - lastBreakMillis;
  int minutesSinceBreak = elapsed / 60000;
  int secondsLeft = (BREAK_INTERVAL - elapsed) / 1000;
  int minutesLeft = secondsLeft / 60;

  // Nudge when overdue
  if (elapsed > BREAK_INTERVAL && !nudgeActive) {
    nudgeActive = true;
    digitalWrite(LED_PIN, HIGH);
  }

  // Update OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (nudgeActive) {
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.println("Take a");
    display.println("break!");
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.println("Press button to reset");
  } else {
    if (!isnan(temperature)) {
      display.printf("Temp: %.1fC  Hum: %.0f%%\n", temperature, humidity);
    } else {
      display.println("Sensor error");
    }
    display.printf("Light: %d%%\n", lightPercent);
    display.printf("Break in: %d min\n", minutesLeft);
    display.println("---");
    display.printf("Since break: %d min\n", minutesSinceBreak);
  }

  display.display();

  // Post to Flask every 5 seconds
  if (millis() - lastPost > POST_INTERVAL && WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(FLASK_URL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["device"] = "deskbuddy";
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["light"] = lightPercent;
    doc["minutes_since_break"] = minutesSinceBreak;
    doc["nudge_active"] = nudgeActive;

    String body;
    serializeJson(doc, body);
    http.POST(body);
    http.end();
    lastPost = millis();
  }

  delay(500);
}
