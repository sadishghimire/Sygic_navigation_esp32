#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// ============================================================
// HARDWARE PIN DEFINITIONS
// ============================================================

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64

// I2C Bus Pins (Shared by SSD1306 OLED & BMP180 Barometer)
#define OLED_SDA       21
#define OLED_SCL       22
#define OLED_ADDRESS   0x3C

// Tactile Switch / Push Button Pin (Internal Pull-Up)
#define BUTTON_PIN     18

// DHT11 Sensor Pin (1-Wire Digital Data)
#define DHT_PIN        4
#define DHTTYPE        DHT11

// ============================================================
// HARDWARE OBJECTS
// ============================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP085 bmp;
DHT dht(DHT_PIN, DHTTYPE);

bool bmpFound = false;

// ============================================================
// SYSTEM MODES
// ============================================================

enum AppMode {
  MODE_NAVIGATION = 0,
  MODE_WEATHER,
  MODE_STOPWATCH,
  MODE_WIFI_SCANNER,
  MODE_SYSTEM_MONITOR,
  MODE_TORCH,
  MODE_COUNT
};

AppMode currentMode = MODE_NAVIGATION;

// Mode change transition banner
unsigned long modeBannerUntil = 0;
const char* modeNames[] = {
  "1. Sygic Nav HUD",
  "2. Weather Station",
  "3. Clock & Timer",
  "4. WiFi Scanner",
  "5. System Monitor",
  "6. Screen Torch"
};

// ============================================================
// WEATHER DATA & UNITS
// ============================================================

struct WeatherData {
  float temperatureC = NAN;
  float temperatureF = NAN;
  float humidity = NAN;
  float pressureHpa = NAN;
  float altitudeMeters = NAN;
  float altitudeFeet = NAN;
  bool dhtValid = false;
  bool bmpValid = false;
};

WeatherData currentWeather;
bool weatherUseImperial = false; // false = Metric (°C, m), true = Imperial (°F, ft)
unsigned long lastWeatherReadTime = 0;
const unsigned long WEATHER_READ_INTERVAL = 2000; // Read sensors every 2 seconds

void updateWeatherSensors() {
  if (millis() - lastWeatherReadTime < WEATHER_READ_INTERVAL && lastWeatherReadTime != 0) {
    return;
  }
  lastWeatherReadTime = millis();

  // 1. Read DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    currentWeather.dhtValid = false;
  } else {
    currentWeather.humidity = h;
    currentWeather.temperatureC = t;
    currentWeather.temperatureF = (t * 1.8f) + 32.0f;
    currentWeather.dhtValid = true;
  }

  // 2. Read BMP180 (Barometer & High-Precision Temp)
  if (bmpFound) {
    float bmpTemp = bmp.readTemperature();
    float bmpPress = bmp.readPressure() / 100.0f; // Pa to hPa
    float bmpAlt = bmp.readAltitude(101325);       // Standard sea level pressure

    currentWeather.pressureHpa = bmpPress;
    currentWeather.altitudeMeters = bmpAlt;
    currentWeather.altitudeFeet = bmpAlt * 3.28084f;
    currentWeather.bmpValid = true;

    // If DHT11 failed, use BMP180 temperature as fallback
    if (!currentWeather.dhtValid) {
      currentWeather.temperatureC = bmpTemp;
      currentWeather.temperatureF = (bmpTemp * 1.8f) + 32.0f;
    }
  } else {
    currentWeather.bmpValid = false;
  }
}

// ============================================================
// BUTTON DEBOUNCE & PRESS DETECTION
// ============================================================

enum ButtonEvent {
  BTN_NONE = 0,
  BTN_SHORT_PRESS,
  BTN_LONG_PRESS
};

const unsigned long DEBOUNCE_MS = 50;
const unsigned long LONG_PRESS_MS = 700;

int lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
bool isLongPressHandled = false;

