/*
 * =============================================
 *  BASE STATION - EDA Remote Weather Station
 * =============================================
 * Receives weather data from the Weather Station
 * via ESP-NOW and displays it on a 16x2 I2C LCD.
 *
 * SETUP INSTRUCTIONS:
 *   1. Upload this sketch to the Base Station ESP32-C3.
 *   2. Open Serial Monitor at 115200 baud.
 *   3. Copy the MAC address printed on startup.
 *   4. Paste that MAC into WeatherStation.ino as baseStationMAC[].
 *   5. Upload WeatherStation.ino to the Weather Station.
 *
 * LED BEHAVIOUR:
 *   - Solid 1s on boot     = Setup complete
 *   - Slow 1Hz blink       = Waiting for data
 *   - Double flash         = Data packet received
 *   - Fast blink (forever) = Fatal error during setup
 *
 * LCD BEHAVIOUR:
 *   - "Waiting to pair"    = No data received (or link timed out)
 *   - Temp / Humidity /
 *     Pressure readings    = Live weather data being received
 *
 * Hardware:
 *   - ESP32-C3 (Seeed Studio XIAO ESP32-C3)
 *   - Indicator LED on LED_PIN (change if needed)
 *   - 16x2 I2C LCD on SDA=D4, SCL=D5 (address LCD_ADDR)
 *     Common addresses: 0x27 (most common) or 0x3F
 * =============================================
 */

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---- Pin Configuration ----
#define LED_PIN  10   // Onboard LED on Seeed Studio XIAO ESP32-C3
#define SDA_PIN  D4   // I2C SDA for LCD
#define SCL_PIN  D5   // I2C SCL for LCD

// ---- LCD Configuration ----
#define LCD_ADDR 0x27  // Change to 0x3F if display is blank
#define LCD_COLS 16
#define LCD_ROWS 2

// If link is silent for this long (ms), revert to "Waiting to pair".
// WeatherStation sends every 5 s, so 15 s = 3 missed packets.
#define PAIR_TIMEOUT_MS 15000

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---- Shared Data Structure (must match WeatherStation.ino) ----
typedef struct WeatherPacket {
  float temperature;  // Degrees Celsius
  float humidity;     // Percent relative humidity
  float pressure;     // Hectopascals (hPa)
} WeatherPacket;

WeatherPacket incomingData;
volatile bool dataReceived = false;

// ---- Pairing / timeout state ----
bool paired = false;
unsigned long lastDataTime = 0;

// ---- Helper: Blocking LED blink ----
void ledBlink(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

// ---- ESP-NOW Receive Callback ----
// Called automatically when a packet arrives (runs in WiFi task context).
// NOTE: If your ESP32 Arduino core is older than 2.x, change the signature to:
//   void onDataReceived(const uint8_t *mac, const uint8_t *data, int len)
void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (len == sizeof(WeatherPacket)) {
    memcpy(&incomingData, data, len);
    dataReceived = true;
  }
}

// ---- LCD Helpers ----

// Print a left-aligned string padded to LCD_COLS to clear any leftover chars.
void lcdPrintPadded(const char *str) {
  int len = strlen(str);
  lcd.print(str);
  for (int i = len; i < LCD_COLS; i++) lcd.print(' ');
}

void showWaitingScreen() {
  lcd.setCursor(0, 0);
  lcdPrintPadded("Waiting to pair");
  lcd.setCursor(0, 1);
  lcdPrintPadded("(searching...)");
}

void showWeatherData() {
  char line1[LCD_COLS + 1];
  char line2[LCD_COLS + 1];

  // Line 1: Temperature and Humidity  e.g. "T:25.3C  H:65.2%"
  snprintf(line1, sizeof(line1), "T:%.1fC  H:%.1f%%",
           incomingData.temperature, incomingData.humidity);

  // Line 2: Pressure  e.g. "P: 1013.2 hPa  "
  snprintf(line2, sizeof(line2), "P: %.1f hPa",
           incomingData.pressure);

  lcd.setCursor(0, 0);
  lcdPrintPadded(line1);
  lcd.setCursor(0, 1);
  lcdPrintPadded(line2);
}

// ---- Slow-blink state (non-blocking) ----
unsigned long lastBlinkTime = 0;
bool ledState = false;

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ---- Initialise I2C and LCD ----
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  showWaitingScreen();

  // ---- WiFi in Station mode (required for ESP-NOW) ----
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("\n=== BASE STATION STARTING ===");
  Serial.print("Base Station MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.println(">>> Copy the MAC above into WeatherStation.ino <<<\n");

  // ---- Initialise ESP-NOW ----
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed!");
    lcd.setCursor(0, 0);
    lcdPrintPadded("ESP-NOW ERROR");
    lcd.setCursor(0, 1);
    lcdPrintPadded("Check serial log");
    while (1) { ledBlink(1, 100, 100); }  // Fast blink = fatal error
  }

  esp_now_register_recv_cb(onDataReceived);

  Serial.println("Base Station ready. Waiting for weather data...");

  // Solid LED for 1 second = setup OK
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

// ============================================================
void loop() {
  if (dataReceived) {
    dataReceived = false;
    lastDataTime = millis();

    // First packet — transition to paired state
    if (!paired) {
      paired = true;
      Serial.println("Weather Station paired!");
    }

    // Double flash to indicate a packet was received
    ledBlink(2, 150, 100);

    Serial.println("======= Weather Update =======");
    Serial.printf("  Temperature : %.1f C\n",   incomingData.temperature);
    Serial.printf("  Humidity    : %.1f %%\n",  incomingData.humidity);
    Serial.printf("  Pressure    : %.1f hPa\n", incomingData.pressure);
    Serial.println("==============================\n");

    showWeatherData();

  } else {
    // Check for link timeout
    if (paired && (millis() - lastDataTime) > PAIR_TIMEOUT_MS) {
      paired = false;
      Serial.println("Weather Station link lost. Waiting to pair...");
      showWaitingScreen();
    }

    // Non-blocking slow blink (1 Hz) while waiting or between updates
    unsigned long now = millis();
    if (now - lastBlinkTime >= 1000) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      lastBlinkTime = now;
    }
  }
}
