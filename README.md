# ⏰ ESP32 IoT Smart Clock & Weather Station

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32%20Dev%20Module-red.svg?logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32)
[![Framework: Arduino / FreeRTOS](https://img.shields.io/badge/Framework-Arduino%20%7C%20FreeRTOS-00979D.svg?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Time Sync: NTP & DS3231](https://img.shields.io/badge/Timekeeping-NTP%20(IST)%20%2B%20DS3231%20RTC-blue.svg)](https://en.wikipedia.org/wiki/Real-time_clock)
[![Weather API: OpenWeather](https://img.shields.io/badge/Weather-OpenWeatherMap%20REST%20API-orange.svg)](https://openweathermap.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

An advanced, dual-display IoT Smart Clock and Environmental Station engineered on the **ESP32 Dual-Core SoC**.

This project features hybrid timekeeping (automated **NTP WiFi sync** with battery-backed **DS3231 RTC** offline fallback), dual hardware displays (**16x2 I2C LCD** + **CD4511 multiplexed 4-digit 7-segment LED**), an **integrated web configuration dashboard**, live **OpenWeatherMap meteorological telemetry**, **DHT11 climate monitoring**, and a **relay-actuated TG113 high-volume sound module** for multi-schedule alarms.

---

## 🌟 Key Highlights & Engineering Features

- 🌐 **Dual-Source Time Synchronization**:
  - Automatically queries pool NTP servers over WiFi and adjusts for Indian Standard Time (IST UTC +5:30).
  - Synchronizes continuously with a hardware **DS3231 Real-Time Clock**, guaranteeing sub-second accuracy even during total WiFi or power blackouts.
- 📺 **Dual Display Architecture**:
  - **Primary Clock**: 4-digit 7-segment LED display driven via a **CD4511 BCD-to-7-segment latch/decoder** with high-speed FreeRTOS transistor multiplexing.
  - **Information HUD**: 16×2 character LCD with custom icons, multi-page menus, animated transitions, and auto-dimming screensaver.
- 📱 **Embedded WiFi Web Dashboard**:
  - Host an on-device asynchronous HTTP server reachable at `http://<ESP32-IP>`.
  - Configure alarms, repeat days, and snooze durations directly from any smartphone or laptop.
  - Real-time scratchpad note editor stored in persistent flash memory.
- ⏰ **Triple Non-Volatile Alarms**:
  - 3 independent programmable alarms saved to ESP32 Flash via `Preferences.h`.
  - Configurable day-of-week filters (Weekdays, Weekends, Daily, or Custom).
  - Hardware relay control driving an external **TG113 Bluetooth / MP3 speaker** for room-filling alarm audio.
  - Interactive snooze (+5 or +15 min) and dismiss controls.
- ⛅ **Environmental & Weather Intelligence**:
  - Live local temperature & humidity telemetry via **DHT11**.
  - Periodic REST API calls to **OpenWeatherMap** fetching real-time weather forecasts and pressure.

---

## 🏗️ System Architecture & Block Diagram

```text
                               +--------------------------------------------+
                               |              ESP32 Dev Module              |
                               +--------------------------------------------+
                                     |               |               |
               WiFi / Internet       |               | I2C Bus       | GPIO Transistor
               ================      |               | (SDA=21,      | Multiplexing
           [NTP Servers] (IST)       |               |  SCL=22)      | (D1=4, D2=16,
           [OpenWeather API]         |               |               |  D3=17, D4=5)
           [Web Dashboard UI]        |               |               |
                                     v               v               v
                           +------------------+ +----------+ +---------------+
                           | DHT11 Sensor     | | DS3231   | | CD4511 BCD    |
                           | (Pin 13)         | | RTC Mod  | | Decoder       |
                           +------------------+ +----------+ | (A=25, B=26,  |
                                     |               |       |  C=27, D=14)  |
                                     v               v       +---------------+
                           +------------------+ +----------+         |
                           | Relay Module     | | LCD 16x2 |         v
                           | (Pin 23)         | | (I2C)    | +---------------+
                           |        |         +----------+ | 4-Digit 7-Seg   |
                           |        v                      | LED Display     |
                           |  [TG113 Speaker]              +---------------+
                           +------------------+
```

---

## 🔌 Hardware Wiring & Pinout Guide

### 1. I2C Bus Devices (LCD 16×2 & DS3231 RTC)
| Signal | ESP32 GPIO | Description |
| :--- | :---: | :--- |
| **SDA** | `GPIO 21` | Shared I2C Data Line (External pull-up recommended) |
| **SCL** | `GPIO 22` | Shared I2C Clock Line (100 kHz) |
| **VCC** | `5V / 3.3V`| System Power Rail |
| **GND** | `GND` | Common Ground |

### 2. CD4511 BCD Decoder & 7-Segment Display
| CD4511 Pin | BCD Bit | ESP32 GPIO | Notes |
| :--- | :---: | :---: | :--- |
| Pin 7 (Input A) | Bit 0 (LSB) | `GPIO 25` | Binary weight $2^0 = 1$ |
| Pin 1 (Input B) | Bit 1 | `GPIO 26` | Binary weight $2^1 = 2$ |
| Pin 2 (Input C) | Bit 2 | `GPIO 27` | Binary weight $2^2 = 4$ |
| Pin 6 (Input D) | Bit 3 (MSB) | `GPIO 14` | Binary weight $2^3 = 8$ |

### 3. Digit Select Transistors (Common Cathode Multiplexing)
| Digit | Position | ESP32 GPIO | Transistor Buffer |
| :--- | :--- | :---: | :--- |
| **Digit 1** | Hours Tens (H1) | `GPIO 4` | BC547 NPN Collector |
| **Digit 2** | Hours Units (H2) | `GPIO 16` | BC547 NPN Collector |
| **Digit 3** | Minutes Tens (M1) | `GPIO 17` | BC547 NPN Collector |
| **Digit 4** | Minutes Units (M2) | `GPIO 5` | BC547 NPN Collector |

### 4. Sensors, Buttons & Actuators
| Component | Function | ESP32 GPIO | Configuration |
| :--- | :--- | :---: | :--- |
| **DHT11 Data** | Temperature & Humidity | `GPIO 13` | Single-bus bidirectional data |
| **Relay IN** | TG113 Speaker Power Switch | `GPIO 23` | Active HIGH trigger |
| **Button 1** | `MODE` (Cycle Menu / Cancel) | `GPIO 15` | Internal Pull-Up with Debounce |
| **Button 2** | `UP` (Increment / Snooze) | `GPIO 32` | Internal Pull-Up with Debounce |
| **Button 3** | `DOWN` (Decrement / Dismiss)| `GPIO 19` | Internal Pull-Up with Debounce |

---

## 🌐 Web Dashboard Walkthrough

Once connected to your home WiFi, the device prints its local IP address over Serial and on the 16x2 LCD. Navigate to `http://<ESP32-IP>` in any web browser:

```text
====================================================
           ESP32 SMART CLOCK DASHBOARD
====================================================
 [Time] 12:45:30 PM (IST) | [Temp] 28°C | [Humidity] 62%
----------------------------------------------------
 Alarms Configuration:
   Alarm 1: [07]:[00]  Repeat: [Mon, Tue, Wed, Thu, Fri]  [X] Active
   Alarm 2: [08]:[30]  Repeat: [Sat, Sun]                 [ ] Active
   Alarm 3: [14]:[15]  Repeat: [Daily]                    [X] Active

 Persistent Quick-Note:
   [ Buy groceries and inspect solder joints          ]
                                    [ SAVE TO FLASH ]
====================================================
```

- **Instant Flash Synchronization**: Changes made in the web UI take effect immediately without needing an MCU reboot.
- **Relay Test**: Manually test the TG113 speaker audio output directly from the web interface.

---

## 🛠️ Installation & Flashing (Arduino IDE)

1. **Install ESP32 Board Core**:
   - Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Additional Board Manager URLs.
   - Install **esp32 by Espressif Systems** via Board Manager.
2. **Install Required Libraries**:
   - `LiquidCrystal_I2C` by Frank de Brabander
   - `RTClib` by Adafruit (for DS3231)
   - `DHT sensor library` by Adafruit
   - `ArduinoJson` by Benoît Blanchon
   - `ESPAsyncWebServer` & `AsyncTCP`
3. **Configure WiFi & API Keys**:
   - Open [`clock.ino`](clock.ino) in Arduino IDE.
   - Update your network credentials and OpenWeatherMap API key:
     ```cpp
     const char* ssid     = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     String openWeatherKey = "YOUR_OPENWEATHER_API_KEY";
     String city = "Chennai,IN";
     ```
4. **Compile & Upload**:
   - Board: **ESP32 Dev Module**
   - Flash Size: **4MB**
   - Upload Speed: **921600**
   - Click **Upload** (`Ctrl + U`).

---

## 📄 License

Distributed under the [MIT License](LICENSE).
