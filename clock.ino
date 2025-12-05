// ===================================================
// PART 1/7 — INCLUDES + GLOBALS + STRUCTS
// (Patched Version with NTP + India Time + 3 Alarms + Flash Save + Music Page + Animations + Alarm Enable Page)
// ===================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "RTClib.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <time.h>
#include <Preferences.h>

// ====================== DHT11 =======================
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====================== LCD =========================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ====================== RTC =========================
RTC_DS3231 rtc;

// =================== CD4511 BCD PINS ================
#define A_PIN 25
#define B_PIN 26
#define C_PIN 27
#define D_PIN 14

// Digit enables
#define D1 4
#define D2 16
#define D3 17
#define D4 5

// ====================== BUTTONS =====================
#define BTN_MODE 15
#define BTN_UP   32
#define BTN_DOWN 19

// ====================== RELAY =======================
#define RELAY_PIN 23

// ===================================================
// 7-SEG DIGITS BUFFER (HHMM)
// ===================================================
int digits[4] = {0, 0, 0, 0};

// ===================================================
// TIME CACHE
// ===================================================
int curHour = 0, curMin = 0, curSec = 0;
int curDay = 0, curMonth = 0, curYear = 0;
int curWeekday = 0;  // 0=Sun, 1=Mon, ..., 6=Sat
String curDayName = "Sun";
String curMonthName = "Jan";
unsigned long lastUnixTime = 0;
unsigned long lastRTCUpdate = 0;

// ===================================================
// DAY AND MONTH NAMES
// ===================================================
const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// ===================================================
// BRIGHTNESS LEVEL (0-10 where 0=Auto)
// ===================================================
int brightnessLevel = 0;  // 0 = Auto brightness

// ====================== PAGES =======================
enum DisplayPage {
  PAGE_CLOCK = 0,
  PAGE_NOTE,
  PAGE_ALARM,
  PAGE_BRIGHTNESS,
  PAGE_WIFI,
  PAGE_DHT,
  PAGE_WEATHER,
  PAGE_TIME_SYNC,
  PAGE_MUSIC,     // <--- NEW PAGE
  PAGE_COUNT
};

DisplayPage currentPage = PAGE_CLOCK;
DisplayPage lastPage = PAGE_CLOCK;

// ====================== NOTE ========================
String noteText = "ROHAN STAY FOCUSED";   // 1–2 lines by default
int noteScrollIndex = 0;

// DHT cache
float lastTemp = NAN;
float lastHum  = NAN;
unsigned long lastDHTUpdate = 0;

// LCD update timer
unsigned long lastLCDUpdate = 0;
// ====================== SCREENSAVER =========================
unsigned long lastInteraction = 0;
bool screensaverMode = false;
unsigned long lastScreenRotate = 0;

DisplayPage screensaverPages[] = {
    PAGE_CLOCK,
    PAGE_NOTE,
    PAGE_WIFI,
    PAGE_DHT,
    PAGE_WEATHER,
    PAGE_TIME_SYNC
};
int screensaverIndex = 0;
const int screensaverCount = sizeof(screensaverPages)/sizeof(screensaverPages[0]);


// ====================== RTOS MUX ====================
TaskHandle_t muxTaskHandle = NULL;

// ====================== WIFI CONFIG =================
const char* WIFI_SSID     = "Ramshaj";    
const char* WIFI_PASSWORD = "loxrambulchub";

bool wifiEnabled   = true;  // Enable WiFi by default
bool wifiConnected = false;
unsigned long lastWiFiAttempt = 0;
bool shouldSyncTimeOnConnect = false;  // Flag to sync time when WiFi connects

WebServer server(80);

// ====================== WEATHER =====================
String weatherCity   = "--";
String weatherMain   = "--";
float  weatherTempC  = NAN;
unsigned long lastWeatherFetch = 0;
bool weatherAnim = false;   // small animation toggle

const char* OPENWEATHER_API_KEY = "c9b96b2d6785949a401d32089dccd4c9";

// ====================== TIME SYNC (NTP / IST) ===================
bool timeSynced = false;
String lastSyncMessage = "Not synced";
unsigned long lastSyncAttempt = 0;

const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 19800;     // IST = UTC + 5:30 => 5.5 * 3600
const int   DST_OFFSET_SEC = 0;

// ===================================================
// 3 ALARMS + ACTIVE INDEX
// ===================================================
struct AlarmData {
  int hour, minute;
  uint8_t repeatMask;   // bit0=Sun ... bit6=Sat
  bool enabled;
  bool ringing;
  unsigned long snoozeUntil;
};

AlarmData alarms[3] = {
  {7,  0, 0x7F, false, false, 0},   // Alarm 1
  {12, 0, 0x7F, false, false, 0},   // Alarm 2
  {18, 0, 0x00, false, false, 0}    // Alarm 3 (off by default)
};

int activeAlarm = 0;   // 0, 1 or 2 → which alarm we are viewing/editing

// Alarm edit state
enum AlarmEditState {
  ALARM_VIEW = 0,
  ALARM_EDIT_HOUR,
  ALARM_EDIT_MIN,
  ALARM_EDIT_DAYS,
  ALARM_EDIT_ENABLE   // <--- We will use this for full toggle page
};

AlarmEditState alarmState = ALARM_VIEW;
uint8_t alarmDayCursor = 0;

