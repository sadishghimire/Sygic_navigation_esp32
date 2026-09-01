# ESP32 Multipurpose HUD & Mini Weather Station (0.96" OLED)

A versatile, multi-mode firmware for **ESP32** integrating **Sygic GPS Navigation BLE HUD**, an environmental **Mini Weather Station** (DHT11 & BMP180), digital stopwatch & clock, WiFi scanner, system hardware monitor, and screen flashlight — switchable via a **single tactile push button**.

---

## 📱 Available Modes

Cycle through 6 switchable modes using the tactile button:

| Mode # | Mode Name | Description & Key Features | Button Controls |
|---|---|---|---|
| **1** | **Sygic Navigation HUD** | Full Sygic Smart HUD emulation: Real-time turn arrows (left, right, slight, sharp, keep, u-turn, straight), speed in km/h, distance & road names. *(BLE stays active in background across all modes)* | **Short Press**: Next mode |
| **2** | **🌤️ Mini Weather Station** | Live environmental monitoring: Ambient Temperature (°C/°F), Relative Humidity (% RH), Barometric Pressure (hPa), and Estimated Altitude (m/ft) with air comfort status. | **Short Press**: Next mode<br>**Long Press**: Toggle Metric (°C/m) & Imperial (°F/ft) |
| **3** | **Clock & Stopwatch** | Live uptime digital clock and high-precision stopwatch with tenths of a second. | **Short Press**: Next mode<br>**Long Press**: Start / Pause / Reset |
| **4** | **WiFi Scanner & RSSI** | Real-time 2.4 GHz WiFi discovery showing SSID list, RSSI signal strength in dBm, and total networks found. | **Short Press**: Next mode<br>**Long Press**: Trigger immediate rescan |
| **5** | **System Monitor** | Live ESP32 diagnostics: CPU frequency (MHz), Free RAM heap, Sensor connectivity status (`DHT+ BMP+`), Flash size, and BLE status. | **Short Press**: Next mode |
| **6** | **Screen Torch** | Turns the 0.96" OLED display into an all-white pocket flashlight. | **Short Press**: Next mode |

---

## 🛠️ Hardware Wiring & Pinout

### Components Needed
1. **ESP32 Development Board** (NodeMCU-32S / ESP32-WROOM-32)
2. **0.96" I2C OLED Display** (SSD1306, 128x64 pixels, address `0x3C`)
3. **BMP180 Barometric Pressure & Altitude Sensor** (I2C)
4. **DHT11 Temperature & Humidity Sensor** (1-Wire Digital)
5. **1x Tactile Push Button / Switch**
6. Jumper wires & breadboard

### Wiring Table

| Component | Pin | ESP32 GPIO | Notes |
|---|---|---|---|
| **OLED Display** | VCC | 3.3V or 5V | Power |
| | GND | GND | Ground |
| | SDA | **GPIO 21** | I2C Data (Shared with BMP180) |
| | SCL | **GPIO 22** | I2C Clock (Shared with BMP180) |
| **BMP180 Sensor** | VCC | 3.3V | Power |
| | GND | GND | Ground |
| | SDA | **GPIO 21** | Connects to same I2C SDA as OLED |
| | SCL | **GPIO 22** | Connects to same I2C SCL as OLED |
| **DHT11 Sensor** | VCC | 3.3V or 5V | Power |
| | GND | GND | Ground |
| | DATA / OUT | **GPIO 4** | 1-Wire Digital Pin (Configurable `#define DHT_PIN 4`) |
| **Tactile Button** | Pin 1 | **GPIO 18** | Input (uses internal `INPUT_PULLUP`) |
| | Pin 2 | **GND** | Ground (no external resistor needed) |

---

## 📦 Required Arduino IDE Libraries

Install the following libraries via the Arduino IDE Library Manager (**Sketch > Include Library > Manage Libraries...**):

1. **Adafruit SSD1306** (by Adafruit)
2. **Adafruit GFX Library** (by Adafruit)
3. **Adafruit BMP085 Library** (by Adafruit — works with BMP180 & BMP085)
4. **DHT sensor library** (by Adafruit)
5. **Adafruit Unified Sensor** (dependency for DHT)
6. **ESP32 Board Package** (includes `BLEDevice.h` and `WiFi.h`)

---

## 🎮 Button Navigation

- **Short Press (< 700 ms)**: Cycles sequentially to the next mode and shows an on-screen mode banner.
- **Long Press (> 700 ms)**:
  - **In Weather Mode**: Toggles between Metric (°C, meters) and Imperial (°F, feet).
  - **In Stopwatch Mode**: Starts &rarr; Pauses &rarr; Resets the stopwatch.
  - **In WiFi Scanner Mode**: Triggers a new 2.4 GHz WiFi scan.

---

## 📲 Sygic BLE Navigation Connection

1. Flash the sketch to your ESP32.
2. Open **Sygic GPS Navigation** on your smartphone.
3. Start a route and turn on **Head-up Display (HUD)** / Bluetooth HUD under Sygic's settings.
4. The ESP32 connects automatically as **ESP32 HUD**.
5. You can switch to Weather, Clock, or WiFi scanner modes anytime; navigation packets continue to be processed in the background!

---

## 📜 License

This project is open source and available under the [MIT License](LICENSE).
