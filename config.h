#pragma once

#include <Arduino.h>

// Pin definitions
static const uint8_t PIN_SCREEN_SCK = 18;
static const uint8_t PIN_SCREEN_MOSI = 23;
static const uint8_t PIN_SCREEN_DC = 16;
static const uint8_t PIN_SCREEN_CS = 17;
static const uint8_t PIN_SCREEN_RST = 5;

#define SCREEN_ROTATION 2

static const uint8_t PIN_DFPLAYER_TX = 26; // DFPlayer TX -> ESP32 RX pin
static const uint8_t PIN_DFPLAYER_RX = 27; // DFPlayer RX -> ESP32 TX pin

static const uint8_t PIN_TOUCH_PLAY = 32;
static const uint8_t PIN_TOUCH_NEXT = 33;
static const uint8_t PIN_TOUCH_PREV = 25;

static const uint8_t PIN_BTN_VOL_DOWN = 21;
static const uint8_t PIN_BTN_VOL_UP = 22;

static const uint8_t PIN_BATTERY_SENSE = 35;

// Audio settings
static const uint16_t DEFAULT_TRACK = 1;
static const uint8_t MIN_VOLUME = 0;
static const uint8_t MAX_VOLUME = 30;
static const uint8_t DEFAULT_VOLUME = 20;

// Debounce timings (milliseconds)
static const uint16_t DEBOUNCE_MS = 50;
static const uint16_t HOLD_MS = 250;
static const uint16_t REPEAT_MS = 80;
static const uint16_t MODE_TOGGLE_HOLD_MS = 2000;

// Battery measurement
static const float ADC_REFERENCE = 3.3f;
static const uint16_t ADC_MAX = 4095;
static const float VOLTAGE_DIVIDER_RATIO = 2.0f; // 100k/100k divider doubles voltage
static const float CELL_MIN_V = 3.3f;
static const float CELL_MAX_V = 4.2f;

// Manual time seed (used if no RTC present)
static const uint8_t MANUAL_TIME_START_HOUR = 10; // set to your local hour
static const uint8_t MANUAL_TIME_START_MIN = 50;  // set to your local minute

// UI timing
static const uint16_t UI_ANIM_MS = 350;
static const uint16_t UI_VOLUME_OVERLAY_MS = 900;
static const uint16_t UI_SCANLINE_SPACING = 6;
static const uint16_t UI_HUD_REFRESH_MS = 100;
static const uint16_t UI_BATTERY_REFRESH_MS = 2000;
static const uint32_t UI_BACKGROUND_REFRESH_MS = 60000;

// UI thresholds
static const uint8_t UI_WARNING_THRESHOLD = 15; // battery percent

// Colors
static const uint16_t COLOR_BG = 0x0000; // Black
static const uint16_t COLOR_GRID = 0x19E7; // Dim cyan grid
static const uint16_t COLOR_TEXT = 0x07E0; // Green
static const uint16_t COLOR_ACCENT = 0x05DF; // Cyan accent
static const uint16_t COLOR_AMBER = 0xFD20; // Amber/Orange
static const uint16_t COLOR_DARK = 0x2104; // Dark slate
static const uint16_t COLOR_PANEL = 0x0841; // Very dark blue-green
static const uint16_t COLOR_WARNING = 0xF980; // Warm amber/red

// UI layout
static const uint16_t SCREEN_SIZE = 240;
static const uint16_t CENTER_X = SCREEN_SIZE / 2;
static const uint16_t CENTER_Y = SCREEN_SIZE / 2;
static const uint16_t UI_SAFE_RADIUS = 110;
static const uint16_t UI_SAFE_DIAMETER = UI_SAFE_RADIUS * 2;
static const uint16_t UI_SAFE_LEFT = CENTER_X - UI_SAFE_RADIUS;
static const uint16_t UI_SAFE_TOP = CENTER_Y - UI_SAFE_RADIUS;

