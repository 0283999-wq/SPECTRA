#include "power.h"

void powerInit() {
  analogReadResolution(12);
}

static uint8_t voltageToPercent(float v) {
  if (v <= CELL_MIN_V) return 0;
  if (v >= CELL_MAX_V) return 100;
  float pct = (v - CELL_MIN_V) / (CELL_MAX_V - CELL_MIN_V) * 100.0f;
  return static_cast<uint8_t>(pct);
}

BatteryStatus readBattery() {
  uint16_t raw = analogRead(PIN_BATTERY_SENSE);
  float measured = (static_cast<float>(raw) / ADC_MAX) * ADC_REFERENCE * VOLTAGE_DIVIDER_RATIO;
  uint8_t pct = voltageToPercent(measured);
  BatteryLevel lvl = BatteryLevel::Red;
  if (pct > 40) {
    lvl = BatteryLevel::Green;
  } else if (pct > 15) {
    lvl = BatteryLevel::Amber;
  }

  BatteryStatus status{};
  status.voltage = measured;
  status.percent = pct;
  status.level = lvl;
  return status;
}