const char dayCharsLCD[7] = {'S','M','T','W','T','F','S'};

Preferences prefs;     // Permanent storage

// ===================================================
// RELAY STATE
// ===================================================
bool relayOn = false;
unsigned long relayOffTime = 0;

// ============================================================
// PART 2/7 — FUNCTION DECLARATIONS
// ============================================================

// Helper functions
void sendBCD(int n);
void updateDigits(int hour, int minute);
int getAutoBrightness(int hour);
void setNumber(int value);
String fit16(const String &s);
String getIPString();

// RTOS Task
void muxTaskCode(void *parameter);

// WiFi functions
void manageWiFi();
void handleRoot();
void handleSave();

// Weather functions
bool detectCityFromIP(String &cityOut);
void fetchWeather();

// Time functions
void syncTimeFromNet();

// Alarm functions
bool alarmDayMatches(const DateTime &now, int idx);
void startAlarm(int idx);
void stopAlarm(int idx, bool fromStopBtn);
void snoozeAlarm(int idx, uint8_t min);
void updateAlarmRTC(const DateTime &now);

// Flash functions
void saveAlarmsToFlash();
void loadAlarmsFromFlash();

// Button functions
void handleButtons();
void drawAlarmLCD();

// Display functions
void renderNotePage();
void renderClockPage();
void renderBrightnessPage();
void renderWiFiPage();
void renderWeatherPage();
void renderTimeSyncPage();
void renderAlarmRingingLCD(int idx);
void updateLCD();
void updateRTC();

// Relay functions
void startRelayPulse(unsigned long ms);
void updateRelay();

// Serial functions
void handleSerial();

// ============================================================
// PART 3/7 — RTOS 7-SEG TASK + BRIGHTNESS + HELPER FUNCTIONS
// ============================================================

// ---------------------- BCD SEND ----------------------------
void sendBCD(int n) {
  digitalWrite(A_PIN, n & 1);
  digitalWrite(B_PIN, (n >> 1) & 1);
  digitalWrite(C_PIN, (n >> 2) & 1);
  digitalWrite(D_PIN, (n >> 3) & 1);
}

// -------------------- UPDATE DIGITS -------------------------
void updateDigits(int hour, int minute) {
  digits[0] = hour / 10;
  digits[1] = hour % 10;
  digits[2] = minute / 10;
  digits[3] = minute % 10;
}

// ----------------------- AUTO BRIGHT ------------------------
int getAutoBrightness(int hour) {
  if (hour >= 0 && hour < 6) return 2;
  else if (hour < 9) return 4;
  else if (hour < 17) return 10;
  else if (hour < 21) return 6;
  else return 3;
}

// ============================================================
// RTOS MULTIPLEX TASK (Runs on Core 0)
// Ultra-stable, no flicker, no interference with buttons/LCD
// ============================================================
void muxTaskCode(void *parameter) {
  int digit = 0;
  int frame = 0;

  for (;;) {
    int actualBrightness = (brightnessLevel == 0) ? getAutoBrightness(curHour) : brightnessLevel;
    int duty = (actualBrightness + 1) / 2;  // 1–10 → 1–5

    // turn all digits OFF
    digitalWrite(D1, LOW);
    digitalWrite(D2, LOW);
    digitalWrite(D3, LOW);
    digitalWrite(D4, LOW);

    if (frame < duty) {
      sendBCD(digits[digit]);

      switch (digit) {
        case 0: digitalWrite(D1, HIGH); break;
        case 1: digitalWrite(D2, HIGH); break;
        case 2: digitalWrite(D3, HIGH); break;
        case 3: digitalWrite(D4, HIGH); break;
      }
    }

    vTaskDelay(1); // ~1ms

    switch (digit) {
      case 0: digitalWrite(D1, LOW); break;
      case 1: digitalWrite(D2, LOW); break;
      case 2: digitalWrite(D3, LOW); break;
      case 3: digitalWrite(D4, LOW); break;
    }

    digit++;
    if (digit > 3) {
      digit = 0;
      frame++;
      if (frame > 4) frame = 0;
    }
  }
}

// ============================================================
// ------------- QUICK NUMBER TEST HELPER ---------------------
// ============================================================
void setNumber(int value) {
  value = constrain(value, 0, 9999);
  digits[3] = value % 10; value /= 10;
  digits[2] = value % 10; value /= 10;
  digits[1] = value % 10; value /= 10;
  digits[0] = value % 10;
}

// ============================================================
// SMALL UTILITIES
// ============================================================
String fit16(const String &s) {
  if (s.length() <= 16) return s;
  return s.substring(0, 16);
}

String getIPString() {
  if (!wifiConnected) return "0.0.0.0";
  return WiFi.localIP().toString();
}

// ============================================================
// PART 4/7 — WIFI + WEB UI + TIME + WEATHER
// ============================================================

// ================== WIFI MANAGER =====================
void manageWiFi() {
  if (!wifiEnabled) {
    if (wifiConnected) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      wifiConnected = false;
      Serial.println("[WIFI] Disabled");
    }
    return;
  }

  if (!wifiConnected) {
    if (millis() - lastWiFiAttempt > 4000) {
      lastWiFiAttempt = millis();
      Serial.printf("[WIFI] Connecting to %s...\n", WIFI_SSID);

      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.print("[WIFI] Connected! IP = ");
      Serial.println(WiFi.localIP());
      server.begin();
      Serial.println("[WEB] Server started");

      // Set flag to sync time when WiFi connects
      shouldSyncTimeOnConnect = true;
      Serial.println("[TIME] Will sync time on WiFi connect");
    }
  }

  // Auto-sync time when WiFi connects
  if (wifiConnected && shouldSyncTimeOnConnect) {
    shouldSyncTimeOnConnect = false;
    syncTimeFromNet();  // Sync time immediately via NTP
  }
}

