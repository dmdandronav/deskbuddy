"""
Hardware Template — Dashboard backend
---------------------------------------
This Flask app is the "brain" that sits on your laptop during the hackathon.

  - Your ESP32 SENSOR NODE (firmware/sensor_node.ino) POSTs readings here
    every couple seconds -> /api/ingest
  - This dashboard (/) shows a live chart of those readings
  - Optionally, if you also have an ESP32 SERVO CONTROLLER
    (firmware/servo_controller.ino) on your network, this dashboard can send
    it commands -> /api/command -> forwarded to the ESP32's own tiny web server

This mirrors the GhostVision pattern (ESP32 + sensor -> local Flask app,
no cloud) and the DUM-E pattern (Flask server bridging an LLM/UI to an
Arduino/ESP32 that drives servos).
"""

import os
import time
from collections import deque

import requests
from flask import Flask, request, jsonify, render_template
from flask_cors import CORS
from dotenv import load_dotenv

load_dotenv()

app = Flask(__name__)
CORS(app)

# Keep the last N readings in memory — plenty for a live demo chart.
HISTORY = deque(maxlen=200)

# Set this to your servo-controller ESP32's IP, e.g. "http://192.168.1.55"
ESP32_SERVO_URL = os.environ.get("ESP32_SERVO_URL")

# Optional: simple alert thresholds. Tune per project (e.g. distance_cm for
# presence/fall detection, temperature for environment monitoring).
ALERT_RULES = {
    # "distance_cm": {"below": 10, "message": "Object very close!"},
}


@app.route("/")
def index():
    return render_template("index.html", esp32_configured=bool(ESP32_SERVO_URL))


@app.route("/api/ingest", methods=["POST"])
def ingest():
    """ESP32 sensor node POSTs JSON readings here, e.g.:
    { "temperature": 22.4, "humidity": 41.0, "distance_cm": 87.3 }
    """
    data = request.get_json(force=True)
    data["timestamp"] = time.time()

    alerts = []
    for key, rule in ALERT_RULES.items():
        if key in data:
            if "below" in rule and data[key] < rule["below"]:
                alerts.append(rule["message"])
            if "above" in rule and data[key] > rule["above"]:
                alerts.append(rule["message"])
    data["alerts"] = alerts

    HISTORY.append(data)
    return jsonify({"status": "ok", "alerts": alerts})


@app.route("/api/latest")
def latest():
    return jsonify(list(HISTORY))


@app.route("/api/command", methods=["POST"])
def command():
    """Forward a simple command to the servo-controller ESP32.
    Body: { "angle": 90 }
    """
    if not ESP32_SERVO_URL:
        return jsonify({"error": "Set ESP32_SERVO_URL in .env to enable this"}), 400

    angle = request.get_json(force=True).get("angle", 90)
    try:
        r = requests.get(f"{ESP32_SERVO_URL}/move", params={"angle": angle}, timeout=3)
        return jsonify(r.json())
    except Exception as e:
        return jsonify({"error": f"Couldn't reach ESP32: {e}"}), 502


if __name__ == "__main__":
    # host="0.0.0.0" so devices on your local network (the ESP32) can reach it
    app.run(host="0.0.0.0", debug=True, port=5000)
