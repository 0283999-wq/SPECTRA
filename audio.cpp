#include "audio.h"

#include <DFRobotDFPlayerMini.h>

static HardwareSerial dfSerial(1);
static DFRobotDFPlayerMini dfPlayer;

static uint16_t currentTrack = DEFAULT_TRACK;
static uint8_t currentVolume = DEFAULT_VOLUME;
static PlaybackState playbackState = PlaybackState::Stopped;
static bool initialized = false;
static bool online = false;
static uint16_t trackCount = 1;
static unsigned long lastQuery = 0;

static uint16_t detectTrackCount() {
  int16_t count = dfPlayer.readFileCounts();
  if (count > 0) return static_cast<uint16_t>(count);

  count = dfPlayer.readFileCountsInFolder(1);
  if (count > 0) return static_cast<uint16_t>(count);

  // Stable fallback: assume single track available
  return 1;
}

void audioInit() {
  dfSerial.begin(9600, SERIAL_8N1, PIN_DFPLAYER_TX, PIN_DFPLAYER_RX);
  if (dfPlayer.begin(dfSerial)) {
    initialized = true;
    online = true;
    trackCount = detectTrackCount();
    if (trackCount == 0) trackCount = 1;
    dfPlayer.volume(currentVolume);
    currentTrack = constrain(currentTrack, (uint16_t)1, trackCount);
    audioPlayTrack(currentTrack);
  } else {
    initialized = false;
    online = false;
    trackCount = 1;
  }
}

void audioLoop() {
  if (!initialized) return;
  unsigned long now = millis();
  if (now - lastQuery > 1000) {
    lastQuery = now;
    // Reserved for periodic polling
  }
}

void audioPlayTrack(uint16_t trackNumber) {
  currentTrack = constrain(trackNumber, (uint16_t)1, trackCount);
  if (initialized) {
    if (currentTrack <= trackCount) {
      dfPlayer.playMp3Folder(currentTrack);
      playbackState = PlaybackState::Playing;
    }
  }
}

void audioNext() {
  if (!online || !initialized) return;
  if (trackCount <= 1) return;
  if (currentTrack < trackCount) {
    currentTrack++;
    audioPlayTrack(currentTrack);
  }
}

void audioPrev() {
  if (!online || !initialized) return;
  if (trackCount <= 1) return;
  if (currentTrack > 1) {
    currentTrack--;
    audioPlayTrack(currentTrack);
  }
}

void audioTogglePause() {
  if (!initialized) return;
  if (playbackState == PlaybackState::Playing) {
    dfPlayer.pause();
    playbackState = PlaybackState::Paused;
  } else {
    dfPlayer.start();
    playbackState = PlaybackState::Playing;
  }
}

bool audioVolumeUp() {
  if (currentVolume < MAX_VOLUME) {
    currentVolume++;
    if (initialized) {
      dfPlayer.volume(currentVolume);
    }
    return true;
  }
  return false;
}

bool audioVolumeDown() {
  if (currentVolume > MIN_VOLUME) {
    currentVolume--;
    if (initialized) {
      dfPlayer.volume(currentVolume);
    }
    return true;
  }
  return false;
}

AudioStatus getAudioStatus() {
  AudioStatus s{};
  s.track = currentTrack;
  s.volume = currentVolume;
  s.trackCount = trackCount;
  s.online = online;
  s.state = playbackState;
  return s;
}

