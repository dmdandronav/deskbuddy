"""
DeskBuddy — Dashboard backend
-------------------------------
Receives sensor readings from the ESP32 every 5 seconds, stores a rolling
history, checks alert thresholds, and serves the live break-timer dashboard.

Endpoints:
  POST /api/ingest   — ESP32 posts readings here
  GET  /api/latest   — returns full reading history
  GET  /api/status   — returns latest break status and nudge state
  GET  /api/nudge    — context-aware nudge via OpenAI (optional)
"""

import os
import time
from collections import deque

from flask import Flask, request, jsonify, render_template
from flask_cors import CORS
from dotenv import load_dotenv

load_dotenv()

app = Flask(__name__)
CORS(app)

# Keep the last N readings in memory — plenty for a live demo chart.
HISTORY = deque(maxlen=200)

ALERT_RULES = {
    "temperature": {"min": 16, "max": 30, "label": "Room temp"},
    "humidity": {"min": 30, "max": 70, "label": "Humidity"},
    "light": {"min": 10, "max": None, "label": "Light level"},
}


def _latest_reading():
    """Return the most recent reading dict, or sensible defaults."""
    if HISTORY:
        return HISTORY[-1]
    return {
        "temperature": 22.0,
        "humidity": 50.0,
        "light": 50,
        "minutes_since_break": 0,
        "nudge_active": False,
    }


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/ingest", methods=["POST"])
def ingest():
    """ESP32 POSTs JSON readings here every 5 s.
    Example: { "device": "deskbuddy", "temperature": 22.4, "humidity": 41.0,
               "light": 65, "minutes_since_break": 12, "nudge_active": false }
    """
    data = request.get_json(force=True)
    data["timestamp"] = time.time()

    alerts = []
    for key, rule in ALERT_RULES.items():
        if key in data:
            val = data[key]
            if rule.get("min") is not None and val < rule["min"]:
                alerts.append(f"{rule['label']} too low: {val}")
            if rule.get("max") is not None and val > rule["max"]:
                alerts.append(f"{rule['label']} too high: {val}")
    data["alerts"] = alerts

    HISTORY.append(data)
    return jsonify({"status": "ok", "alerts": alerts})


@app.route("/api/latest")
def latest():
    return jsonify(list(HISTORY))


@app.route("/api/status")
def status():
    """Returns the latest break status and nudge state for quick polling."""
    reading = _latest_reading()
    return jsonify({
        "nudge_active": reading.get("nudge_active", False),
        "minutes_since_break": reading.get("minutes_since_break", 0),
        "timestamp": reading.get("timestamp"),
    })


@app.route("/api/nudge")
def nudge():
    """Generate a friendly, context-aware productivity or wellness nudge via OpenAI."""
    reading = _latest_reading()
    minutes = reading.get("minutes_since_break", 0)
    temp = reading.get("temperature", 22.0)
    light_pct = reading.get("light", 50)

    openai_key = os.environ.get("OPENAI_API_KEY")
    if not openai_key:
        return jsonify({"nudge": "Set OPENAI_API_KEY in dashboard/.env to enable AI nudges."}), 200

    try:
        import openai
        client = openai.OpenAI(api_key=openai_key)
        prompt = (
            f"Given that the user has worked for {minutes} minutes since their last break, "
            f"temperature is {temp:.1f}°C, and light level is {light_pct}/100, "
            "generate a single friendly one-sentence productivity nudge or wellness tip."
        )
        response = client.chat.completions.create(
            model="gpt-4o-mini",
            messages=[{"role": "user", "content": prompt}],
            max_tokens=80,
            temperature=0.8,
        )
        nudge_text = response.choices[0].message.content.strip()
        return jsonify({"nudge": nudge_text})
    except Exception as e:
        return jsonify({"nudge": f"Nudge unavailable: {e}"}), 500


if __name__ == "__main__":
    # host="0.0.0.0" so the ESP32 on your local network can reach this server
    app.run(host="0.0.0.0", debug=True, port=5000)
