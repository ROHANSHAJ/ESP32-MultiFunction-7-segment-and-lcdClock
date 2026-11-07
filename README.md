# ⏰ ESP32 IoT Smart Clock & Weather Station

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32%20Dev%20Module-red.svg?logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32)
[![Framework: Arduino / FreeRTOS](https://img.shields.io/badge/Framework-Arduino%20%7C%20FreeRTOS-00979D.svg?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Time Sync: NTP & DS3231](https://img.shields.io/badge/Timekeeping-NTP%20(IST)%20%2B%20DS3231%20RTC-blue.svg)](https://en.wikipedia.org/wiki/Real-time_clock)
[![Weather API: OpenWeather](https://img.shields.io/badge/Weather-OpenWeatherMap%20REST%20API-orange.svg)](https://openweathermap.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An advanced, dual-display IoT Smart Clock and Environmental Station engineered on the **ESP32 Dual-Core SoC**.

This project features hybrid timekeeping (automated **NTP WiFi sync** with battery-backed **DS3231 RTC** offline fallback), dual hardware displays (**16x2 I2C LCD** + **CD4511 multiplexed 4-digit 7-segment LED**), an **integrated web configuration dashboard**, live **OpenWeatherMap meteorological telemetry**, **DHT11 climate monitoring**, and a **relay-actuated TG113 high-volume sound module** for multi-schedule alarms.

---

## 🌟 Key Highlights & Engineering Features

```text
                               +--------------------------------------------+
                               |              ESP32 Dev Module              |
                               +--------------------------------------------+
                                                      |
    [WiFi Network] ----------> | NTP Time Synchronizer (pool.ntp.org)       |
                               | OpenWeatherMap JSON Client (REST API)      |
                               | Embedded Async Web Configuration Server    |
                               |                   |                        |
    [DS3231 I2C RTC] <=======> | Dual-Timekeeping Engine (RTC + NTP Fallback)
    [DHT11 Sensor] ----------> | Ambient Temperature & Humidity Sampling   |
                               |                   |                        |
                               | Flash Persistence (Preferences.h / NVS)    |
                               | (Saves 3 Alarms, Sound Settings, Timezone) |
                               |                   |                        |
                               | +-----------------+----------------------+ |
                               | |                                        | |
                               v v                                        v v
                  [16x2 I2C LCD Display]                [CD4511 BCD Decoder Driver]
                  (0x27 I2C Address)                     (Pins: 25, 26, 27, 14)
                  • Real-time Date & Weather             • High-Visibility LED Clock
                  • Alarm Status & IP Address            • Multiplexed Anodes (33, 32, 19, 18)
                  • Ambient Temp & Humidity              • Smooth Transition Animations
                               |
                               v
                     [Relay Actuation Stage]
                     (Pin 4 Active Low Trigger)
                               |
                               v
                    [TG113 Sound System Module]
                    (High-Decibel Wakeup Audio)
```

---

## 🔌 Hardware Pinout & Wiring Specification

### 1. CD4511 BCD to 7-Segment Decoder & Multiplexer
| Signal | ESP32 GPIO | Connected To | Function |
| :--- | :---: | :--- | :--- |
| **BCD Bit A** | `GPIO 25` | CD4511 Pin 7 (A) | Least Significant Bit (LSB) |
| **BCD Bit B** | `GPIO 26` | CD4511 Pin 1 (B) | Bit 1 |
| **BCD Bit C** | `GPIO 27` | CD4511 Pin 2 (C) | Bit 2 |
| **BCD Bit D** | `GPIO 14` | CD4511 Pin 6 (D) | Most Significant Bit (MSB) |
| **Digit 1 (D1)** | `GPIO 33` | Common Cathode Transistor 1 | Hours Tens Digit |
| **Digit 2 (D2)** | `GPIO 32` | Common Cathode Transistor 2 | Hours Units Digit |
| **Digit 3 (D3)** | `GPIO 19` | Common Cathode Transistor 3 | Minutes Tens Digit |
| **Digit 4 (D4)** | `GPIO 18` | Common Cathode Transistor 4 | Minutes Units Digit |

### 2. I2C Bus & Sensor Interfacing
| Device | ESP32 GPIO | Bus / Protocol | Notes |
| :--- | :---: | :---: | :--- |
| **I2C SDA** | `GPIO 21` | I2C Data | Shared by PCF8574 LCD (0x27) & DS3231 RTC (0x68) |
| **I2C SCL** | `GPIO 22` | I2C Clock | 400 kHz Fast-Mode I2C Bus |
| **DHT11 Data** | `GPIO 13` | 1-Wire Digital | Ambient Temperature & Relative Humidity |
| **Sound Relay** | `GPIO 4` | Active-Low Output | Powers TG113 Sound Module amplifier on alarm |

---

## 🌐 Embedded Web Configuration Dashboard

When connected to WiFi, the ESP32 hosts a responsive, mobile-optimized HTTP control portal accessible from any browser on the local subnet (`http://<esp32-ip>/`):

- **Live Synchronization**: Force instant NTP re-synchronization with precision offset calculation.
- **Alarm Management**: Configure and persist **3 independent alarm schedules** (Hours, Minutes, Days Active, Sound Profile).
- **Audio Profile Selection**: Toggle TG113 speaker relay actuation, continuous buzzer, or silent visual notification.
- **Display Themes**: Configure LCD cycling intervals and customize idle standby screens.
- **WiFi Credential Provisioning**: Hotspot fallback captive portal if network connection drops.

---

## 💾 Non-Volatile Flash Persistence (`Preferences.h`)

All user configurations—including alarm times, active alarm flags, buzzer volume profiles, and weather station API keys—are persisted in ESP32 internal non-volatile storage (NVS) using `Preferences.h`. All settings survive power cycles and brownouts automatically.

---

## 🛠️ Installation & Flashing Guide

1. **Install Arduino IDE** (or VS Code + PlatformIO).
2. Install the **ESP32 Board Package**:
   - Go to **Settings** $\rightarrow$ **Additional Board Manager URLs** and add:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Install required libraries via Library Manager:
   - `LiquidCrystal_I2C` by Frank de Brabander
   - `RTClib` by Adafruit
   - `DHT sensor library` by Adafruit
   - `ArduinoJson` (v6 or v7) by Benoît Blanchon
4. Open `clock.ino`.
5. Configure your WiFi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
6. Select Board: **ESP32 Dev Module**.
7. Connect ESP32 via USB and click **Upload**.

---

## 📄 License

Distributed under the [MIT License](LICENSE).