// ============================================================
// ------------------ WEB SERVER HTML -------------------------
// ============================================================
String htmlHeader() {
  return F("<html><head><meta charset='utf-8'>"
           "<style>"
           "body{font-family:Arial;background:#111;color:#eee;padding:20px;}"
           "h2{color:#4ec9b0;}"
           "label{display:block;margin:5px 0;}"
           ".btn{background:#4ec9b0;color:black;padding:10px 20px;"
           "border:none;border-radius:6px;margin-top:10px;cursor:pointer;font-weight:bold;}"
           "</style>"
           "</head><body>");
}

String htmlFooter() {
  return F("</body></html>");
}

// ---------------------- ROOT PAGE ---------------------------
void handleRoot() {
  String html = htmlHeader();
  html += F("<h2>ESP32 SMART CLOCK</h2>");

  html += F("<h3>Personal Note</h3>"
            "<form action='/save' method='GET'>"
            "<textarea name='note' rows='2' cols='40'>");
  html += noteText;
  html += F("</textarea><br><br>");

  // ------------------- 3 ALARMS -------------------
  for (int i = 0; i < 3; i++) {
    html += F("<h3>Alarm ");
    html += (i+1);
    html += "</h3>";
    html += "Hour: <input type='number' name='ah";
    html += i;
    html += "' min='0' max='23' value='";
    html += String(alarms[i].hour);
    html += "'><br>";
    html += "Minute: <input type='number' name='am";
    html += i;
    html += "' min='0' max='59' value='";
    html += String(alarms[i].minute);
    html += "'><br>";

    // Day checkboxes
    const char* dayNamesWeb[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    html += F("<h4>Repeat Days:</h4>");
    for (int j = 0; j < 7; j++) {
      html += "<label><input type='checkbox' name='d";
      html += i;
      html += j;
      html += "' ";
      if (alarms[i].repeatMask & (1 << j)) html += "checked";
      html += "> ";
      html += String(dayNamesWeb[j]);
      html += "</label>";
    }

    // Enabled checkbox
    html += "<br><br>Enabled: <input type='checkbox' name='ae";
    html += i;
    html += "' ";
    if (alarms[i].enabled) html += "checked";
    html += "><br>";
  }

  // ------------------- WIFI -------------------
  html += F("<h3>WiFi</h3>");
  html += "Status: ";
  html += (wifiConnected ? "Connected" : "Not Connected");
  html += "<br>IP: ";
  html += getIPString();
  html += "<br><br>";

  html += F("<input class='btn' type='submit' value='Save Settings'>");
  html += F("</form>");
  html += htmlFooter();

  server.send(200,"text/html",html);
}

// ---------------------- SAVE HANDLER ------------------------
void handleSave() {
  if (server.hasArg("note")) {
    noteText = server.arg("note");
  }

  for (int i = 0; i < 3; i++) {
    String ah = "ah" + String(i);
    String am = "am" + String(i);
    if (server.hasArg(ah)) {
      int ahVal = server.arg(ah).toInt();
      if (ahVal >= 0 && ahVal < 24) alarms[i].hour = ahVal;
    }
    if (server.hasArg(am)) {
      int amVal = server.arg(am).toInt();
      if (amVal >= 0 && amVal < 60) alarms[i].minute = amVal;
    }

    // Days
    uint8_t mask = 0;
    for (int j = 0; j < 7; j++) {
      String dj = "d" + String(i) + String(j);
      if (server.hasArg(dj)) mask |= (1 << j);
    }
    alarms[i].repeatMask = mask;

    // Enabled
    String ae = "ae" + String(i);
    alarms[i].enabled = server.hasArg(ae);
  }

  saveAlarmsToFlash();

  server.send(200,"text/html",
    "<html><body><h2>Saved!</h2>"
    "<a href='/'>BACK</a></body></html>"
  );

  Serial.println("[WEB] 3 Alarms updated");
}

// ============================================================
// ---------------------- WEATHER -----------------------------
// ============================================================
bool detectCityFromIP(String &cityOut) {
  if (!wifiConnected) return false;

  HTTPClient http;
  http.begin("http://ip-api.com/json/?fields=city,status,countryCode");
  http.setTimeout(5000);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[WEATHER] IP detection failed: %d\n", code);
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  if (err) {
    Serial.printf("[WEATHER] JSON error: %s\n", err.c_str());
    return false;
  }

  if (String((const char*)doc["status"]) == "success") {
    cityOut = doc["city"].as<String>();
    String country = doc["countryCode"].as<String>();
    Serial.printf("[WEATHER] Detected city: %s, %s\n", cityOut.c_str(), country.c_str());
    return true;
  }
  return false;
}

void fetchWeather() {
  if (!wifiConnected) return;

  String city;
  if (!detectCityFromIP(city)) {
    city = "Kolkata";  // Default fallback
  }

  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city +
               "&appid=" + OPENWEATHER_API_KEY + "&units=metric";

  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[WEATHER] API error: %d\n", code);
    weatherCity = "Error";
    weatherTempC = NAN;
    weatherMain = "Err";
    return;
  }

  String payload = http.getString();
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.printf("[WEATHER] JSON parse error: %s\n", err.c_str());
    weatherCity = "ParseErr";
    return;
  }

  weatherTempC = doc["main"]["temp"].as<float>();
  weatherMain  = doc["weather"][0]["main"].as<String>();
  weatherCity  = city;

  lastWeatherFetch = millis();
  Serial.printf("[WEATHER] %s: %.1f°C %s\n", city.c_str(), weatherTempC, weatherMain.c_str());
}

