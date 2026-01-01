#pragma once

#include <Arduino.h>
#include "config.h"

enum class PlaybackState {
  Stopped,
  Playing,
  Paused
};

struct AudioStatus {
  uint16_t track;
  uint8_t volume;
  uint16_t trackCount;
  bool online;
  PlaybackState state;
};

void audioInit();
void audioLoop();
void audioPlayTrack(uint16_t trackNumber);
void audioNext();
void audioPrev();
void audioTogglePause();
bool audioVolumeUp();
bool audioVolumeDown();
AudioStatus getAudioStatus();

