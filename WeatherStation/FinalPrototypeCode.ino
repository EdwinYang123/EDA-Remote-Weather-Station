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
#define LED_PIN    10
#define SDA_PIN    D4
#define SCL_PIN    D5
#define BUTTON_PIN D2

// ---- Button / Display ----
int displayMode = 0;  // 0=temp, 1=humidity, 2=pressure

bool buttonState = HIGH;
bool lastStableState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ---- LCD Configuration ----
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

#define PAIR_TIMEOUT_MS 15000

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---- Data Structure ----
typedef struct WeatherPacket {
  float temperature;
  float humidity;
  float pressure;
} WeatherPacket;

WeatherPacket incomingData;
volatile bool dataReceived = false;

// ---- State ----
bool paired = false;
unsigned long lastDataTime = 0;

// ---- LED Blink ----
void ledBlink(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

// ---- ESP-NOW Callback ----
void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (len == sizeof(WeatherPacket)) {
    memcpy(&incomingData, data, len);
    dataReceived = true;
  }
}

// ---- LCD Helpers ----
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

  if (displayMode == 0) {
    snprintf(line1, sizeof(line1), "Temperature");
    snprintf(line2, sizeof(line2), "%.1f C", incomingData.temperature);
  }
  else if (displayMode == 1) {
    snprintf(line1, sizeof(line1), "Humidity");
    snprintf(line2, sizeof(line2), "%.1f %%", incomingData.humidity);
  }
  else {
    snprintf(line1, sizeof(line1), "Pressure");
    snprintf(line2, sizeof(line2), "%.1f hPa", incomingData.pressure);
  }

  lcd.setCursor(0, 0);
  lcdPrintPadded(line1);
  lcd.setCursor(0, 1);
  lcdPrintPadded(line2);
}

// ---- LED blink state ----
unsigned long lastBlinkTime = 0;
bool ledState = false;

// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // I2C + LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  showWaitingScreen();

  // WiFi for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("\n=== BASE STATION STARTING ===");
  Serial.print("Base Station MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.println(">>> Copy this into WeatherStation.ino <<<\n");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed!");
    lcd.setCursor(0, 0);
    lcdPrintPadded("ESP-NOW ERROR");
    lcd.setCursor(0, 1);
    lcdPrintPadded("Check serial");
    while (1) { ledBlink(1, 100, 100); }
  }

  esp_now_register_recv_cb(onDataReceived);

  Serial.println("Waiting for weather data...");

  // LED solid = OK
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

// ============================================================
void loop() {

  // ---- BUTTON HANDLING (FIXED DEBOUNCE) ----
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastStableState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        displayMode++;
        if (displayMode > 2) displayMode = 0;
        lcd.clear();
      }
    }
  }

  lastStableState = reading;

  // ---- DATA RECEIVED ----
  if (dataReceived) {
    dataReceived = false;
    lastDataTime = millis();

    if (!paired) {
      paired = true;
      Serial.println("Weather Station paired!");
    }

    ledBlink(2, 150, 100);

    Serial.println("======= Weather Update =======");
    Serial.printf("  Temperature : %.1f C\n",   incomingData.temperature);
    Serial.printf("  Humidity    : %.1f %%\n",  incomingData.humidity);
    Serial.printf("  Pressure    : %.1f hPa\n", incomingData.pressure);
    Serial.println("==============================\n");
  }

  // ---- TIMEOUT CHECK ----
  if (paired && (millis() - lastDataTime) > PAIR_TIMEOUT_MS) {
    paired = false;
    Serial.println("Link lost. Waiting...");
    showWaitingScreen();
  }

  // ---- DISPLAY UPDATE (ALWAYS REFRESH) ----
  if (paired) {
    showWeatherData();
  }

  // ---- LED STATUS BLINK ----
  unsigned long now = millis();
  if (now - lastBlinkTime >= 1000) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    lastBlinkTime = now;
  }
}
