/*
  Servo Controller — ESP32
  ---------------------------
  Runs a tiny web server on the ESP32 itself. Your Flask dashboard (or an
  LLM, via the dashboard's /api/command route) sends an HTTP request like
  http://<esp32-ip>/move?angle=90 and this board moves a servo to that angle.

  This is the "DUM-E" pattern from Hack the North 2025: an LLM decides WHAT
  to do, a Flask server on a Pi/laptop translates that into a simple HTTP
  call, and the microcontroller just executes motor commands. Splitting it
  this way means you can swap in a totally different "brain" (LLM, voice
  command, gesture from Template 2) without touching this firmware.

  WIRING:
    Servo signal wire -> GPIO 13
    Servo power: for a single SG90, the ESP32's 5V pin can usually handle it.
    For 2+ servos or larger MG996R servos, use a SEPARATE 5V power supply for
    the servos (share GND with the ESP32) — large servos can pull 1-2A each
    and will brown out the board if powered from it directly. The Hack the
    North DUM-E team specifically ran into this with a 5-servo arm.

  LIBRARIES TO INSTALL:
    - "ESP32Servo" by Kevin Harrington / madhephaestus

  ADAPT THIS FOR YOUR PROMPT:
    - Add more servos: more Servo objects, more pins, more query params
      (?pan=90&tilt=45) for a pan-tilt camera mount or multi-joint arm.
    - Add /grip?state=open|closed for a claw end-effector.
    - This same WebServer pattern can drive a relay, LED strip, or motor
      driver instead of a servo — just change handleMove().
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ---- CONFIGURE THESE ----
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// --------------------------

#define SERVO_PIN 13

WebServer server(80);
Servo myServo;

void handleMove() {
  if (!server.hasArg("angle")) {
    server.send(400, "application/json", "{\"error\":\"missing 'angle' query param\"}");
    return;
  }
  int angle = server.arg("angle").toInt();
  angle = constrain(angle, 0, 180);
  myServo.write(angle);

  String response = "{\"status\":\"ok\",\"angle\":" + String(angle) + "}";
  server.send(200, "application/json", response);
}

void handleRoot() {
  server.send(200, "text/plain", "Servo controller online. Try /move?angle=90");
}

void setup() {
  Serial.begin(115200);

  myServo.setPeriodHertz(50);       // standard 50Hz servo
  myServo.attach(SERVO_PIN, 500, 2400); // typical SG90/MG996R pulse range

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! Servo controller at: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Put this IP in dashboard/.env as ESP32_SERVO_URL");

  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.begin();
}

void loop() {
  server.handleClient();
}
