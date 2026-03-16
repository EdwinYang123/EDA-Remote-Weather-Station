/*
 * =============================================
 *  WEATHER STATION - EDA Remote Weather Station
 * =============================================
 * Reads temperature, humidity, and pressure from
 * a BME280 sensor and transmits the data to the
 * Base Station every 5 seconds via ESP-NOW.
 *
 * SETUP INSTRUCTIONS:
 *   1. Flash BaseStation.ino to the Base Station first.
 *   2. Open its Serial Monitor and copy the MAC address.
 *   3. Paste that MAC into baseStationMAC[] below.
 *   4. Upload this sketch to the Weather Station ESP32-C3.
 *
 * LED BEHAVIOUR:
 *   - Solid 1s on boot          = Setup complete
 *   - Single flash (every 5s)   = Data delivered successfully
 *   - Triple fast flash         = Delivery failed (check MAC / range)
 *   - Fast blink (forever)      = Fatal error during setup
 *
 * Hardware:
 *   - ESP32-C3
 *   - BME280 on I2C: SDA=D1, SCL=D0, address 0x76
 *     (change I2C address to 0x77 if 0x76 is not found)
 *   - Indicator LED on LED_PIN (change if needed)
 * =============================================
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <esp_now.h>
#include <WiFi.h>

// ---- Pin Configuration ----
#define SDA_PIN  D1
#define SCL_PIN  D0
#define LED_PIN  10  // Onboard LED on Seeed Studio XIAO ESP32-C3

// ---- Base Station MAC Address ----
// Run BaseStation.ino first, then copy the MAC it prints and replace the bytes below.
// Example: if Serial prints "A0:B1:C2:D3:E4:F5" enter {0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5}
uint8_t baseStationMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // <-- REPLACE THIS

// ---- Send interval ----
#define SEND_INTERVAL_MS 5000  // milliseconds between readings

// ---- BME280 ----
Adafruit_BME280 bme;

// ---- Shared Data Structure (must match BaseStation.ino) ----
typedef struct WeatherPacket {
  float temperature;  // Degrees Celsius
  float humidity;     // Percent relative humidity
  float pressure;     // Hectopascals (hPa)
} WeatherPacket;

WeatherPacket weatherData;

// ---- Delivery status flags (set inside ESP-NOW callback) ----
volatile bool deliveryDone    = false;
volatile bool deliverySuccess = false;

// ---- Helper: Blocking LED blink ----
void ledBlink(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

// ---- ESP-NOW Send Callback ----
// Called after each esp_now_send() to report delivery status.
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  deliverySuccess = (status == ESP_NOW_SEND_SUCCESS);
  deliveryDone    = true;
}

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ---- Initialise BME280 ----
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bme.begin(0x76)) {
    Serial.println("ERROR: BME280 not found at 0x76. Try 0x77 or check wiring.");
    while (1) { ledBlink(1, 100, 100); }  // Fast blink = fatal error
  }
  Serial.println("BME280 initialised.");

  // ---- WiFi in Station mode (required for ESP-NOW) ----
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("Weather Station MAC Address: ");
  Serial.println(WiFi.macAddress());

  // ---- Initialise ESP-NOW ----
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed!");
    while (1) { ledBlink(1, 100, 100); }  // Fast blink = fatal error
  }

  esp_now_register_send_cb(onDataSent);

  // ---- Register the Base Station as an ESP-NOW peer ----
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, baseStationMAC, 6);
  peerInfo.channel = 0;      // 0 = use current channel
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ERROR: Failed to add peer. Check baseStationMAC[].");
    while (1) { ledBlink(1, 100, 100); }  // Fast blink = fatal error
  }

  Serial.println("Weather Station ready. Sending data every 5 seconds.");

  // Solid LED for 1 second = setup OK
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

// ============================================================
void loop() {
  // ---- Read sensor ----
  weatherData.temperature = bme.readTemperature();
  weatherData.humidity    = bme.readHumidity();
  weatherData.pressure    = bme.readPressure() / 100.0F;

  Serial.printf("Sending -> Temp: %.1f C | Humidity: %.1f %% | Pressure: %.1f hPa\n",
    weatherData.temperature, weatherData.humidity, weatherData.pressure);

  // ---- Transmit via ESP-NOW ----
  deliveryDone = false;
  esp_now_send(baseStationMAC, (uint8_t *)&weatherData, sizeof(weatherData));

  // Wait for delivery confirmation (timeout 500 ms)
  unsigned long waitStart = millis();
  while (!deliveryDone && (millis() - waitStart) < 500);

  if (deliverySuccess) {
    Serial.println("  -> Delivered OK");
    ledBlink(1, 200, 0);        // Single flash = success
  } else {
    Serial.println("  -> DELIVERY FAILED (check MAC / distance)");
    ledBlink(3, 100, 100);      // Triple fast flash = failure
  }

  delay(SEND_INTERVAL_MS);
}
