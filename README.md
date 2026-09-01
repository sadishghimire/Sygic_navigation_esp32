# ESP32 Sygic Navigation BLE HUD (0.96" OLED)

A turn-by-turn Head-Up Display (HUD) navigation receiver for **ESP32** that connects to the **Sygic GPS Navigation** mobile app via Bluetooth Low Energy (BLE). Real-time navigation arrows, current speed, and distance/instruction messages are displayed on a **0.96-inch I2C OLED display (SSD1306 128x64)**.

---

## 🚀 Features

- **Sygic Smart HUD Protocol Emulation**: Connects automatically to the Sygic mobile application using standard BLE HUD service UUIDs.
- **Visual Turn-by-Turn Arrows**:
  - Left / Right
  - Slight Left / Slight Right (Easy)
  - Sharp Left / Sharp Right
  - Keep Left / Keep Right
  - Straight / Follow / Motorway / Tunnel
  - U-Turn
- **Speed Display**: Shows current speed (in km/h).
- **Navigation Instructions & Distance**: Displays turn instructions and distance remaining (supports multi-line auto-splitting).
- **Keep-Alive Protocol**: Requests navigation updates periodically if idle.

---

## 🛠️ Hardware Requirements

- **Microcontroller**: ESP32 Development Board (e.g., ESP32 NodeMCU / ESP32-WROOM-32)
- **Display**: 0.96" I2C OLED Display Module (SSD1306 driver, 128x64 resolution, 0x3C I2C address)
- Jumper wires and breadboard / custom PCB

---

## 📌 Wiring Diagram

| 0.96" OLED Pin | ESP32 Pin | Description |
|---|---|---|
| **VCC** | 3.3V / 5V | Power Supply |
| **GND** | GND | Ground |
| **SDA** | GPIO 21 | I2C Data |
| **SCL** | GPIO 22 | I2C Clock |

---

## 📦 Required Arduino Libraries

Install the following libraries via Arduino IDE Library Manager (**Sketch > Include Library > Manage Libraries...**):

1. **Adafruit SSD1306** (by Adafruit)
2. **Adafruit GFX Library** (by Adafruit)
3. **ESP32 BLE Arduino** (built-in with the ESP32 board package)

---

## 📲 How to Use with Sygic

1. **Flash the Code**: Open `Navigation_code.ino` in Arduino IDE, select your ESP32 board, and upload.
2. **Open Serial Monitor**: Set baud rate to `115200` to observe debug messages.
3. **Launch Sygic**:
   - Open **Sygic GPS Navigation** on your smartphone (Android or iOS).
   - Start a route / navigation.
   - Go to Sygic's menu or Add-ons and enable **Head-up Display (HUD)** / Bluetooth HUD.
   - The ESP32 will automatically connect as **ESP32 HUD**, and navigation directions will show up on the 0.96" OLED screen.

---

## 📜 License

This project is open source and available under the [MIT License](LICENSE).