// ============================================================
// ---------------------- TIME SYNC (NTP) ---------------------
// ============================================================
void syncTimeFromNet() {
  if (!wifiConnected) {
    lastSyncMessage = "No WiFi";
    Serial.println("[TIME] Cannot sync, no WiFi");
    return;
  }

  // Configure NTP with IST offset
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5000)) {
    lastSyncMessage = "NTP Fail";
    Serial.println("[TIME] Failed to get NTP time");
    return;
  }

  // Convert to DateTime for RTC
  DateTime now(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  rtc.adjust(now);

  curHour = now.hour();
  curMin  = now.minute();
  curSec  = now.second();
  curDay  = now.day();
  curMonth = now.month();
  curYear = now.year();
  curWeekday = now.dayOfTheWeek();
  curDayName = dayNames[curWeekday];
  curMonthName = monthNames[curMonth - 1];
  lastUnixTime = now.unixtime();

  updateDigits(curHour, curMin);

  timeSynced = true;

  char buf[20];
  snprintf(buf, sizeof(buf), "IST %02d:%02d:%02d", curHour, curMin, curSec);
  lastSyncMessage = String("Synced ") + String(buf);

  Serial.printf("[TIME] NTP Sync OK: %02d:%02d:%02d %s %02d %s %04d\n",
                curHour, curMin, curSec,
                curDayName.c_str(), curDay, curMonthName.c_str(), curYear);
}

// ============================================================
// PART 5/7 — BUTTONS + ALARM LOGIC (CURSOR UI + 3 ALARMS)
// ============================================================

// ============================================================
// --------------  ALARM MATCH + RING/SNOOZE ------------------
// ============================================================
bool alarmDayMatches(const DateTime &now, int idx) {
  uint8_t dow = now.dayOfTheWeek();   // 0=Sun
  if (alarms[idx].repeatMask == 0) return true; // If mask=0 → daily
  return (alarms[idx].repeatMask & (1 << dow));
}

void startAlarm(int idx) {
  if (!alarms[idx].enabled) return;
  alarms[idx].ringing = true;
  alarms[idx].snoozeUntil = 0;

  Serial.printf("[ALARM %d] RINGING!\n", idx+1);
  startRelayPulse(4000);
}

void stopAlarm(int idx, bool fromStopBtn) {
  if (!alarms[idx].ringing) return;

  alarms[idx].ringing = false;
  alarms[idx].snoozeUntil = 0;

  Serial.printf("[ALARM %d] STOP\n", idx+1);
  if (fromStopBtn) {
    startRelayPulse(4000);
  }
}

void snoozeAlarm(int idx, uint8_t min) {
  alarms[idx].ringing = false;
  alarms[idx].snoozeUntil = lastUnixTime + (unsigned long)min * 60UL;
  Serial.printf("[ALARM %d] Snooze %u minutes\n", idx+1, min);
}

void updateAlarmRTC(const DateTime &now) {
  unsigned long nowUnix = now.unixtime();

  for (int i = 0; i < 3; i++) {
    // If snooze active
    if (!alarms[i].ringing && alarms[i].snoozeUntil != 0 &&
        nowUnix >= alarms[i].snoozeUntil) {
      Serial.printf("[ALARM %d] Snooze time reached\n", i+1);
      startAlarm(i);
      continue;
    }

    if (!alarms[i].enabled || alarms[i].ringing) continue;

    // Check if alarm matches current time
    bool timeMatches = (now.hour() == alarms[i].hour &&
                        now.minute() == alarms[i].minute &&
                        now.second() == 0);

    if (!timeMatches) continue;

    // Check day condition
    if (alarmDayMatches(now, i)) {
      startAlarm(i);
    }
  }
}

// ===================================================
// SAVE / LOAD ALL 3 ALARMS TO FLASH
// ===================================================
void saveAlarmsToFlash() {
  prefs.begin("alarms", false);
  for (int i = 0; i < 3; i++) {
    String key = "a" + String(i);
    prefs.putInt((key + "h").c_str(), alarms[i].hour);
    prefs.putInt((key + "m").c_str(), alarms[i].minute);
    prefs.putUChar((key + "d").c_str(), alarms[i].repeatMask);
    prefs.putBool((key + "e").c_str(), alarms[i].enabled);
  }
  prefs.putInt("active", activeAlarm);
  prefs.end();
  Serial.println("[FLASH] 3 alarms saved");
}

