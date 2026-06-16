# DeskBuddy

A small always-on desk gadget that shows you what your body already knows but your phone keeps interrupting: how long you've been sitting, whether the room is getting stuffy, and a gentle nudge when it's time to stand up.

The OLED is the interface. No app to open, no notification to dismiss — just a glanceable 128×64 pixel display that's always on and always current.

---

## What it does

- **Focus timer** — counts minutes worked in the current session, Pomodoro-style (25-minute target)
- **Break nudges** — escalating messages on the OLED as work time grows: *Focus mode* → *Break soon!* → *Take a break* → *REST NOW!*
- **Break mode** — press the button to toggle a "BREAK" splash screen; timer resets when you resume
- **Ambient sensing** — DHT22 reads temperature and humidity; LDR reads room brightness, all shown on line 1 of the display
- **Dashboard** — Flask web app on your laptop shows a live focus-timer ring, environment meter bars, alert banners, and a Chart.js history graph
- **AI nudges** — click "Get Nudge" on the dashboard to get a GPT-generated one-sentence wellness tip tuned to your current work time, temperature, and light level

---

## Hardware

### Parts

| Part | Notes |
|------|-------|
| ESP32 dev board | Any common 38-pin variant works |
| SSD1306 OLED 128×64 (I2C) | 0.96" or 1.3" both work at 0x3C |
| DHT22 temperature/humidity sensor | Needs a 10 kΩ pull-up on DATA |
| LDR (photoresistor) + 10 kΩ resistor | For the voltage-divider light sensor |
| Momentary push button | Break toggle |
| USB power bank or USB-C supply | Powers the whole gadget |

### Wiring

```
SSD1306 OLED (I2C, 128×64)
  VCC  → 3.3V
  GND  → GND
  SDA  → GPIO 21
  SCL  → GPIO 22

DHT22
  VCC  → 3.3V
  GND  → GND
  DATA → GPIO 4   (add 10 kΩ pull-up to 3.3V if your module lacks one)

LDR light sensor (voltage divider)
  3.3V → LDR → GPIO 35 → 10 kΩ → GND
  (GPIO 35 reads the mid-point voltage; higher ADC = brighter room)

Break button
  GPIO 0 → button → GND   (firmware uses INPUT_PULLUP)
```

The **OLED is the primary interface**. It shows sensor data and the focus timer constantly — no phone, no laptop screen needed to check your work state. The dashboard on your laptop is for the longer history graph and AI nudges.

---

## Why not just use a phone app?

| Phone app | DeskBuddy |
|-----------|-----------|
| Screen off by default | Always on |
| Requires an unlock | Zero friction — eyes up, read, eyes down |
| App must be foregrounded | Persistent ambient display |
| Relies on phone sensors (no room temp) | Physical sensors in your workspace |
| Notification = interruption | Glance = information |

The key insight from building this: the friction of picking up a phone and unlocking it is enough to make you ignore break reminders entirely. An always-visible display on the desk removes that friction completely.

---

## Setup

### 1. Flash the ESP32

1. Open `firmware/deskbuddy.ino` in the Arduino IDE
2. Install the required libraries (Tools → Manage Libraries):
   - **Adafruit SSD1306** by Adafruit
   - **Adafruit GFX Library** by Adafruit
   - **DHT sensor library** by Adafruit
3. Edit the three config lines at the top of the sketch:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_NAME";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   const char* SERVER_URL    = "http://<YOUR_LAPTOP_IP>:5000/api/ingest";
   ```
   Find your laptop's local IP with `ifconfig` (Mac/Linux) or `ipconfig` (Windows).
4. Select your ESP32 board (Tools → Board → esp32 → ESP32 Dev Module) and the correct port, then Upload
5. Open the Serial Monitor at 115200 baud — you should see `DeskBuddy online: 192.168.x.x`

### 2. Run the dashboard

```bash
cd dashboard
python -m venv venv && source venv/bin/activate   # Windows: venv\Scripts\activate
pip install -r requirements.txt
cp .env.example .env
# Edit .env and add your OPENAI_API_KEY for /api/nudge
python app.py
```

Open `http://localhost:5000` in a browser. Readings appear within 10 seconds of the ESP32 booting.

---

## Alert thresholds

Edit `ALERT_RULES` in `dashboard/app.py` to tune when the dashboard shows warning banners:

```python
ALERT_RULES = {
    "minutes_worked": {"above": 50, "message": "You've been at it for 50 minutes — time for a proper break!"},
    "temperature":    {"above": 27, "message": "Room is warm — consider cracking a window."},
}
```

---

## Troubleshooting

- **OLED stays blank** — check I2C address; most SSD1306 modules are `0x3C` but some are `0x3D`. Scan with an I2C scanner sketch.
- **DHT22 returns NaN** — add or check the 10 kΩ pull-up resistor on the DATA line.
- **ESP32 can't reach the dashboard** — campus/venue WiFi often blocks device-to-device traffic. Use your phone's hotspot instead; it's the most reliable option at hackathons.
- **Light readings always 0 or 4095** — verify the voltage divider: LDR and the 10 kΩ resistor should meet at GPIO 35, with LDR going to 3.3V and resistor going to GND.
- **AI nudge says "Set OPENAI_API_KEY"** — copy `.env.example` to `.env` and paste in a valid key.
