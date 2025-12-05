# ESP32 Smart Clock (3 Alarms + TG113 Audio + Weather + Web UI)
Full-Featured Smart Clock with NTP (IST), DS3231 RTC, LCD UI, 7-Segment Display, 3 Alarms with Flash Save, Weather API, WiFi Web Dashboard, Screensaver, and Relay-Controlled TG113 Bluetooth Speaker for Alarm Sound.

## Overview
This project implements a complete IoT smart clock system using an ESP32 Dev Module.

### Key Features
- NTP Time Sync (IST +5:30)
- DS3231 RTC for offline accuracy
- LCD UI with multiple pages
- 7-Segment HHMM display via CD4511 + RTOS multiplexing
- 3 Alarms (hour/minute/days)
- Flash save using Preferences.h
- TG113 Bluetooth module connected via relay for alarm sound
- OpenWeather API integration
- DHT11 temperature & humidity
- WiFi Web Dashboard
- Screensaver with auto-rotation
- Brightness control (manual + auto)
- Music Page for controlling relay/TG113

## Hardware Used
- ESP32 Dev Module  
- TG113 Bluetooth Speaker Module (powered via relay)  
- DS3231 RTC Module  
- LCD 16×2 (I2C)  
- 4-Digit 7-Segment Display  
- CD4511 BCD Driver  
- BC457 Transistors  
- DHT11 Sensor  
- Relay Module  
- Buttons (MODE/UP/DOWN)

## Wiring

### LCD (I2C)
| Signal | ESP32 Pin |
|--------|------------|
| SDA | 21 |
| SCL | 22 |

### DHT11
| Signal | Pin |
|--------|----|
| DATA | 13 |

### CD4511 BCD
| Bit | Pin |
|-----|-----|
| A | 25 |
| B | 26 |
| C | 27 |
| D | 14 |

### Digit Enable Pins
| Digit | Pin |
|--------|------|
| D1 | 4 |
| D2 | 16 |
| D3 | 17 |
| D4 | 5 |

### Buttons
| Button | Pin |
|--------|------|
| MODE | 15 |
| UP | 32 |
| DOWN | 19 |

### Relay (TG113 Power Control)
| Device | Pin |
|--------|------|
| Relay IN | 23 |

## Web Interface
Open in browser:
```
http://<ESP32-IP>
```

You can:
- Edit note  
- Edit all 3 alarms  
- Modify repeat days  
- Enable/disable alarms  
- Save to flash  

## Alarm Behavior
- At alarm time → ESP32 triggers relay ON (4 sec)  
- TG113 powers up and plays from SD/USB/FM  
- Alarm can be stopped or snoozed  
- Snooze: +5 or +15 minutes  

## Screensaver
- Activates after 15 seconds  
- Auto-rotates pages every 4 seconds  

## Brightness
- 0 = Auto  
- 1–10 = Manual levels  

