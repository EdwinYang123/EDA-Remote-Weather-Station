#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>

#define SDA_PIN D1
#define SCL_PIN D0
#define BUTTON_PIN D2

Adafruit_BME280 bme;
LiquidCrystal_I2C lcd(0x27, 16, 2);

int displayMode = 0;
bool lastButtonState = HIGH;

void setup() {

  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  if (!bme.begin(0x76)) {
    lcd.setCursor(0,0);
    lcd.print("BME280 Error");
    while(1);
  }

  lcd.clear();
}

void loop() {

  bool buttonState = digitalRead(BUTTON_PIN);

  // detect button press
  if (lastButtonState == HIGH && buttonState == LOW) {
    displayMode++;
    if (displayMode > 2) displayMode = 0;
    lcd.clear();
    delay(200);
  }

  lastButtonState = buttonState;

  float temp = bme.readTemperature();
  float hum  = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  lcd.setCursor(0,0);

  if(displayMode == 0) {
    lcd.print("Temperature");
    lcd.setCursor(0,1);
    lcd.print(temp);
    lcd.print(" C");
  }

  if(displayMode == 1) {
    lcd.print("Humidity");
    lcd.setCursor(0,1);
    lcd.print(hum);
    lcd.print(" %");
  }

  if(displayMode == 2) {
    lcd.print("Pressure");
    lcd.setCursor(0,1);
    lcd.print(pres);
    lcd.print(" hPa");
  }

  delay(500);
}