void loadAlarmsFromFlash() {
  prefs.begin("alarms", true);
  for (int i = 0; i < 3; i++) {
    String key = "a" + String(i);
    alarms[i].hour       = prefs.getInt((key + "h").c_str(), (i==0?7:12));
    alarms[i].minute     = prefs.getInt((key + "m").c_str(), 0);
    alarms[i].repeatMask = prefs.getUChar((key + "d").c_str(), (i<2?0x7F:0x00));
    alarms[i].enabled    = prefs.getBool((key + "e").c_str(), false);
  }
  activeAlarm = prefs.getInt("active", 0);
  prefs.end();
  Serial.println("[FLASH] 3 alarms loaded");
}

// ============================================================
// ----------------------- BUTTONS ----------------------------
// ============================================================
void handleButtons() {
  
  static bool lastMode = HIGH;
  static bool lastUp   = HIGH;
  static bool lastDn   = HIGH;

  static unsigned long lastDebounce = 0;
  unsigned long now = millis();

  if (now - lastDebounce < 80) return;   // stable debounce

  bool modeNow = digitalRead(BTN_MODE);
  bool upNow   = digitalRead(BTN_UP);
  bool dnNow   = digitalRead(BTN_DOWN);

  bool modeFall = (lastMode == HIGH && modeNow == LOW);
  bool upFall   = (lastUp   == HIGH && upNow   == LOW);
  bool dnFall   = (lastDn   == HIGH && dnNow   == LOW);

  // FIX: only when actual press happens
  if (modeFall || upFall || dnFall) {
      lastInteraction = millis();
      screensaverMode = false;
  }

  // =============== RINGING MODE (priority) =================
  for (int i = 0; i < 3; i++) {
    if (alarms[i].ringing) {
      if (modeFall) { 
        stopAlarm(i, true);
      }
      else if (upFall) { 
        snoozeAlarm(i, 5);
      }
      else if (dnFall) { 
        snoozeAlarm(i, 15);
      }

      lastDebounce = now;
      lastMode=modeNow; lastUp=upNow; lastDn=dnNow;
      return;
    }
  }

  // =========================================================
  //               NORMAL BUTTON FUNCTION
  // =========================================================

  // ================= MODE BUTTON — SHORT + LONG (3 sec) =================

  static unsigned long modePressStart = 0;
  static bool modeLongHandled = false;

  bool modePressed = (modeNow == LOW);
  bool modeRelease = (!modePressed && lastMode == LOW);

  // Start detecting press
  if (modePressed && lastMode == HIGH) {
      modePressStart = now;
      modeLongHandled = false;
  }

  // LONG PRESS: 3 seconds
  bool modeLong = (modePressed && !modeLongHandled && (now - modePressStart >= 3000));

  // ================= LONG PRESS ACTION =================
  if (modeLong) {
      modeLongHandled = true;
      lastDebounce = now;

      // ENTER EDIT MODE
      if (currentPage == PAGE_ALARM && alarmState == ALARM_VIEW) {
          alarmState = ALARM_EDIT_HOUR;
          Serial.println("[MODE 3s] ENTER EDIT MODE");
      }
      // EXIT EDIT MODE → NEXT PAGE
      else if (currentPage == PAGE_ALARM && alarmState != ALARM_VIEW) {
          alarmState = ALARM_VIEW;
          currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
          saveAlarmsToFlash();
          Serial.println("[MODE 3s] EXIT EDIT → NEXT PAGE");
      }
      // Normal long press → next page
      else {
          currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
          Serial.println("[MODE 3s] NEXT PAGE");
      }

      return;
  }

  // ================= SHORT PRESS ACTION =================
  if (modeRelease) {
      lastDebounce = now;

      // If not in edit mode: short press = next page
      if (alarmState == ALARM_VIEW) {
          currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
          Serial.printf("[MODE SHORT] NEXT PAGE → %d\n", currentPage);
      }
      // If editing: move to next edit field
      else {
          switch (alarmState) {
              case ALARM_EDIT_HOUR:   alarmState = ALARM_EDIT_MIN;    break;
              case ALARM_EDIT_MIN:    alarmState = ALARM_EDIT_DAYS;   break;
              case ALARM_EDIT_DAYS:   alarmState = ALARM_EDIT_ENABLE; break;
              case ALARM_EDIT_ENABLE: alarmState = ALARM_EDIT_HOUR;   break;
          }
          Serial.printf("[MODE SHORT] NEXT EDIT FIELD → %d\n", alarmState);
      }
  }

  // ---------- UP button ----------
  if (upFall) {
      lastDebounce = now;

      if (currentPage == PAGE_ALARM) {
          if (alarmState == ALARM_VIEW) {
              activeAlarm = (activeAlarm + 1) % 3;           // Switch alarms
              Serial.printf("[UP] Switched to Alarm %d\n", activeAlarm+1);
          } else {
              switch (alarmState) {
                  case ALARM_EDIT_HOUR:
                      alarms[activeAlarm].hour = (alarms[activeAlarm].hour + 1) % 24;
                      break;

                  case ALARM_EDIT_MIN:
                      alarms[activeAlarm].minute = (alarms[activeAlarm].minute + 1) % 60;
                      break;

                  case ALARM_EDIT_DAYS:
                      alarmDayCursor = (alarmDayCursor + 1) % 7;
                      break;

                  case ALARM_EDIT_ENABLE:
                      if (upFall || dnFall) {
                          alarms[activeAlarm].enabled = !alarms[activeAlarm].enabled;
                          saveAlarmsToFlash();
                      }
                      break;

                  default:
                      // in VIEW, UP does nothing
                      break;
              }
              saveAlarmsToFlash();
          }
      }
      else if (currentPage == PAGE_BRIGHTNESS) {
          if (brightnessLevel < 10) brightnessLevel++;
      }
      else if (currentPage == PAGE_WIFI) {
          wifiEnabled = !wifiEnabled;
      }
      else if (currentPage == PAGE_MUSIC) {
          startRelayPulse(4000);   // ON pulse
      }
  }

  // ---------- DOWN button ----------
  if (dnFall) {
      lastDebounce = now;

      if (currentPage == PAGE_ALARM && alarmState != ALARM_VIEW) {
          switch (alarmState) {
              case ALARM_EDIT_HOUR:
                  alarms[activeAlarm].hour = (alarms[activeAlarm].hour + 23) % 24;
                  break;

              case ALARM_EDIT_MIN:
                  alarms[activeAlarm].minute = (alarms[activeAlarm].minute + 59) % 60;
                  break;

              case ALARM_EDIT_DAYS:
                  alarms[activeAlarm].repeatMask ^= (1 << alarmDayCursor);   // toggle day
                  break;

              case ALARM_EDIT_ENABLE:
                  if (upFall || dnFall) {
                      alarms[activeAlarm].enabled = !alarms[activeAlarm].enabled;
                      saveAlarmsToFlash();
                  }
                  break;

              default:
                  break;
          }
          saveAlarmsToFlash();
      }
      else if (currentPage == PAGE_BRIGHTNESS) {
          if (brightnessLevel > 0) brightnessLevel--;
      }
      else if (currentPage == PAGE_WIFI) {
          wifiEnabled = !wifiEnabled;
      }
      else if (currentPage == PAGE_MUSIC) {
          startRelayPulse(4000);   // OFF pulse
      }
  }

  lastMode = modeNow;
  lastUp   = upNow;
  lastDn   = dnNow;
}

