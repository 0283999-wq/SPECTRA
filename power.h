#pragma once

#include <Arduino.h>
#include "config.h"

enum class BatteryLevel {
  Green,
  Amber,
  Red
};

struct BatteryStatus {
  float voltage;
  uint8_t percent;
  BatteryLevel level;
};

void powerInit();
BatteryStatus readBattery();