ButtonEvent readButtonEvent() {
  int reading = digitalRead(BUTTON_PIN);
  ButtonEvent event = BTN_NONE;

  // Button pressed (active LOW with pull-up)
  if (reading == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
    isLongPressHandled = false;
  }
  // Button held down
  else if (reading == LOW && lastButtonState == LOW) {
    if (!isLongPressHandled && (millis() - buttonPressTime >= LONG_PRESS_MS)) {
      isLongPressHandled = true;
      event = BTN_LONG_PRESS;
    }
  }
  // Button released
  else if (reading == HIGH && lastButtonState == LOW) {
    if (!isLongPressHandled && (millis() - buttonPressTime >= DEBOUNCE_MS)) {
      event = BTN_SHORT_PRESS;
    }
  }

  lastButtonState = reading;
  return event;
}

// ============================================================
// SYGIC BLE HUD UUIDs & CONSTANTS
// ============================================================

#define SERVICE_UUID        "DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_INDICATE_UUID  "DD3F0AD2-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_WRITE_UUID     "DD3F0AD3-6239-4E1F-81F1-91F6C9F01D86"

// Sygic directions
#define DIRECTION_NONE          0
#define DIRECTION_START         1
#define DIRECTION_EASY_LEFT     2
#define DIRECTION_EASY_RIGHT    3
#define DIRECTION_END           4
#define DIRECTION_VIA           5
#define DIRECTION_KEEP_LEFT     6
#define DIRECTION_KEEP_RIGHT    7
#define DIRECTION_LEFT          8
#define DIRECTION_OUT_OF_ROUTE  9
#define DIRECTION_RIGHT         10
#define DIRECTION_SHARP_LEFT    11
#define DIRECTION_SHARP_RIGHT   12
#define DIRECTION_STRAIGHT      13
#define DIRECTION_UTURN_LEFT    14
#define DIRECTION_UTURN_RIGHT   15
#define DIRECTION_FERRY         16
#define DIRECTION_BOUNDARY      17
#define DIRECTION_FOLLOW        18
#define DIRECTION_MOTORWAY      19
#define DIRECTION_TUNNEL        20
#define DIRECTION_EXIT_LEFT     21
#define DIRECTION_EXIT_RIGHT    22

// BLE variables
BLEServer* bleServer = nullptr;
BLECharacteristic* indicateCharacteristic = nullptr;
bool deviceConnected = false;
unsigned long lastActivity = 0;

// Navigation live data
uint8_t currentSpeed = 0;
uint8_t currentDirection = DIRECTION_NONE;
String currentMessage = "";
bool hasNavData = false;

// ============================================================
// STOPWATCH & CLOCK STATE
// ============================================================

bool stopwatchRunning = false;
unsigned long stopwatchStartTime = 0;
unsigned long stopwatchAccumulated = 0;

void toggleStopwatch() {
  if (stopwatchRunning) {
    stopwatchAccumulated += (millis() - stopwatchStartTime);
    stopwatchRunning = false;
  } else {
    stopwatchStartTime = millis();
    stopwatchRunning = true;
  }
}

void resetStopwatch() {
  stopwatchRunning = false;
  stopwatchAccumulated = 0;
  stopwatchStartTime = 0;
}

unsigned long getStopwatchElapsed() {
  if (stopwatchRunning) {
    return stopwatchAccumulated + (millis() - stopwatchStartTime);
  }
  return stopwatchAccumulated;
}

// ============================================================
// WIFI SCANNER STATE
// ============================================================

int wifiNetworksFound = -1;
bool isScanningWifi = false;

void triggerWifiScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(10);
  wifiNetworksFound = WiFi.scanNetworks(true); // Async scan
  isScanningWifi = true;
}

// ============================================================
// NAVIGATION DRAWING HELPERS
// ============================================================

void drawLeftArrow() {
  display.drawLine(100, 32, 35, 32, SSD1306_WHITE);
  display.drawLine(35, 32, 55, 12, SSD1306_WHITE);
  display.drawLine(35, 32, 55, 52, SSD1306_WHITE);
}

void drawRightArrow() {
  display.drawLine(28, 32, 93, 32, SSD1306_WHITE);
  display.drawLine(93, 32, 73, 12, SSD1306_WHITE);
  display.drawLine(93, 32, 73, 52, SSD1306_WHITE);
}

void drawStraightArrow() {
  display.drawLine(64, 52, 64, 12, SSD1306_WHITE);
  display.drawLine(64, 12, 45, 31, SSD1306_WHITE);
  display.drawLine(64, 12, 83, 31, SSD1306_WHITE);
}

