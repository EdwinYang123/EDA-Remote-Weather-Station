/*
 * =============================================
 *  BASE STATION - EDA Remote Weather Station
 * =============================================
 * Receives weather data from the Weather Station
 * via ESP-NOW and prints it to the Serial Monitor.
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
 * Hardware:
 *   - ESP32-C3
 *   - Indicator LED on LED_PIN (change if needed)
 * =============================================
 */

#include <esp_now.h>
#include <WiFi.h>

// ---- Pin Configuration ----
#define LED_PIN 8  // Onboard LED on many ESP32-C3 boards; change if using external LED

// ---- Shared Data Structure (must match WeatherStation.ino) ----
typedef struct WeatherPacket {
  float temperature;  // Degrees Celsius
  float humidity;     // Percent relative humidity
  float pressure;     // Hectopascals (hPa)
} WeatherPacket;

WeatherPacket incomingData;
volatile bool dataReceived = false;

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

// ---- Slow-blink state (non-blocking) ----
unsigned long lastBlinkTime = 0;
bool ledState = false;

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

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

    // Double flash to indicate a packet was received
    ledBlink(2, 150, 100);

    Serial.println("======= Weather Update =======");
    Serial.printf("  Temperature : %.1f C\n",   incomingData.temperature);
    Serial.printf("  Humidity    : %.1f %%\n",  incomingData.humidity);
    Serial.printf("  Pressure    : %.1f hPa\n", incomingData.pressure);
    Serial.println("==============================\n");

  } else {
    // Non-blocking slow blink (1 Hz) while waiting for data
    unsigned long now = millis();
    if (now - lastBlinkTime >= 1000) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      lastBlinkTime = now;
    }
  }
}
