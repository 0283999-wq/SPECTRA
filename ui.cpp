#include "ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <math.h>
#include "vinyl_ui.h"

static Adafruit_GC9A01A display = Adafruit_GC9A01A(PIN_SCREEN_CS, PIN_SCREEN_DC, PIN_SCREEN_MOSI, PIN_SCREEN_SCK, PIN_SCREEN_RST);

struct UIStateCache {
  bool initialized = false;
  AudioStatus audio{};
  BatteryStatus battery{};
  UIMode mode = UIMode::DFP;
  uint16_t minuteTick = 0;
  ClockTime clock{};
};

static UIStateCache uiCache;
static unsigned long lastHudRefresh = 0;
static unsigned long lastBatteryRefresh = 0;
static unsigned long lastBackgroundRefresh = 0;
static unsigned long lastPulse = 0;
static unsigned long volumeOverlayUntilMs = 0;
static bool backgroundDrawn = false;
static bool volumeOverlayActive = false;
static float overlayVolumeLerp = DEFAULT_VOLUME;
static uint8_t btAnimPhase = 0;
static unsigned long lastBtAnim = 0;
static unsigned long lastVinylStep = 0;
static uint16_t vinylAngle = 0;
static bool overlayPendingClear = false;
static bool hudNeedsRestore = false;

static void formatTime(char *buf, size_t len, const ClockTime &clock) {
  uint8_t hours = clock.hour % 24;
  uint8_t minutes = clock.minute % 60;
  bool pm = hours >= 12;
  uint8_t displayHour = hours % 12;
  if (displayHour == 0) displayHour = 12;
  snprintf(buf, len, "%02u:%02u %s", displayHour, minutes, pm ? "PM" : "AM");
}

static void drawSafeGridLineV(int16_t x) {
  int16_t dx = x - CENTER_X;
  if (abs(dx) > UI_SAFE_RADIUS) return;
  float span = sqrt((float)(UI_SAFE_RADIUS * UI_SAFE_RADIUS - dx * dx));
  int16_t y0 = CENTER_Y - (int16_t)span;
  int16_t y1 = CENTER_Y + (int16_t)span;
  display.drawFastVLine(x, y0, y1 - y0, COLOR_GRID);
}

static void drawSafeGridLineH(int16_t y) {
  int16_t dy = y - CENTER_Y;
  if (abs(dy) > UI_SAFE_RADIUS) return;
  float span = sqrt((float)(UI_SAFE_RADIUS * UI_SAFE_RADIUS - dy * dy));
  int16_t x0 = CENTER_X - (int16_t)span;
  int16_t x1 = CENTER_X + (int16_t)span;
  display.drawFastHLine(x0, y, x1 - x0, COLOR_GRID);
}

static void drawBackground() {
  display.fillScreen(COLOR_BG);
  display.drawRGBBitmap(0, 0, VINYL_UI_BITMAP_PTR, VINYL_UI_WIDTH, VINYL_UI_HEIGHT);
  display.drawCircle(CENTER_X, CENTER_Y, UI_SAFE_RADIUS, COLOR_DARK);
  display.drawCircle(CENTER_X, CENTER_Y, UI_SAFE_RADIUS - 2, COLOR_PANEL);

  for (int16_t x = UI_SAFE_LEFT + 10; x < UI_SAFE_LEFT + UI_SAFE_DIAMETER; x += 16) {
    drawSafeGridLineV(x);
  }
  for (int16_t y = UI_SAFE_TOP + 12; y < UI_SAFE_TOP + UI_SAFE_DIAMETER; y += 16) {
    drawSafeGridLineH(y);
  }

  for (uint16_t r = 18; r < UI_SAFE_RADIUS; r += 24) {
    display.drawCircle(CENTER_X, CENTER_Y, r, COLOR_DARK);
  }
}

static void blitVinylRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
  uint16_t lineBuf[UI_SAFE_DIAMETER];
  int16_t x0 = max<int16_t>(0, x);
  int16_t y0 = max<int16_t>(0, y);
  int16_t x1 = min<int16_t>(VINYL_UI_WIDTH, x + w);
  int16_t y1 = min<int16_t>(VINYL_UI_HEIGHT, y + h);
  for (int16_t row = y0; row < y1; ++row) {
    int idx = row * VINYL_UI_WIDTH + x0;
    for (int16_t col = x0; col < x1; ++col) {
      lineBuf[col - x0] = pgm_read_word(&VINYL_UI_BITMAP_PTR[idx++]);
    }
    display.drawRGBBitmap(x0, row, lineBuf, x1 - x0, 1);
  }
}

static void drawPanel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill) {
  display.fillRoundRect(x, y, w, h, 8, fill);
  display.drawRoundRect(x, y, w, h, 8, border);
}