// ============================================================
// ------------ ALARM LCD RENDER WITH CURSOR ------------------
// ============================================================
void drawAlarmLCD() {
  lcd.clear();

  // ===================== ALARM ENABLE PAGE =====================
  if (alarmState == ALARM_EDIT_ENABLE) {
    lcd.setCursor(0,0);
    lcd.print("Alarm ");
    lcd.print(activeAlarm + 1);

    lcd.setCursor(0,1);
    lcd.print("Enable: ");
    lcd.print(alarms[activeAlarm].enabled ? "ON " : "OFF");
    lcd.print(" UP:Toggle");

    return;
  }
  // =============================================================


  // NORMAL ALARM PAGE (VIEW + HOUR + MIN + DAYS)
  lcd.setCursor(0,0);
  lcd.print("Alarm ");
  lcd.print(activeAlarm + 1);
  lcd.print("  ");

  // HH:MM
  if (alarms[activeAlarm].hour < 10) lcd.print("0");
  lcd.print(alarms[activeAlarm].hour);
  lcd.print(":");
  if (alarms[activeAlarm].minute < 10) lcd.print("0");
  lcd.print(alarms[activeAlarm].minute);

  lcd.print(alarms[activeAlarm].enabled ? " ON" : " OFF");

  if (alarmState != ALARM_VIEW) {
    lcd.setCursor(14,0);
    lcd.print("E");
  }

  lcd.setCursor(0,1);

  if (alarmState == ALARM_VIEW) {
    lcd.print("UP:Next MODE:Edit");
  }
  else if (alarmState == ALARM_EDIT_DAYS) {
    lcd.print("D:");
    for (int i = 0; i < 7; i++) {
      bool on = alarms[activeAlarm].repeatMask & (1 << i);
      if (i == alarmDayCursor) {
        lcd.print("[");
        lcd.print(on ? dayCharsLCD[i] : '-');
        lcd.print("]");
      } else {
        lcd.print(on ? dayCharsLCD[i] : '-');
      }
    }
  } 
  else if (alarmState == ALARM_EDIT_HOUR) {
    lcd.print("Edit Hour  UP/DN");
  }
  else if (alarmState == ALARM_EDIT_MIN) {
    lcd.print("Edit Min   UP/DN");
  }
}

// ✅ FIX: function must be OUTSIDE
void renderAlarmRingingLCD(int idx) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ALARM ");
  lcd.print(idx+1);
  lcd.print(" RINGING");

  lcd.setCursor(0,1);
  lcd.print("STOP  SNOOZE");
}

// ============================================================
// SPARKLE TEXT CLEAR ANIMATION
// ============================================================
void sparkleClear() {
  for (int i = 0; i < 50; i++) {
    int x = random(0,16);
    int y = random(0,2);
    lcd.setCursor(x,y);
    lcd.print(" ");
    delay(5);
  }
  lcd.clear();
}

// ============================================================
// PART 6/7 — LCD RENDERER (ALL PAGES)
// ============================================================
void renderNotePage() {
  lcd.clear();
  int len = noteText.length();

  if (len <= 16) {
    lcd.setCursor(0,0);
    lcd.print(noteText);
    lcd.setCursor(0,1);
    lcd.print("MODE:Next");
  }
  else if (len <= 32) {
    lcd.setCursor(0,0);
    lcd.print(noteText.substring(0,16));
    lcd.setCursor(0,1);
    lcd.print(noteText.substring(16));
  }
  else {
    String line1="", line2="";
    for (int i=0; i<16; i++) {
      line1 += noteText[(noteScrollIndex + i) % len];
      line2 += noteText[(noteScrollIndex + 16 + i) % len];
    }
    lcd.setCursor(0,0); lcd.print(line1);
    lcd.setCursor(0,1); lcd.print(line2);
    noteScrollIndex = (noteScrollIndex + 1) % len;
  }
}