void drawUTurn() {
  display.drawLine(42, 15, 42, 35, SSD1306_WHITE);
  display.drawLine(42, 35, 48, 45, SSD1306_WHITE);
  display.drawLine(48, 45, 64, 50, SSD1306_WHITE);
  display.drawLine(64, 50, 80, 45, SSD1306_WHITE);
  display.drawLine(80, 45, 86, 35, SSD1306_WHITE);
  display.drawLine(86, 35, 86, 15, SSD1306_WHITE);
  display.drawLine(42, 15, 34, 23, SSD1306_WHITE);
  display.drawLine(42, 15, 50, 23, SSD1306_WHITE);
}

void drawEasyLeft() {
  display.drawLine(100, 45, 55, 20, SSD1306_WHITE);
  display.drawLine(55, 20, 60, 35, SSD1306_WHITE);
  display.drawLine(55, 20, 72, 20, SSD1306_WHITE);
}

void drawEasyRight() {
  display.drawLine(28, 45, 73, 20, SSD1306_WHITE);
  display.drawLine(73, 20, 56, 20, SSD1306_WHITE);
  display.drawLine(73, 20, 68, 35, SSD1306_WHITE);
}

void drawDirection(uint8_t direction) {
  switch (direction) {
    case DIRECTION_LEFT:
    case DIRECTION_SHARP_LEFT:
    case DIRECTION_KEEP_LEFT:
      drawLeftArrow();
      break;

    case DIRECTION_RIGHT:
    case DIRECTION_SHARP_RIGHT:
    case DIRECTION_KEEP_RIGHT:
      drawRightArrow();
      break;

    case DIRECTION_EASY_LEFT:
      drawEasyLeft();
      break;

    case DIRECTION_EASY_RIGHT:
      drawEasyRight();
      break;

    case DIRECTION_UTURN_LEFT:
    case DIRECTION_UTURN_RIGHT:
      drawUTurn();
      break;

    case DIRECTION_STRAIGHT:
    case DIRECTION_START:
    case DIRECTION_FOLLOW:
    case DIRECTION_MOTORWAY:
    case DIRECTION_TUNNEL:
      drawStraightArrow();
      break;

    default:
      break;
  }
}

void drawSpeed(uint8_t speed) {
  if (speed == 0) return;

  display.setTextSize(1);
  String speedText = String(speed) + " km/h";

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(speedText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(128 - w - 2, 1);
  display.print(speedText);
}

void drawMessage(String message) {
  if (message.length() == 0) return;

  message.replace("\r", "");
  message.replace("\n", " ");

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  int maxChars = 21;

  if (message.length() <= maxChars) {
    display.setCursor(0, 54);
    display.print(message);
    return;
  }

  String line1 = message.substring(0, maxChars);
  String line2 = message.substring(maxChars, min((int)message.length(), maxChars * 2));

  display.setCursor(0, 45);
  display.print(line1);
  display.setCursor(0, 55);
  display.print(line2);
}

// ============================================================
// TOP STATUS BAR HELPER
// ============================================================

void drawTopStatusBar(const char* title) {
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 1);
  display.print(title);

  // BLE status indicator badge
  if (deviceConnected) {
    display.setCursor(100, 1);
    display.print("[BLE]");
  }
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

// ============================================================
// RENDER FUNCTIONS FOR EACH MODE
// ============================================================

// Mode 1: Sygic Navigation HUD
void renderNavigationMode() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!deviceConnected) {
    display.setTextSize(2);
    display.setCursor(25, 6);
    display.println("NAV");

    display.setTextSize(1);
    display.setCursor(10, 30);
    display.println("Waiting for Sygic");

    display.setCursor(10, 44);
    display.println("BLE HUD: Advertising");

    display.setCursor(10, 55);
    display.println("Press BTN: Next Mode");
  } else if (!hasNavData || (currentDirection == DIRECTION_NONE && currentMessage.length() == 0)) {
    display.setTextSize(2);
    display.setCursor(25, 6);
    display.println("NAV");

    display.setTextSize(1);
    display.setCursor(10, 30);
    display.println("Sygic Connected!");

    display.setCursor(10, 44);
    display.println("Start Route in App");
  } else {
    drawDirection(currentDirection);
    drawSpeed(currentSpeed);
    drawMessage(currentMessage);
  }
}

