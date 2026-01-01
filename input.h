#pragma once

#include <Arduino.h>
#include "config.h"

enum class InputEvent {
  None,
  PlayPause,
  Next,
  Prev,
  VolUp,
  VolDown,
  ModeToggle
};

void inputInit();
InputEvent inputPoll();