void renderClockPage() {
  lcd.clear();

  // Line 1 - Date DD MMM YYYY
  lcd.setCursor(0,0);
if (curDay < 10) lcd.print("0");
lcd.print(curDay);
lcd.print(" ");
lcd.print(curMonthName);
lcd.print(" ");
lcd.print(curYear % 100);
lcd.print(" ");
lcd.print(curDayName);   // <--- added


  // Line 2 - India Time + WiFi flag W1/W0
  lcd.setCursor(0,1);
  lcd.print("IN ");
  if (curHour < 10) lcd.print("0");
  lcd.print(curHour);
  lcd.print(":");
  if (curMin < 10) lcd.print("0");
  lcd.print(curMin);
  lcd.print(":");
  if (curSec < 10) lcd.print("0");
  lcd.print(curSec);
  lcd.print(" ");
  lcd.print(wifiConnected ? "W1" : "W0");
}

void renderBrightnessPage() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Brightness");
  lcd.setCursor(0,1);
  lcd.print("Level: ");
  if (brightnessLevel == 0) {
    lcd.print("AUTO");
  } else {
    lcd.print(brightnessLevel);
  }
  lcd.print("  UP/DN");
}

// ============================================================
// MUSIC PAGE
// ============================================================
void renderMusicPage() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Music Control");

  lcd.setCursor(0,1);
  lcd.print("UP:ON DN:OFF");
}

void renderWiFiPage() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Status");
  lcd.setCursor(0,1);

  if (!wifiEnabled) {
    lcd.print("Disabled");
    return;
  }

  if (!wifiConnected) {
    lcd.print("Connecting...");
  } else {
    lcd.print("IP:");
    lcd.print(WiFi.localIP().toString().substring(0,16));
  }
}

// ============================================================
// WEATHER ASCII ICONS
// ============================================================
String getWeatherIcon(String main) {
  if (main == null) return "WEATHER";
  
  main = main.toLowerCase();
  if (main.indexOf("clear") >= 0) return "SUN ☀";
  if (main.indexOf("cloud") >= 0) return "CLOUD ☁";
  if (main.indexOf("rain")  >= 0) return "RAIN ☂";
  if (main.indexOf("storm") >= 0) return "⚡ STORM";

  return "WEATHER";
}

void renderWeatherPage() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(getWeatherIcon(weatherMain));
  lcd.setCursor(0,1);

  if (!wifiConnected) {
    lcd.print("No WiFi");
    return;
  }

  // Refresh every 10 minutes
  if (millis() - lastWeatherFetch > 600000 || isnan(weatherTempC)) {
    fetchWeather();
  }

  if (isnan(weatherTempC)) {
    lcd.print("Fetching...");
    return;
  }

  // Show city and temperature
  lcd.print(weatherCity.substring(0,8));
  lcd.print(" ");
  lcd.print(String(weatherTempC,1));
  lcd.print("C");

  // Show weather condition on first line if space
  lcd.setCursor(8,0);
  if (weatherMain.length() <= 8) {
    lcd.print(weatherMain);
  }

  // CLOUD DRIFT ANIMATION
  weatherAnim = !weatherAnim;
  lcd.setCursor(weatherAnim ? 14 : 15, 0);
  lcd.print(" ");
  lcd.setCursor(weatherAnim ? 15 : 14, 0);
  lcd.print("☁");
}

void renderTimeSyncPage() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Time Sync NTP");
  lcd.setCursor(0,1);

  if (!wifiConnected) {
    lcd.print("No WiFi");
    return;
  }

  // Auto sync occasionally if not synced
  if (!timeSynced && millis() - lastSyncAttempt > 15000) {
    lastSyncAttempt = millis();
    syncTimeFromNet();
  }

  lcd.print(lastSyncMessage.substring(0,16));
}

// ============================================================
// ----------------- MASTER LCD UPDATE ------------------------
// ============================================================
void updateLCD() {
  // 🔔 Alarm ringing overrides all pages
for (int i = 0; i < 3; i++) {
    if (alarms[i].ringing) {
        renderAlarmRingingLCD(i);
        return;
    }
}

  if (millis() - lastLCDUpdate < 250) return;
  lastLCDUpdate = millis();

  // Run sparkle animation on page change (except alarm ringing & screensaver)
  if (!screensaverMode) {
      bool alarmActive = false;
      for (int i=0;i<3;i++){
          if (alarms[i].ringing) alarmActive = true;
      }
      if (!alarmActive && currentPage != lastPage) {
          sparkleClear();
          lastPage = currentPage;
      }
  }

  switch (currentPage) {
    case PAGE_CLOCK:
      renderClockPage();
      break;

    case PAGE_NOTE:
      renderNotePage();
      break;

    case PAGE_ALARM:
      drawAlarmLCD();  // Already clears LCD inside
      break;

    case PAGE_WIFI:
      renderWiFiPage();
      break;

    case PAGE_DHT:
      lcd.clear();
      if (millis() - lastDHTUpdate > 5000) {
        lastDHTUpdate = millis();
        lastTemp = dht.readTemperature();
        lastHum  = dht.readHumidity();
      }
      lcd.setCursor(0,0);
      lcd.print("Temp:");
      lcd.print(isnan(lastTemp) ? "Err" : String(lastTemp,1)+"C");
      lcd.setCursor(0,1);
      lcd.print("Hum:");
      lcd.print(isnan(lastHum) ? "Err" : String(lastHum,1)+"%");
      break;

    case PAGE_WEATHER:
      renderWeatherPage();
      break;

    case PAGE_TIME_SYNC:
      renderTimeSyncPage();
      break;

    case PAGE_BRIGHTNESS:
      renderBrightnessPage();
      break;

    case PAGE_MUSIC:
        renderMusicPage();
        break;
  }
}