// Mode 2: Mini Weather Station (DHT11 + BMP180)
void renderWeatherMode() {
  display.clearDisplay();
  drawTopStatusBar(weatherUseImperial ? "WEATHER [F/ft]" : "WEATHER [C/m]");

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // 1. Temperature & Humidity Row
  display.setCursor(2, 13);
  if (currentWeather.dhtValid || currentWeather.bmpValid) {
    if (weatherUseImperial) {
      display.printf("Temp: %.1f F", currentWeather.temperatureF);
    } else {
      display.printf("Temp: %.1f C", currentWeather.temperatureC);
    }
  } else {
    display.print("Temp: N/A");
  }

  display.setCursor(76, 13);
  if (currentWeather.dhtValid) {
    display.printf("Hum:%.0f%%", currentWeather.humidity);
  } else {
    display.print("Hum: N/A");
  }

  // 2. Barometric Pressure Row
  display.setCursor(2, 26);
  if (currentWeather.bmpValid) {
    display.printf("Pres: %.1f hPa", currentWeather.pressureHpa);
  } else {
    display.print("Pres: BMP180 N/A");
  }

  // 3. Estimated Altitude Row
  display.setCursor(2, 39);
  if (currentWeather.bmpValid) {
    if (weatherUseImperial) {
      display.printf("Alt:  %.0f ft", currentWeather.altitudeFeet);
    } else {
      display.printf("Alt:  %.0f m", currentWeather.altitudeMeters);
    }
  } else {
    display.print("Alt:  BMP180 N/A");
  }

  // 4. Air Comfort & Forecast Bar
  display.drawLine(0, 50, 128, 50, SSD1306_WHITE);
  display.setCursor(2, 54);

  if (currentWeather.dhtValid) {
    float h = currentWeather.humidity;
    if (h < 30.0f) {
      display.print("Air: Dry (Hold: Unit)");
    } else if (h <= 60.0f) {
      display.print("Air: Good (Hold: Unit)");
    } else {
      display.print("Air: Humid (Hold: Unit)");
    }
  } else {
    display.print("Hold BTN: Change Unit");
  }
}

// Mode 3: Clock & Stopwatch
void renderStopwatchMode() {
  display.clearDisplay();
  drawTopStatusBar("STOPWATCH & CLOCK");

  // System Uptime Clock
  unsigned long totalSec = millis() / 1000;
  int upH = totalSec / 3600;
  int upM = (totalSec % 3600) / 60;
  int upS = totalSec % 60;

  display.setTextSize(1);
  display.setCursor(2, 14);
  display.printf("Up: %02d:%02d:%02d", upH, upM, upS);

  // Stopwatch Timer Display
  unsigned long elapsed = getStopwatchElapsed();
  unsigned long swMs = (elapsed % 1000) / 100; // tenths
  unsigned long swSec = (elapsed / 1000) % 60;
  unsigned long swMin = (elapsed / 60000) % 60;
  unsigned long swHr = elapsed / 3600000;

  display.setTextSize(2);
  display.setCursor(8, 28);
  if (swHr > 0) {
    display.printf("%02lu:%02lu:%02lu", swHr, swMin, swSec);
  } else {
    display.printf("%02lu:%02lu.%01lu", swMin, swSec, swMs);
  }

  // Controls legend
  display.setTextSize(1);
  display.setCursor(2, 48);
  if (stopwatchRunning) {
    display.print("Status: [RUNNING]");
  } else if (elapsed > 0) {
    display.print("Status: [PAUSED]");
  } else {
    display.print("Status: [READY]");
  }

  display.setCursor(2, 56);
  display.print("Hold: Start/Stop/Reset");
}

