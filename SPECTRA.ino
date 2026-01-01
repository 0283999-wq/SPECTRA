#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "audio.h"
#include "input.h"
#include "power.h"
#include "ui.h"

static unsigned long lastBatteryRead = 0;
static BatteryStatus cachedBattery{};
static UIMode currentMode = UIMode::DFP;
static bool rtcPresent = false;
static unsigned long rtcFallbackStart = 0;

static uint8_t bcdToDec(uint8_t val) {
  return ((val / 16) * 10) + (val % 16);
}

static void rtcInit() {
  Wire.begin();
  Wire.beginTransmission(0x68);
  rtcPresent = (Wire.endTransmission() == 0);
  rtcFallbackStart = millis();
}

static ClockTime rtcNow() {
  ClockTime t{};
  if (rtcPresent) {
    Wire.beginTransmission(0x68);
    Wire.write((uint8_t)0x00);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x68, (uint8_t)3) == 3) {
      uint8_t ss = Wire.read();
      uint8_t mm = Wire.read();
      uint8_t hh = Wire.read();
      t.minute = bcdToDec(mm);
      t.hour = bcdToDec(hh & 0x3F);
      t.valid = true;
      return t;
    }
    rtcPresent = false;
  }

  unsigned long elapsedMinutes = ((millis() - rtcFallbackStart) / 60000UL);
  unsigned long startMinutes = (MANUAL_TIME_START_HOUR % 24) * 60UL + (MANUAL_TIME_START_MIN % 60);
  unsigned long totalMinutes = (startMinutes + elapsedMinutes) % 1440UL;
  t.hour = (totalMinutes / 60) % 24;
  t.minute = totalMinutes % 60;
  t.valid = false;
  return t;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== SPECTRA SETUP START ===");
  powerInit();
  inputInit();
  audioInit();
  rtcInit();
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
      }
      break;
    case InputEvent::Next:
      if (currentMode == UIMode::DFP) {
        audioNext();
        uiPulse("TRACK >>");
      }
      break;
    case InputEvent::Prev:
      if (currentMode == UIMode::DFP) {
        audioPrev();
        uiPulse("TRACK <<");
      }
      break;
    case InputEvent::VolUp:
      if (currentMode == UIMode::DFP) {
        if (audioVolumeUp()) {
          uiShowVolumeOverlay();
        }
      }
      break;
    case InputEvent::VolDown:
      if (currentMode == UIMode::DFP) {
        if (audioVolumeDown()) {
          uiShowVolumeOverlay();
        }
      }
      break;
    case InputEvent::ModeToggle:
      currentMode = (currentMode == UIMode::DFP) ? UIMode::BT : UIMode::DFP;
      uiPulse("MODE");
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

  ClockTime nowClock = rtcNow();
  uiUpdate(getAudioStatus(), cachedBattery, currentMode, nowClock);
}