// ============================================================
// PART 7/7 — RTC UPDATE + 7-SEG UPDATE + ALARM + RELAY
// ============================================================
void updateRTC() {
  if (millis() - lastRTCUpdate < 1000) return;
  lastRTCUpdate = millis();

  DateTime now = rtc.now();

  curHour = now.hour();
  curMin  = now.minute();
  curSec  = now.second();
  curDay  = now.day();
  curMonth = now.month();
  curYear = now.year();
  curWeekday = now.dayOfTheWeek();
  curDayName = dayNames[curWeekday];
  curMonthName = monthNames[curMonth - 1];
  lastUnixTime = now.unixtime();

  // Update 7-seg only for HH:MM
  updateDigits(curHour, curMin);

  // Alarm check
  updateAlarmRTC(now);
}

// ============================================================
// RELAY FUNCTIONS
// ============================================================
void startRelayPulse(unsigned long ms) {
  digitalWrite(RELAY_PIN, HIGH);
  relayOn = true;
  relayOffTime = millis() + ms;
  Serial.printf("[RELAY] ON for %lu ms\n", ms);
}

void updateRelay() {
  if (relayOn && millis() >= relayOffTime) {
    relayOn = false;
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("[RELAY] OFF");
  }
}

// ============================================================
// SERIAL HANDLER
// ============================================================
void handleSerial() {
  if (Serial.available()) {
    char cmd = Serial.read();
    // future serial commands
  }
}

// ============================================================
// SETUP AND LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  loadAlarmsFromFlash();  // Load 3 alarms from flash
  Serial.println("\nBOOT STARTING WITH 3 ALARMS...");

  // I2C
  Wire.begin(21, 22);

  // RTC
  if (!rtc.begin()) {
    Serial.println("RTC ERROR!");
  }

  // LCD
  lcd.begin();
  lcd.backlight();

  // DHT
  dht.begin();

  // 7-seg pins
  pinMode(A_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  pinMode(C_PIN, OUTPUT);
  pinMode(D_PIN, OUTPUT);

  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);

  // Buttons
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  // Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Read initial RTC time
  DateTime now = rtc.now();
  curHour = now.hour();
  curMin  = now.minute();
  curSec  = now.second();
  curDay  = now.day();
  curMonth = now.month();
  curYear = now.year();
  curWeekday = now.dayOfTheWeek();
  curDayName = dayNames[curWeekday];
  curMonthName = monthNames[curMonth - 1];
  lastUnixTime = now.unixtime();

  updateDigits(curHour, curMin);

  // Web server routes
  server.on("/", handleRoot);
  server.on("/save", handleSave);

  // RTOS Multiplex Task (Runs on Core-0 continuously)
  xTaskCreatePinnedToCore(
    muxTaskCode,
    "muxTask",
    2048,
    NULL,
    1,
    &muxTaskHandle,
    0
  );

  Serial.println("\nSYSTEM READY");
  Serial.println("Pages: CLOCK → NOTE → ALARM → BRIGHT → WIFI → DHT → WEATHER → TIME SYNC → MUSIC → CLOCK");
  Serial.println("Alarm Page: UP switches Alarm 1/2/3, MODE 3s enters edit, short MODE cycles fields");
}

void loop() {
  // ---------------- SCREENSAVER MODE HANDLING ----------------
unsigned long now = millis();

// Override screensaver for certain modes
if (currentPage == PAGE_BRIGHTNESS) {
    lastInteraction = now;
    screensaverMode = false;
}
if (currentPage == PAGE_ALARM && alarmState != ALARM_VIEW) {
    lastInteraction = now;
    screensaverMode = false;
}
for (int i=0;i<3;i++){
    if(alarms[i].ringing){
        screensaverMode = false;
        lastInteraction = now;
    }
}

// Enter screensaver after 15 sec inactivity
if (!screensaverMode && (now - lastInteraction > 15000)) {
    screensaverMode = true;
    lastScreenRotate = now;
    screensaverIndex = 0;
    currentPage = screensaverPages[0];
}

// Rotate pages every 4 sec
if (screensaverMode) {
    if (now - lastScreenRotate > 4000) {
        lastScreenRotate = now;
        screensaverIndex = (screensaverIndex + 1) % screensaverCount;
        currentPage = screensaverPages[screensaverIndex];
    }
}

  handleSerial();
  handleButtons();
  updateLCD();
  updateRTC();
  updateRelay();
  manageWiFi();

  if (wifiConnected) {
    server.handleClient();
  }
}
