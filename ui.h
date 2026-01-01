#pragma once

#include <Arduino.h>
#include "config.h"
#include "audio.h"
#include "power.h"

struct ClockTime {
  uint8_t hour;
  uint8_t minute;
  bool valid;
};

enum class UIMode {
  DFP,
  BT
};

void uiInit();
void uiUpdate(const AudioStatus &audio, const BatteryStatus &battery, UIMode mode, const ClockTime &timeNow);
void uiPulse(const char *label);
void uiShowVolumeOverlay();