static void drawTopBar(const BatteryStatus &bat, const char *timeStr, bool warn) {
  const int16_t x = UI_SAFE_LEFT + 10;
  const int16_t y = UI_SAFE_TOP + 8;
  const int16_t w = UI_SAFE_DIAMETER - 20;
  const int16_t h = 20;
  uint16_t border = warn ? COLOR_AMBER : COLOR_ACCENT;
  uint16_t fill = warn ? COLOR_WARNING : COLOR_PANEL;
  drawPanel(x, y, w, h, border, fill);

  display.setTextSize(1);
  display.setTextColor(warn ? COLOR_BG : COLOR_TEXT, fill);
  display.setCursor(x + 6, y + 6);
  if (warn) {
    display.print("WARNING!");
  } else {
    display.print("SYSTEM ONLINE");
  }

  display.setTextColor(COLOR_ACCENT, fill);
  int16_t timeX = x + w - 64;
  display.setCursor(timeX, y + 6);
  display.print(timeStr);
}

static void drawTrackPanel(const AudioStatus &audio, bool pulse) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 36;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 32;
  uint16_t border = pulse ? COLOR_AMBER : COLOR_ACCENT;
  drawPanel(x, y, w, h, border, COLOR_PANEL);

  display.setTextSize(2);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 8);
  display.print("TRACK ");
  char buf[6];
  snprintf(buf, sizeof(buf), "%04d", audio.track);
  display.print(buf);

  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  display.setCursor(x + w - 54, y + 10);
  display.print("/");
  snprintf(buf, sizeof(buf), "%03d", audio.trackCount);
  display.print(buf);
}

static void drawStatePanel(const AudioStatus &audio, bool pulse) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 74;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 26;
  uint16_t border = pulse ? COLOR_AMBER : COLOR_ACCENT;
  drawPanel(x, y, w, h, border, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 8);
  display.print(audio.state == PlaybackState::Playing ? "PLAY" : "PAUSE");

  display.setCursor(x + w - 70, y + 8);
  display.setTextColor(audio.online ? COLOR_ACCENT : COLOR_AMBER, COLOR_PANEL);
  display.print(audio.online ? "AUDIO CORE" : "AUDIO OFFLINE");
}

static void drawVolumePanel(const AudioStatus &audio) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 164;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 24;
  drawPanel(x, y, w, h, COLOR_ACCENT, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  display.setCursor(x + 10, y + 7);
  display.print("VOL");

  uint16_t barX = x + 36;
  uint16_t barY = y + 7;
  uint16_t barW = w - 60;
  uint16_t barH = h - 12;
  display.drawRoundRect(barX, barY, barW, barH, 3, COLOR_ACCENT);
  uint16_t fill = map(audio.volume, MIN_VOLUME, MAX_VOLUME, 0, barW - 4);
  display.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 2, COLOR_TEXT);
}

static void drawBatteryPanel(const BatteryStatus &bat) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 118;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 32;
  drawPanel(x, y, w, h, COLOR_ACCENT, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 8);
  display.print("BAT ");
  display.print(bat.voltage, 2);
  display.print("V  ");
  display.print(bat.percent);
  display.print("%");

  uint16_t iconX = x + w - 58;
  uint16_t iconY = y + 6;
  uint16_t iconW = 40;
  uint16_t iconH = h - 12;
  display.drawRoundRect(iconX, iconY, iconW, iconH, 3, COLOR_GRID);
  display.fillRect(iconX + iconW, iconY + 4, 4, iconH - 8, COLOR_GRID);

  uint16_t fill = map(bat.percent, 0, 100, 0, iconW - 4);
  uint16_t color = COLOR_AMBER;
  if (bat.level == BatteryLevel::Green) color = COLOR_TEXT;
  if (bat.level == BatteryLevel::Red) color = COLOR_WARNING;
  display.fillRoundRect(iconX + 2, iconY + 2, fill, iconH - 4, 2, color);
}

static void drawMessagePanel(const char *timeStr) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 100;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 16;
  display.fillRoundRect(x, y, w, h, 6, COLOR_BG);
  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_BG);
  display.setCursor(x + 6, y + 4);
  display.print("TIME ");
  display.print(timeStr);
}

