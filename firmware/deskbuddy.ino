/*
  DeskBuddy — Desk Companion
  ESP32 with SSD1306 OLED, DHT22, LDR, and a button.

  WIRING:
    SSD1306 OLED (I2C, 128x64):
      VCC -> 3.3V, GND -> GND
      SDA -> GPIO 21, SCL -> GPIO 22
    DHT22:
      DATA -> GPIO 4 (10k pullup to 3.3V), VCC -> 3.3V, GND -> GND
    LDR (light sensor):
      Voltage divider: LDR from 3.3V to GPIO35, 10kΩ from GPIO35 to GND
    Break button:
      One leg -> GPIO 0, other leg -> GND (uses INPUT_PULLUP)

  LIBRARIES (Arduino IDE > Manage Libraries):
    - "Adafruit SSD1306" by Adafruit
    - "Adafruit GFX Library" by Adafruit
    - "DHT sensor library" by Adafruit
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL = "http://192.168.1.100:5000/api/ingest";

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define LDR_PIN 35
#define BTN_PIN 0
#define BREAK_INTERVAL_MIN 25  // Pomodoro-style

unsigned long lastBreakTime = 0;
unsigned long sessionStart = 0;
bool breakMode = false;

const char* getBreakMessage(int minutesWorked) {
  if (minutesWorked < 25) return "Focus mode";
  if (minutesWorked < 30) return "Break soon!";
  if (minutesWorked < 45) return "Take a break";
  return "REST NOW!";
}

void updateDisplay(float temp, float hum, int light, int minWorked) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Line 1: temp + humidity
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("%.1fC  %.0f%%RH  L:%d", temp, hum, light / 40);

  // Separator
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  if (breakMode) {
    // Break screen
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print("BREAK!");
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print("Press button to resume");
  } else {
    // Work screen — big timer
    display.setTextSize(3);
    display.setCursor(20, 18);
    display.printf("%02d:%02d", minWorked, 0);
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.print(getBreakMessage(minWorked));
    display.setCursor(90, 52);
    display.printf("btn=brk");
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(BTN_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("  DeskBuddy booting...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastBreakTime = millis();
  sessionStart = millis();

  while (WiFi.status() != WL_CONNECTED) { delay(200); }
  Serial.println("DeskBuddy online: " + WiFi.localIP().toString());
}

unsigned long lastPost = 0;
void loop() {
  // Button: toggle break mode
  if (digitalRead(BTN_PIN) == LOW) {
    delay(50);  // debounce
    if (digitalRead(BTN_PIN) == LOW) {
      breakMode = !breakMode;
      if (!breakMode) {
        lastBreakTime = millis();
        sessionStart = millis();
      }
      delay(300);
    }
  }

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int light = analogRead(LDR_PIN);

  unsigned long elapsed = (millis() - sessionStart) / 60000;
  updateDisplay(isnan(temperature) ? 0 : temperature, isnan(humidity) ? 0 : humidity, light, elapsed);

  // POST to dashboard every 10s
  if (millis() - lastPost > 10000 && WiFi.status() == WL_CONNECTED) {
    lastPost = millis();
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    String payload = "{";
    payload += "\"temperature\":" + String(isnan(temperature) ? 0 : temperature, 1) + ",";
    payload += "\"humidity\":" + String(isnan(humidity) ? 0 : humidity, 1) + ",";
    payload += "\"light\":" + String(light) + ",";
    payload += "\"minutes_worked\":" + String(elapsed) + ",";
    payload += "\"break_mode\":" + String(breakMode ? "true" : "false");
    payload += "}";
    http.POST(payload);
    http.end();
  }

  delay(500);
}
