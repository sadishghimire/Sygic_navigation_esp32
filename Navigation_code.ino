#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// ============================================================
// Sygic BLE HUD UUIDs
// ============================================================

#define SERVICE_UUID \
  "DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86"

#define CHAR_INDICATE_UUID \
  "DD3F0AD2-6239-4E1F-81F1-91F6C9F01D86"

#define CHAR_WRITE_UUID \
  "DD3F0AD3-6239-4E1F-81F1-91F6C9F01D86"

// ============================================================
// Sygic directions
// ============================================================

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

// ============================================================
// BLE variables
// ============================================================

BLEServer* bleServer = nullptr;
BLECharacteristic* indicateCharacteristic = nullptr;

bool deviceConnected = false;

unsigned long lastActivity = 0;

// ============================================================
// Navigation data
// ============================================================

uint8_t currentSpeed = 0;
uint8_t currentDirection = DIRECTION_NONE;

String currentMessage = "";

// ============================================================
// Display helpers
// ============================================================

void clearDisplay()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
}

// ============================================================
// Draw LEFT arrow
// ============================================================

void drawLeftArrow()
{
  display.drawLine(
    100, 32,
    35, 32,
    SSD1306_WHITE
  );

  display.drawLine(
    35, 32,
    55, 12,
    SSD1306_WHITE
  );

  display.drawLine(
    35, 32,
    55, 52,
    SSD1306_WHITE
  );
}

// ============================================================
// Draw RIGHT arrow
// ============================================================

void drawRightArrow()
{
  display.drawLine(
    28, 32,
    93, 32,
    SSD1306_WHITE
  );

  display.drawLine(
    93, 32,
    73, 12,
    SSD1306_WHITE
  );

  display.drawLine(
    93, 32,
    73, 52,
    SSD1306_WHITE
  );
}

// ============================================================
// Draw STRAIGHT arrow
// ============================================================

void drawStraightArrow()
{
  display.drawLine(
    64, 52,
    64, 12,
    SSD1306_WHITE
  );

  display.drawLine(
    64, 12,
    45, 31,
    SSD1306_WHITE
  );

  display.drawLine(
    64, 12,
    83, 31,
    SSD1306_WHITE
  );
}

// ============================================================
// Draw U-TURN
// ============================================================

void drawUTurn()
{
  // U shape

  display.drawLine(
    42, 15,
    42, 35,
    SSD1306_WHITE
  );

  display.drawLine(
    42, 35,
    48, 45,
    SSD1306_WHITE
  );

  display.drawLine(
    48, 45,
    64, 50,
    SSD1306_WHITE
  );

  display.drawLine(
    64, 50,
    80, 45,
    SSD1306_WHITE
  );

  display.drawLine(
    80, 45,
    86, 35,
    SSD1306_WHITE
  );

  display.drawLine(
    86, 35,
    86, 15,
    SSD1306_WHITE
  );

  // Arrow head

  display.drawLine(
    42, 15,
    34, 23,
    SSD1306_WHITE
  );

  display.drawLine(
    42, 15,
    50, 23,
    SSD1306_WHITE
  );
}

// ============================================================
// Draw small diagonal arrow
// ============================================================

void drawEasyLeft()
{
  display.drawLine(
    100, 45,
    55, 20,
    SSD1306_WHITE
  );

  display.drawLine(
    55, 20,
    60, 35,
    SSD1306_WHITE
  );

  display.drawLine(
    55, 20,
    72, 20,
    SSD1306_WHITE
  );
}

void drawEasyRight()
{
  display.drawLine(
    28, 45,
    73, 20,
    SSD1306_WHITE
  );

  display.drawLine(
    73, 20,
    56, 20,
    SSD1306_WHITE
  );

  display.drawLine(
    73, 20,
    68, 35,
    SSD1306_WHITE
  );
}

// ============================================================
// Draw direction
// ============================================================

