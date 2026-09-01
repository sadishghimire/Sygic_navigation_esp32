# ESP32 Multipurpose Navigation HUD & Multi-Tool (0.96" OLED)

A versatile multi-tool firmware for **ESP32** featuring real-time **Sygic GPS Navigation BLE HUD**, digital stopwatch & clock, WiFi scanner & signal meter, system hardware monitor, and screen flashlight — easily switchable via a **tactile push button**.

---

## 📱 Available Modes

You can switch between 5 distinct modes at any time using a single tactile switch:

| Mode # | Mode Name | Description & Key Features | Button Actions |
|---|---|---|---|
| **1** | **Sygic Navigation HUD** | Full Sygic Smart HUD emulation: Real-time turn arrows (left, right, slight, sharp, keep, u-turn, straight), speed in km/h, distance & road names. *(BLE stays active in background across all modes)* | **Short Press**: Switch mode |
| **2** | **Clock & Stopwatch** | Live uptime digital clock and high-precision stopwatch with tenths of a second. | **Short Press**: Switch mode<br>**Long Press**: Start / Pause / Reset |
| **3** | **WiFi Scanner & RSSI** | Real-time 2.4 GHz WiFi discovery showing SSID list, RSSI signal strength in dBm, and total networks found. | **Short Press**: Switch mode<br>**Long Press**: Trigger immediate rescan |
| **4** | **System Monitor** | Live ESP32 hardware diagnostics: CPU frequency (MHz), Free RAM heap, Min heap, Flash size, and BLE connection status. | **Short Press**: Switch mode |
| **5** | **Screen Torch** | Turns the 0.96" OLED display into an all-white pocket flashlight. | **Short Press**: Switch mode |

---

## 🛠️ Hardware Requirements & Pinout

### Required Components
- **ESP32 Development Board** (NodeMCU-32S, ESP32-WROOM-32, etc.)
- **0.96" I2C OLED Display** (SSD1306, 128x64 pixels, 0x3C I2C address)
- **1x Tactile Push Button / Switch**
- Jumper wires & breadboard

### Wiring Table

| Component Pin | ESP32 GPIO Pin | Description / Notes |
|---|---|---|
| **OLED VCC** | 3.3V or 5V | Power supply |
| **OLED GND** | GND | Ground |
| **OLED SDA** | **GPIO 21** | I2C Data line |
| **OLED SCL** | **GPIO 22** | I2C Clock line |
| **Tactile Switch (Pin 1)** | **GPIO 18** | Input pin (uses internal `INPUT_PULLUP`) |
| **Tactile Switch (Pin 2)** | **GND** | Ground (no external resistor required) |

> [!TIP]
> To use a different button GPIO, simply modify `#define BUTTON_PIN 18` near the top of [`Navigation_code.ino`](file:///e:/github/Sygic_navigation_esp32/Navigation_code.ino).

---

## 🎮 How Button Controls Work

- **Short Press (< 700 ms)**: Cycles sequentially to the next mode and presents an on-screen mode banner.
- **Long Press (> 700 ms)**: Performs context-sensitive actions inside the current mode:
  - **In Stopwatch Mode**: Starts timer &rarr; Pauses timer &rarr; Resets timer.
  - **In WiFi Scanner Mode**: Starts an immediate new 2.4 GHz WiFi scan.

---

## 📦 Required Arduino IDE Libraries

Install these libraries via Arduino IDE (**Sketch > Include Library > Manage Libraries...**):
1. **Adafruit SSD1306** (by Adafruit)
2. **Adafruit GFX Library** (by Adafruit)
3. **ESP32 Board Package** (includes `BLEDevice.h` and `WiFi.h`)

---

## 🚀 How to Run & Connect Sygic

1. Open [`Navigation_code.ino`](file:///e:/github/Sygic_navigation_esp32/Navigation_code.ino) in Arduino IDE.
2. Select your ESP32 board and COM port.
3. Click **Upload**.
4. Open the Serial Monitor at `115200` baud.
5. On your smartphone:
   - Launch **Sygic GPS Navigation**.
   - Start any navigation route.
   - Go to Sygic Settings / Add-ons and enable **Head-up Display (HUD)** / Bluetooth HUD.
   - The phone connects to **ESP32 HUD**, and navigation arrows, speed, and instructions will be shown on the OLED.
   - You can press the tactile button anytime to check the stopwatch, scan WiFi, or check system memory without dropping the Sygic BLE connection!

---

## 📜 License

This project is open source and available under the [MIT License](LICENSE).
