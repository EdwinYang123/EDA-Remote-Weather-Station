#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SDA_PIN D1
#define SCL_PIN D0

Adafruit_BME280 bme;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("BME280 test");

  if (!bme.begin(0x76)) {   // try 0x77 if this fails
    Serial.println("BME280 not found");
    while (1);
  }

  Serial.println("BME280 initialized");
}

void loop() {
  Serial.print("Temperature: ");
  Serial.print(bme.readTemperature());
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.print("Pressure: ");
  Serial.print(bme.readPressure() / 100.0);
  Serial.println(" hPa");

  Serial.println("-------------------");

  delay(2000);
}