void drawDirection(uint8_t direction)
{
  switch (direction)
  {
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

// ============================================================
// Draw distance / navigation message
// ============================================================

void drawMessage(String message)
{
  if (message.length() == 0)
    return;

  // Remove any strange characters
  message.replace("\r", "");
  message.replace("\n", " ");

  display.setTextColor(SSD1306_WHITE);

  // Try to display message on bottom 20 pixels

  display.setTextSize(1);

  int maxChars = 21;

  if (message.length() <= maxChars)
  {
    display.setCursor(0, 54);
    display.print(message);
    return;
  }

  // Split into two lines

  String line1 = message.substring(
    0,
    maxChars
  );

  String line2 = message.substring(
    maxChars,
    min((int)message.length(), maxChars * 2)
  );

  display.setCursor(0, 45);
  display.print(line1);

  display.setCursor(0, 55);
  display.print(line2);
}

// ============================================================
// Draw speed
// ============================================================

void drawSpeed(uint8_t speed)
{
  if (speed == 0)
    return;

  display.setTextSize(1);

  String speedText =
    String(speed) + " km/h";

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;

  display.getTextBounds(
    speedText,
    0,
    0,
    &x1,
    &y1,
    &w,
    &h
  );

  display.setCursor(
    128 - w - 2,
    1
  );

  display.print(speedText);
}

// ============================================================
// Main navigation screen
// ============================================================

void showNavigation()
{
  clearDisplay();

  // Direction
  drawDirection(currentDirection);

  // Speed
  drawSpeed(currentSpeed);

  // Message / distance
  drawMessage(currentMessage);

  display.display();
}

// ============================================================
// Waiting screen
// ============================================================

void showWaiting()
{
  clearDisplay();

  display.setTextSize(2);

  display.setCursor(25, 5);
  display.println("NAV");

  display.setTextSize(1);

  display.setCursor(15, 30);

  if (deviceConnected)
  {
    display.println("Sygic connected");
  }
  else
  {
    display.println("Waiting for Sygic");
  }

  display.setCursor(12, 45);
  display.println("BLE HUD enabled");

  display.display();
}

// ============================================================
// No route screen
// ============================================================

void showNoRoute()
{
  clearDisplay();

  display.setTextSize(2);

  display.setCursor(10, 5);
  display.println("NO");

  display.setCursor(10, 30);
  display.println("ROUTE");

  display.display();
}

// ============================================================
// BLE server callbacks
// ============================================================

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(
    BLEServer* server
  ) override
  {
    deviceConnected = true;

    lastActivity = millis();

    Serial.println();
    Serial.println("==========================");
    Serial.println("Sygic CONNECTED");
    Serial.println("==========================");

    showWaiting();
  }

  void onDisconnect(
    BLEServer* server
  ) override
  {
    deviceConnected = false;

    Serial.println();
    Serial.println("Sygic DISCONNECTED");

    showWaiting();

    delay(500);

    BLEDevice::startAdvertising();
  }
};

// ============================================================
// BLE data callback
// ============================================================

class MyWriteCallbacks :
  public BLECharacteristicCallbacks
{
  void onWrite(
    BLECharacteristic* characteristic
  ) override
  {
    std::string value =
      characteristic->getValue();

    lastActivity = millis();

    if (value.length() == 0)
      return;

    // --------------------------------------------------------
    // Print RAW packet
    // --------------------------------------------------------

    Serial.println();
    Serial.println("--------------------------");

    Serial.print(
      "Received bytes: "
    );

    Serial.println(
      value.length()
    );

    Serial.print(
      "HEX: "
    );

    for (size_t i = 0;
         i < value.length();
         i++)
    {
      Serial.printf(
        "%02X ",
        (uint8_t)value[i]
      );
    }

    Serial.println();

    // --------------------------------------------------------
    // Sygic basic data format:
    //
    // byte 0 = data type
    // byte 1 = speed
    // byte 2 = direction
    // byte 3+ = text
    //
    // Example:
    //
    // 01 32 0A 33 35 30 6D
    //
    // 01 = basic data
    // 32 = 50 km/h
    // 0A = right
    // "350m"
    // --------------------------------------------------------

    const uint8_t* data =
      (const uint8_t*)value.data();

    uint8_t dataType = data[0];

    Serial.print(
      "Data type: 0x"
    );

    Serial.printf(
      "%02X\n",
      dataType
    );

    // --------------------------------------------------------
    // Basic navigation data
    // --------------------------------------------------------

    if (dataType == 0x01)
    {
      if (value.length() < 3)
      {
        Serial.println(
          "Invalid navigation packet"
        );

        return;
      }

      currentSpeed =
        data[1];

      currentDirection =
        data[2];

      // Text begins at byte 3
      currentMessage = "";

      for (size_t i = 3;
           i < value.length();
           i++)
      {
        char c =
          (char)data[i];

        if (c == '\0')
          break;

        currentMessage += c;
      }

      Serial.print(
        "Speed: "
      );

      Serial.println(
        currentSpeed
      );

      Serial.print(
        "Direction: "
      );

      Serial.println(
        currentDirection
      );

      Serial.print(
        "Message: "
      );

      Serial.println(
        currentMessage
      );

      showNavigation();
    }

    // --------------------------------------------------------
    // Unknown packet
    // --------------------------------------------------------

    else
    {
      Serial.println(
        "Unknown Sygic packet type"
      );
    }
  }
};

// ============================================================
// Setup BLE
// ============================================================

void setupBLE()
{
  Serial.println(
    "Starting BLE..."
  );

  // IMPORTANT:
  // This name doesn't matter much because Sygic searches
  // for the service UUID.

  BLEDevice::init(
    "ESP32 HUD"
  );

  bleServer =
    BLEDevice::createServer();

  bleServer->setCallbacks(
    new MyServerCallbacks()
  );

  // ----------------------------------------------------------
  // Create service
  // ----------------------------------------------------------

  BLEService* service =
    bleServer->createService(
      SERVICE_UUID
    );

  // ----------------------------------------------------------
  // Indication characteristic
  // ----------------------------------------------------------

  indicateCharacteristic =
    service->createCharacteristic(
      CHAR_INDICATE_UUID,
      BLECharacteristic::PROPERTY_INDICATE
    );

  indicateCharacteristic->addDescriptor(
    new BLE2902()
  );

  indicateCharacteristic->setValue(
    ""
  );

  // ----------------------------------------------------------
  // Write characteristic
  // ----------------------------------------------------------

  BLECharacteristic* writeCharacteristic =
    service->createCharacteristic(
      CHAR_WRITE_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );

  writeCharacteristic->setCallbacks(
    new MyWriteCallbacks()
  );

  // ----------------------------------------------------------
  // Start service
  // ----------------------------------------------------------

  service->start();

  // ----------------------------------------------------------
  // Advertising
  // ----------------------------------------------------------

  BLEAdvertising* advertising =
    BLEDevice::getAdvertising();

  advertising->addServiceUUID(
    SERVICE_UUID
  );

  advertising->setScanResponse(
    true
  );

  // Important for iPhone connections

  advertising->setMinPreferred(
    0x06
  );

  advertising->setMinPreferred(
    0x12
  );

  BLEDevice::startAdvertising();

  Serial.println(
    "BLE advertising started"
  );
}

// ============================================================
// OLED setup
// ============================================================

void setupOLED()
{
  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      ))
  {
    Serial.println(
      "SSD1306 initialization FAILED"
    );

    while (true)
    {
      delay(100);
    }
  }

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    25,
    5
  );

  display.println(
    "NAV"
  );

  display.setTextSize(1);

  display.setCursor(
    20,
    35
  );

  display.println(
    "Starting BLE..."
  );

  display.display();

  delay(1000);
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println();
  Serial.println(
    "================================"
  );
  Serial.println(
    " ESP32 SYGIC BLE NAVIGATION HUD"
  );
  Serial.println(
    "================================"
  );

  setupOLED();

  setupBLE();

  lastActivity = millis();

  showWaiting();

  Serial.println(
    "Setup complete."
  );

  Serial.println(
    "Waiting for Sygic..."
  );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // If connected but Sygic hasn't sent anything for 4 sec,
  // send an EMPTY indication.
  //
  // This is how the original Sygic BLE-HUD protocol requests
  // an update.
  // ----------------------------------------------------------

  if (deviceConnected)
  {
    if (
      millis() - lastActivity > 4000
    )
    {
      Serial.println(
        "Requesting Sygic update..."
      );

      // Empty value

      indicateCharacteristic->setValue(
        ""
      );

      indicateCharacteristic->indicate();

      lastActivity = millis();
    }
  }

  delay(10);
}