static void drawVinylSpinner(bool spinning) {
  const int16_t size = 140;
  const int16_t x = CENTER_X - size / 2;
  const int16_t y = CENTER_Y - size / 2;
  blitVinylRegion(x - 2, y - 2, size + 4, size + 4);
  if (!spinning) return;

  uint16_t rOuter = size / 2 - 6;
  uint16_t rInner = 26;
  for (uint8_t i = 0; i < 6; ++i) {
    float a = (vinylAngle + i * 60) * DEG_TO_RAD;
    int16_t x0 = CENTER_X + cos(a) * rInner;
    int16_t y0 = CENTER_Y + sin(a) * rInner;
    int16_t x1 = CENTER_X + cos(a) * rOuter;
    int16_t y1 = CENTER_Y + sin(a) * rOuter;
    display.drawLine(x0, y0, x1, y1, COLOR_ACCENT);
  }
  display.fillCircle(CENTER_X, CENTER_Y, 8, COLOR_PANEL);
  display.drawCircle(CENTER_X, CENTER_Y, 10, COLOR_ACCENT);
}

static void drawVolumeOverlayBar(uint8_t volume, float lerpValue) {
  const int16_t w = UI_SAFE_DIAMETER - 34;
  const int16_t h = 26;
  const int16_t x = UI_SAFE_LEFT + 17;
  const int16_t y = CENTER_Y - h / 2;

  display.fillRoundRect(x, y, w, h, 8, COLOR_BG);
  display.drawRoundRect(x, y, w, h, 8, COLOR_ACCENT);
  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_BG);
  display.setCursor(x + 10, y + 8);
  display.print("VOL ");
  display.print(volume);

  uint16_t barX = x + 58;
  uint16_t barY = y + 8;
  uint16_t barW = w - 72;
  uint16_t barH = 10;
  display.drawRoundRect(barX, barY, barW, barH, 3, COLOR_ACCENT);
  uint16_t fill = map((uint16_t)lerpValue, MIN_VOLUME, MAX_VOLUME, 0, barW - 4);
  display.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 2, COLOR_TEXT);
}

static void updateVolumeOverlay(const AudioStatus &audio, const BatteryStatus &bat, const ClockTime &clock, unsigned long now) {
  if (!volumeOverlayActive && !overlayPendingClear) return;

  if (volumeOverlayActive) {
    overlayVolumeLerp = overlayVolumeLerp + (audio.volume - overlayVolumeLerp) * 0.45f;
    drawVolumeOverlayBar(audio.volume, overlayVolumeLerp);
    if (now > volumeOverlayUntilMs) {
      volumeOverlayActive = false;
      overlayPendingClear = true;
      hudNeedsRestore = true;
    }
  }

  if (overlayPendingClear && !volumeOverlayActive) {
    const int16_t w = UI_SAFE_DIAMETER - 34;
    const int16_t h = 26;
    const int16_t x = UI_SAFE_LEFT + 17;
    const int16_t y = CENTER_Y - h / 2;
    blitVinylRegion(x - 2, y - 2, w + 4, h + 4);
    // redraw HUD elements overlapped by overlay region only
    char timeBuf[10];
    formatTime(timeBuf, sizeof(timeBuf), clock);
    drawMessagePanel(timeBuf);
    drawBatteryPanel(bat);
    drawVolumePanel(audio);
    overlayPendingClear = false;
  }
}

static void drawBtHud(const BatteryStatus &bat, const ClockTime &clock, unsigned long now) {
  char timeBuf[10];
  formatTime(timeBuf, sizeof(timeBuf), clock);

  // top header
  const int16_t headerX = UI_SAFE_LEFT + 10;
  const int16_t headerY = UI_SAFE_TOP + 10;
  const int16_t headerW = UI_SAFE_DIAMETER - 20;
  drawPanel(headerX, headerY, headerW, 26, COLOR_ACCENT, COLOR_PANEL);
  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(headerX + 10, headerY + 8);
  display.print("BLUETOOTH");
  display.setCursor(headerX + headerW - 54, headerY + 8);
  display.print(timeBuf);

  // battery badge
  BatteryStatus localBat = bat;
  drawBatteryPanel(localBat);

  // status card
  const int16_t cardX = UI_SAFE_LEFT + 18;
  const int16_t cardY = UI_SAFE_TOP + 50;
  const int16_t cardW = UI_SAFE_DIAMETER - 36;
  const int16_t cardH = 46;
  drawPanel(cardX, cardY, cardW, cardH, COLOR_ACCENT, COLOR_PANEL);
  display.setTextSize(2);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(cardX + 12, cardY + 14);
  display.print("LINK ");
  display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  display.print("STANDBY");

  // Now playing bar animation
  const int16_t barX = UI_SAFE_LEFT + 24;
  const int16_t barY = cardY + cardH + 12;
  const int16_t barW = UI_SAFE_DIAMETER - 48;
  const int16_t barH = 18;
  display.fillRoundRect(barX, barY, barW, barH, 8, COLOR_BG);
  display.drawRoundRect(barX, barY, barW, barH, 8, COLOR_ACCENT);

  if (now - lastBtAnim > 120) {
    lastBtAnim = now;
    btAnimPhase = (btAnimPhase + 1) % 10;
  }
  int16_t segW = (barW - 12) / 10;
  for (uint8_t i = 0; i < 10; ++i) {
    uint16_t color = (i == btAnimPhase) ? COLOR_TEXT : COLOR_GRID;
    display.fillRoundRect(barX + 6 + i * segW, barY + 4, segW - 4, barH - 8, 4, color);
  }
}

