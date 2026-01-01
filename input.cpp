#include "input.h"

struct ButtonState {
  uint8_t pin;
  bool activeHigh;
  bool lastStable;
  bool lastReading;
  bool stablePressed;
  unsigned long pressedAt;
  unsigned long lastChange;
  unsigned long lastRepeat;
};

static ButtonState touchPlay{PIN_TOUCH_PLAY, true, false, false, false, 0, 0, 0};
static ButtonState touchNext{PIN_TOUCH_NEXT, true, false, false, false, 0, 0, 0};
static ButtonState touchPrev{PIN_TOUCH_PREV, true, false, false, false, 0, 0, 0};
static ButtonState btnVolDown{PIN_BTN_VOL_DOWN, false, true, true, false, 0, 0, 0};
static ButtonState btnVolUp{PIN_BTN_VOL_UP, false, true, true, false, 0, 0, 0};
static unsigned long comboStartMs = 0;
static bool comboFired = false;

static void primeButton(ButtonState &btn) {
  bool reading = digitalRead(btn.pin);
  btn.lastReading = reading;
  btn.lastStable = btn.activeHigh ? reading : !reading;
  btn.stablePressed = false;
  btn.lastChange = millis();
  btn.lastRepeat = btn.lastChange;
  btn.pressedAt = 0;
}

static bool readPressed(const ButtonState &btn) {
  bool raw = digitalRead(btn.pin);
  return btn.activeHigh ? raw : !raw;
}

static bool updateButton(ButtonState &btn) {
  bool pressed = readPressed(btn);
  unsigned long now = millis();

  if (pressed != btn.lastReading) {
    btn.lastChange = now;
    btn.lastReading = pressed;
  }

  if ((now - btn.lastChange) > DEBOUNCE_MS) {
    if (pressed != btn.lastStable) {
      btn.lastStable = pressed;
      if (pressed) {
        btn.stablePressed = true;
        btn.pressedAt = now;
        btn.lastRepeat = now;
        return true;
      }
      btn.stablePressed = false;
      btn.pressedAt = 0;
    }
  }
  return false;
}

static bool shouldRepeat(ButtonState &btn, unsigned long now) {
  if (!btn.stablePressed) return false;
  if (btn.pressedAt == 0) return false;
  if (now - btn.pressedAt < HOLD_MS) return false;
  if (now - btn.lastRepeat >= REPEAT_MS) {
    btn.lastRepeat = now;
    return true;
  }
  return false;
}

void inputInit() {
  pinMode(PIN_TOUCH_PLAY, INPUT);
  pinMode(PIN_TOUCH_NEXT, INPUT);
  pinMode(PIN_TOUCH_PREV, INPUT);

  pinMode(PIN_BTN_VOL_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_VOL_UP, INPUT_PULLUP);

  primeButton(touchPlay);
  primeButton(touchNext);
  primeButton(touchPrev);
  primeButton(btnVolDown);
  primeButton(btnVolUp);
}

InputEvent inputPoll() {
  unsigned long now = millis();

  // Update touch buttons (edge detection only)
  if (updateButton(touchPrev)) return InputEvent::Prev;
  if (updateButton(touchPlay)) return InputEvent::PlayPause;
  if (updateButton(touchNext)) return InputEvent::Next;

  // Update mechanical volume buttons with repeat
  bool volDownPressed = updateButton(btnVolDown); // left = volume down
  bool volUpPressed = updateButton(btnVolUp);     // right = volume up

  bool bothPressed = btnVolUp.stablePressed && btnVolDown.stablePressed;
  if (bothPressed) {
    if (comboStartMs == 0) {
      comboStartMs = now;
      comboFired = false;
    } else if (!comboFired && (now - comboStartMs >= MODE_TOGGLE_HOLD_MS)) {
      comboFired = true;
      return InputEvent::ModeToggle;
    }
  } else {
    comboStartMs = 0;
    comboFired = false;
  }

  if (!bothPressed) {
    if (volUpPressed) return InputEvent::VolUp;
    if (volDownPressed) return InputEvent::VolDown;

    if (shouldRepeat(btnVolUp, now)) return InputEvent::VolUp;
    if (shouldRepeat(btnVolDown, now)) return InputEvent::VolDown;
  }

  return InputEvent::None;
}