// Mode 4: WiFi Scanner
void renderWifiScannerMode() {
  display.clearDisplay();
  drawTopStatusBar("WIFI SCANNER");

  int16_t scanStatus = WiFi.scanComplete();
  if (scanStatus == WIFI_SCAN_RUNNING) {
    display.setTextSize(1);
    display.setCursor(15, 26);
    display.println("Scanning 2.4GHz...");
    display.setCursor(15, 40);
    display.println("Please wait...");
    return;
  } else if (scanStatus >= 0) {
    wifiNetworksFound = scanStatus;
    isScanningWifi = false;
  }

  if (wifiNetworksFound <= 0) {
    display.setTextSize(1);
    display.setCursor(10, 24);
    display.println("No networks found.");
    display.setCursor(10, 44);
    display.println("Hold BTN to Rescan");
  } else {
    display.setTextSize(1);
    display.setCursor(2, 13);
    display.printf("Found: %d nets (Hold:Scan)", wifiNetworksFound);

    // Show up to 4 networks
    int displayLimit = min(wifiNetworksFound, 4);
    for (int i = 0; i < displayLimit; i++) {
      int yPos = 24 + (i * 10);
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 13) {
        ssid = ssid.substring(0, 11) + "..";
      }
      int32_t rssi = WiFi.RSSI(i);

      display.setCursor(2, yPos);
      display.printf("%d.%s", i + 1, ssid.c_str());

      display.setCursor(92, yPos);
      display.printf("%ddBm", rssi);
    }
  }
}

// Mode 5: System Monitor
void renderSystemMonitorMode() {
  display.clearDisplay();
  drawTopStatusBar("SYSTEM MONITOR");

  display.setTextSize(1);
  display.setCursor(2, 14);
  display.printf("CPU Freq: %d MHz", getCpuFrequencyMhz());

  display.setCursor(2, 25);
  display.printf("Free Heap: %u KB", ESP.getFreeHeap() / 1024);

  display.setCursor(2, 36);
  display.printf("Sensors: DHT%s BMP%s",
                 currentWeather.dhtValid ? "+" : "-",
                 bmpFound ? "+" : "-");

  display.setCursor(2, 47);
  display.printf("Flash: %u MB", ESP.getFlashChipSize() / (1024 * 1024));

  display.setCursor(2, 56);
  display.printf("BLE: %s", deviceConnected ? "Connected (Sygic)" : "Advertising");
}

// Mode 6: Torch / Screen Light
void renderTorchMode() {
  display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
}

// Mode Switch Announcement Banner
void renderModeBanner() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(2, 2, 124, 60, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 14);
  display.print("SWITCHING TO:");

  display.setTextSize(1);
  display.setCursor(10, 34);
  display.print(modeNames[currentMode]);

  display.setCursor(20, 48);
  display.print("Release to view");
}

// ============================================================
// BLE SERVER CALLBACKS
// ============================================================

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    lastActivity = millis();
    Serial.println("\n[BLE] Sygic CONNECTED");
  }

  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    hasNavData = false;
    Serial.println("\n[BLE] Sygic DISCONNECTED");
    delay(200);
    BLEDevice::startAdvertising();
  }
};

// ============================================================
// BLE DATA CALLBACK
// ============================================================

class MyWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    std::string value = characteristic->getValue();
    lastActivity = millis();

    if (value.length() == 0) return;

    const uint8_t* data = (const uint8_t*)value.data();
    uint8_t dataType = data[0];

    // Sygic Basic Navigation Data Packet (0x01)
    if (dataType == 0x01) {
      if (value.length() < 3) return;

      currentSpeed = data[1];
      currentDirection = data[2];

      currentMessage = "";
      for (size_t i = 3; i < value.length(); i++) {
        char c = (char)data[i];
        if (c == '\0') break;
        currentMessage += c;
      }

      hasNavData = true;

      Serial.printf("[NAV] Speed: %d km/h | Dir: %d | Msg: %s\n",
                    currentSpeed, currentDirection, currentMessage.c_str());
    }
  }
};

// ============================================================
// INITIALIZATION FUNCTIONS
// ============================================================

void setupBLE() {
  Serial.println("Starting BLE...");
  BLEDevice::init("ESP32 HUD");

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MyServerCallbacks());

  BLEService* service = bleServer->createService(SERVICE_UUID);

  indicateCharacteristic = service->createCharacteristic(
    CHAR_INDICATE_UUID,
    BLECharacteristic::PROPERTY_INDICATE
  );
  indicateCharacteristic->addDescriptor(new BLE2902());
  indicateCharacteristic->setValue("");

  BLECharacteristic* writeCharacteristic = service->createCharacteristic(
    CHAR_WRITE_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  writeCharacteristic->setCallbacks(new MyWriteCallbacks());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("BLE Advertising started.");
}