static bool audioChanged(const AudioStatus &a, const AudioStatus &b) {
  return a.track != b.track || a.volume != b.volume || a.state != b.state || a.online != b.online || a.trackCount != b.trackCount;
}

static bool batteryChanged(const BatteryStatus &a, const BatteryStatus &b) {
  return a.percent != b.percent || a.level != b.level || fabs(a.voltage - b.voltage) > 0.02f;
}

void uiInit() {
  display.begin();
  display.setRotation(SCREEN_ROTATION);
  display.setTextWrap(false);
  drawBackground();
  backgroundDrawn = true;
  lastBackgroundRefresh = millis();
  uiCache.initialized = false;
}

void uiUpdate(const AudioStatus &audio, const BatteryStatus &battery, UIMode mode, const ClockTime &timeNow) {
  unsigned long now = millis();
  char timeBuf[10];
  formatTime(timeBuf, sizeof(timeBuf), timeNow);
  uint16_t minuteTick = ((timeNow.hour % 24) * 60) + (timeNow.minute % 60);

  if (!backgroundDrawn || now - lastBackgroundRefresh > UI_BACKGROUND_REFRESH_MS) {
    drawBackground();
    backgroundDrawn = true;
    lastBackgroundRefresh = now;
    uiCache.initialized = false;
  }

  bool pulseActive = (now - lastPulse) <= UI_ANIM_MS;
  bool audioDiff = !uiCache.initialized || audioChanged(audio, uiCache.audio) || pulseActive;
  bool batDiff = !uiCache.initialized || batteryChanged(battery, uiCache.battery);
  bool modeDiff = !uiCache.initialized || mode != uiCache.mode;
  bool timeDiff = !uiCache.initialized || minuteTick != uiCache.minuteTick;

  if (modeDiff) {
    hudNeedsRestore = true;
    if (mode == UIMode::BT) {
      volumeOverlayActive = false;
      overlayPendingClear = false;
    }
  }

  if (mode == UIMode::DFP) {
    bool warn = (battery.percent < UI_WARNING_THRESHOLD || battery.level == BatteryLevel::Red);
    if (batDiff || timeDiff || modeDiff || hudNeedsRestore) {
      drawTopBar(battery, timeBuf, warn);
    }

    if ((audioDiff || modeDiff || timeDiff || hudNeedsRestore) && (hudNeedsRestore || now - lastHudRefresh >= UI_HUD_REFRESH_MS)) {
      drawTrackPanel(audio, pulseActive);
      drawStatePanel(audio, pulseActive);
      drawMessagePanel(timeBuf);
      drawVolumePanel(audio);
      lastHudRefresh = now;
      hudNeedsRestore = false;
    }

    if (batDiff || (now - lastBatteryRefresh >= UI_BATTERY_REFRESH_MS)) {
      drawBatteryPanel(battery);
      lastBatteryRefresh = now;
    }

    bool spinning = (audio.state == PlaybackState::Playing) && audio.online;
    if (spinning && (now - lastVinylStep > 90)) {
      vinylAngle = (vinylAngle + 5) % 360;
      drawVinylSpinner(true);
      lastVinylStep = now;
    } else if (!spinning) {
      drawVinylSpinner(false);
    }

    updateVolumeOverlay(audio, battery, timeNow, now);
  } else {
    if (batDiff || timeDiff || modeDiff) {
      drawBtHud(battery, timeNow, now);
    }
    if (now - lastBtAnim > 120) {
      drawBtHud(battery, timeNow, now);
    }
  }

  uiCache.audio = audio;
  uiCache.battery = battery;
  uiCache.mode = mode;
  uiCache.minuteTick = minuteTick;
  uiCache.clock = timeNow;
  uiCache.initialized = true;
}

void uiPulse(const char *label) {
  (void)label;
  lastPulse = millis();
}

void uiShowVolumeOverlay() {
  unsigned long now = millis();
  volumeOverlayActive = true;
  overlayPendingClear = false;
  volumeOverlayUntilMs = now + UI_VOLUME_OVERLAY_MS;
  overlayVolumeLerp = uiCache.initialized ? uiCache.audio.volume : DEFAULT_VOLUME;
}
