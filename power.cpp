#include "power.h"

static uint16_t sampleBuffer[BAT_FILTER_SAMPLES] = {0};
static uint8_t sampleIndex = 0;
static bool bufferPrimed = false;

void powerInit() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(PIN_BATTERY_SENSE, ADC_11db);
  for (uint8_t i = 0; i < BAT_FILTER_SAMPLES; ++i) {
    sampleBuffer[i] = analogRead(PIN_BATTERY_SENSE);
  }
  bufferPrimed = true;
}

static uint8_t voltageToPercent(float v) {
  if (v <= CELL_MIN_V) return 0;
  if (v >= CELL_MAX_V) return 100;
  float pct = (v - CELL_MIN_V) / (CELL_MAX_V - CELL_MIN_V) * 100.0f;
  return static_cast<uint8_t>(pct);
}

static float readAveragedVoltage() {
  uint16_t raw = analogRead(PIN_BATTERY_SENSE);
  sampleBuffer[sampleIndex++] = raw;
  if (sampleIndex >= BAT_FILTER_SAMPLES) sampleIndex = 0;

  uint32_t acc = 0;
  uint8_t count = bufferPrimed ? BAT_FILTER_SAMPLES : (sampleIndex == 0 ? 1 : sampleIndex);
  for (uint8_t i = 0; i < count; ++i) {
    acc += sampleBuffer[i];
  }
  float avgRaw = static_cast<float>(acc) / static_cast<float>(count);
  float measured = (avgRaw / static_cast<float>(ADC_MAX)) * ADC_REFERENCE * VOLTAGE_DIVIDER_RATIO;
  measured = measured * BAT_CAL_SCALE + BAT_CAL_OFFSET;
  return measured;
}

BatteryStatus readBattery() {
  float measured = readAveragedVoltage();
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

