# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EDA (Electronics Design & Applications) class project: a wireless remote weather monitoring station using two ESP32-C3 microcontrollers communicating via ESP-NOW.

## Uploading / Building

This is an Arduino project with no build system. Sketches are compiled and flashed using the **Arduino IDE** or **arduino-cli**. There are no automated tests.

To upload a sketch with arduino-cli (example):
```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 BaseStation/BaseStation.ino
arduino-cli upload  --fqbn esp32:esp32:XIAO_ESP32C3 -p /dev/ttyUSB0 BaseStation/BaseStation.ino
```

Serial monitor (115200 baud):
```bash
arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200
```

## Architecture

Two independent sketches that must be flashed to separate hardware:

### WeatherStation (`WeatherStation/WeatherStation.ino`)
- Reads temperature, humidity, and pressure from a **BME280** sensor over I2C (`SDA=D1, SCL=D0`, address `0x76`)
- Packs readings into a `WeatherPacket` struct and transmits it **every 5 seconds** via **ESP-NOW** to a hardcoded Base Station MAC address (`baseStationMAC[]`)
- **First-time setup:** flash BaseStation first, copy the printed MAC, paste it into `baseStationMAC[]`, then reflash WeatherStation

### BaseStation (`BaseStation/BaseStation.ino`)
- Receives `WeatherPacket` structs over ESP-NOW (no MAC filter — accepts from any sender)
- Displays live readings on a **16×2 I2C LCD** (`SDA=D4, SCL=D5`, address `0x27`)
- Shows `"Waiting to pair"` until the first packet arrives; reverts to it after **15 s** of silence (3 missed packets)
- Also prints all received data to Serial

### Shared contract
Both sketches define an identical `WeatherPacket` struct — any change to this struct must be applied to **both** files and both boards reflashed.

```cpp
typedef struct WeatherPacket {
  float temperature;  // °C
  float humidity;     // % RH
  float pressure;     // hPa
} WeatherPacket;
```

## Required Arduino Libraries

| Library | Used by |
|---|---|
| `Adafruit BME280 Library` + `Adafruit Unified Sensor` | WeatherStation |
| `LiquidCrystal_I2C` | BaseStation |

ESP-NOW and WiFi are part of the ESP32 Arduino core (no separate install needed).

## Hardware

- **MCU:** Seeed Studio XIAO ESP32-C3 (both stations)
- **Sensor:** Bosch BME280 — I2C address `0x76` (try `0x77` if not found)
- **Display:** 16×2 LCD with I2C backpack — address `0x27` (try `0x3F` if blank)

## TestCode

`TestCode/BME280TEST.ino` is a standalone sketch for verifying the BME280 sensor works independently of the full system.