void setupOLED() {
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 OLED initialization FAILED!");
    while (true) { delay(100); }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(20, 8);
  display.println("ESP32");
  display.setTextSize(1);
  display.setCursor(14, 30);
  display.println("Multi-Tool HUD");
  display.setCursor(8, 46);
  display.println("+ Weather Station");
  display.display();
  delay(1200);
}

void setupSensors() {
  // Initialize DHT11
  dht.begin();
  Serial.println("DHT11 sensor initialized.");

  // Initialize BMP180 on I2C bus
  if (bmp.begin()) {
    bmpFound = true;
    Serial.println("BMP180 Barometer detected successfully.");
  } else {
    bmpFound = false;
    Serial.println("BMP180 Barometer not detected (Check I2C wiring SDA=21, SCL=22).");
  }

  // Initial read
  updateWeatherSensors();
}

// ============================================================
// MAIN ARDUINO SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(400);

  Serial.println("\n=========================================");
  Serial.println(" ESP32 MULTIPURPOSE NAVIGATION & WEATHER");
  Serial.println("=========================================");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  setupOLED();
  setupSensors();
  setupBLE();

  triggerWifiScan();

  lastActivity = millis();
  Serial.println("System Ready. Use Tactile Button on GPIO 18 to switch modes.");
}

// ============================================================
// MAIN ARDUINO LOOP
// ============================================================

unsigned long lastDisplayRefresh = 0;
const unsigned long DISPLAY_REFRESH_INTERVAL = 60; // ~16 FPS

void loop() {
  // ----------------------------------------------------------
  // 1. Update Weather Sensors in Background
  // ----------------------------------------------------------
  updateWeatherSensors();

  // ----------------------------------------------------------
  // 2. Handle Button Inputs (Short & Long Press)
  // ----------------------------------------------------------
  ButtonEvent event = readButtonEvent();

  if (event == BTN_SHORT_PRESS) {
    currentMode = (AppMode)((currentMode + 1) % MODE_COUNT);
    modeBannerUntil = millis() + 900;
    Serial.printf("[BUTTON] Short Press -> Switched to: %s\n", modeNames[currentMode]);

    if (currentMode == MODE_WIFI_SCANNER && !isScanningWifi) {
      triggerWifiScan();
    }
  } else if (event == BTN_LONG_PRESS) {
    Serial.printf("[BUTTON] Long Press in mode: %s\n", modeNames[currentMode]);

    switch (currentMode) {
      case MODE_WEATHER:
        weatherUseImperial = !weatherUseImperial; // Toggle °C / m <-> °F / ft
        Serial.printf("[WEATHER] Units toggled to: %s\n", weatherUseImperial ? "Imperial" : "Metric");
        break;

      case MODE_STOPWATCH:
        if (stopwatchRunning) {
          toggleStopwatch();
        } else if (getStopwatchElapsed() > 0) {
          resetStopwatch();
        } else {
          toggleStopwatch();
        }
        break;

      case MODE_WIFI_SCANNER:
        triggerWifiScan();
        break;

      default:
        break;
    }
  }

  // ----------------------------------------------------------
  // 3. Sygic BLE Keep-Alive Protocol (Background Task)
  // ----------------------------------------------------------
  if (deviceConnected) {
    if (millis() - lastActivity > 4000) {
      indicateCharacteristic->setValue("");
      indicateCharacteristic->indicate();
      lastActivity = millis();
    }
  }

  // ----------------------------------------------------------
  // 4. Render Display
  // ----------------------------------------------------------
  if (millis() - lastDisplayRefresh >= DISPLAY_REFRESH_INTERVAL) {
    lastDisplayRefresh = millis();

    if (millis() < modeBannerUntil) {
      renderModeBanner();
    } else {
      switch (currentMode) {
        case MODE_NAVIGATION:
          renderNavigationMode();
          break;

        case MODE_WEATHER:
          renderWeatherMode();
          break;

        case MODE_STOPWATCH:
          renderStopwatchMode();
          break;

        case MODE_WIFI_SCANNER:
          renderWifiScannerMode();
          break;

        case MODE_SYSTEM_MONITOR:
          renderSystemMonitorMode();
          break;

        case MODE_TORCH:
          renderTorchMode();
          break;

        default:
          break;
      }
    }

    display.display();
  }

  delay(5);
}