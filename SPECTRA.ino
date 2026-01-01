/*
 * SPECTRA firmware sketch
 * Target: ESP32 NodeMCU + GC9A01 round display + DFPlayer Mini
 * Framework: Arduino (no WiFi/Bluetooth stacks used)
 * Required libraries:
 *   - Adafruit_GFX
 *   - Adafruit_GC9A01A
 *   - DFRobotDFPlayerMini
 *   - Preferences (bundled with ESP32 core)
 * Assets:
 *   - vinyl_assets.h (exports either vinyl_ui_bitmap or image_data_Image)
 */

#include <Arduino.h>
#include <Preferences.h>
#include <cstring>
#include "config.h"
#include "audio.h"
#include "input.h"
#include "power.h"
#include "ui.h"

static unsigned long lastBatteryRead = 0;
static BatteryStatus cachedBattery{};
static UIMode currentMode = UIMode::DFP;

struct Settings {
  uint16_t lastTrack = DEFAULT_TRACK;
  uint8_t lastVolume = DEFAULT_VOLUME;
  int16_t timeOffsetMin = 0;
};

static Settings settings{};
static Preferences prefs;

class SoftClock {
 public:
  void begin(int16_t offsetMinutes) {
    baseMinutes = compileMinutes();
    offset = offsetMinutes;
    startMillis = millis();
  }

  ClockTime now() const {
    unsigned long elapsedMinutes = (millis() - startMillis) / 60000UL;
    unsigned long minutes = (baseMinutes + offset + elapsedMinutes) % 1440UL;
    ClockTime t{};
    t.hour = minutes / 60;
    t.minute = minutes % 60;
    t.valid = true;
    return t;
  }

  void adjustMinutes(int16_t delta) {
    offset += delta;
  }

  int16_t offsetMinutes() const { return offset; }

  void setTime(uint8_t hour, uint8_t minute) {
    unsigned long elapsedMinutes = (millis() - startMillis) / 60000UL;
    unsigned long target = (hour % 24) * 60UL + (minute % 60);
    offset = static_cast<int16_t>(target) - static_cast<int16_t>(baseMinutes + elapsedMinutes);
  }

 private:
  unsigned long compileMinutes() const {
    // __DATE__ format: "Mmm dd yyyy"; __TIME__ format: "hh:mm:ss"
    const char monthNames[][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char monthStr[4];
    int day, year, hour, minute, second;
    sscanf(__DATE__, "%3s %d %d", monthStr, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    uint8_t month = 0;
    for (uint8_t i = 0; i < 12; ++i) {
      if (strncmp(monthStr, monthNames[i], 3) == 0) { month = i; break; }
    }
    // approximate minutes of day only; date not used for wrapping
    (void)month; (void)day; (void)year; (void)second;
    return (hour % 24) * 60UL + (minute % 60);
  }

  unsigned long baseMinutes = 0;
  int16_t offset = 0;
  unsigned long startMillis = 0;
};

static SoftClock clockMgr;
static uint8_t editHour = MANUAL_TIME_START_HOUR;
static uint8_t editMinute = MANUAL_TIME_START_MIN;
static bool editingHour = true;
static bool timeEditActive = false;

static void loadSettings() {
  prefs.begin("spectra", false);
  settings.lastTrack = prefs.getUShort("track", DEFAULT_TRACK);
  settings.lastVolume = prefs.getUChar("vol", DEFAULT_VOLUME);
  settings.timeOffsetMin = prefs.getShort("tofs", 0);
}

static void persistSettings() {
  prefs.putUShort("track", settings.lastTrack);
  prefs.putUChar("vol", settings.lastVolume);
  prefs.putShort("tofs", settings.timeOffsetMin);
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== SPECTRA SETUP START ===");
  loadSettings();
  powerInit();
  inputInit();
  audioApplySettings(settings.lastTrack, settings.lastVolume);
  audioInit();
  clockMgr.begin(settings.timeOffsetMin);
  uiInit();
  cachedBattery = readBattery();
  Serial.println("=== SPECTRA SETUP END ===");
}

void loop() {
  InputEvent ev = inputPoll();
  switch (ev) {
    case InputEvent::PlayPause:
      if (currentMode == UIMode::DFP) {
        audioTogglePause();
        uiPulse("PLAYBACK");
      } else if (currentMode == UIMode::TimeSet) {
        clockMgr.setTime(editHour, editMinute);
        settings.timeOffsetMin = clockMgr.offsetMinutes();
        persistSettings();
        currentMode = UIMode::DFP;
        timeEditActive = false;
      }
      break;
    case InputEvent::Next:
      if (currentMode == UIMode::DFP) {
        audioNext();
        settings.lastTrack = getAudioStatus().track;
        persistSettings();
        uiPulse("TRACK >>");
      } else if (currentMode == UIMode::TimeSet) {
        editingHour = !editingHour;
      }
      break;
    case InputEvent::Prev:
      if (currentMode == UIMode::DFP) {
        audioPrev();
        settings.lastTrack = getAudioStatus().track;
        persistSettings();
        uiPulse("TRACK <<");
      }
      break;
    case InputEvent::VolUp:
      if (currentMode == UIMode::DFP) {
        if (audioVolumeUp()) {
          settings.lastVolume = getAudioStatus().volume;
          persistSettings();
          uiShowVolumeOverlay();
        }
      } else if (currentMode == UIMode::TimeSet) {
        if (editingHour) {
          editHour = (editHour + 1) % 24;
        } else {
          editMinute = (editMinute + 1) % 60;
        }
      }
      break;
    case InputEvent::VolDown:
      if (currentMode == UIMode::DFP) {
        if (audioVolumeDown()) {
          settings.lastVolume = getAudioStatus().volume;
          persistSettings();
          uiShowVolumeOverlay();
        }
      } else if (currentMode == UIMode::TimeSet) {
        if (editingHour) {
          editHour = (editHour + 23) % 24;
        } else {
          editMinute = (editMinute + 59) % 60;
        }
      }
      break;
    case InputEvent::ModeToggle:
      if (currentMode == UIMode::TimeSet) break;
      currentMode = (currentMode == UIMode::DFP) ? UIMode::BT : UIMode::DFP;
      uiPulse("MODE");
      break;
    case InputEvent::EnterTimeSet:
      if (currentMode == UIMode::DFP) {
        ClockTime nowClock = clockMgr.now();
        editHour = nowClock.hour;
        editMinute = nowClock.minute;
        editingHour = true;
        timeEditActive = true;
        currentMode = UIMode::TimeSet;
        uiPulse("TIME");
      }
      break;
    default:
      break;
  }

  audioLoop();

  unsigned long now = millis();
  if (now - lastBatteryRead > 2000) {
    cachedBattery = readBattery();
    lastBatteryRead = now;
  }

  ClockTime nowClock = clockMgr.now();
  if (currentMode == UIMode::TimeSet) {
    uiSyncTimeEdit(editHour, editMinute, editingHour);
  }
  uiUpdate(getAudioStatus(), cachedBattery, currentMode, nowClock);
}

