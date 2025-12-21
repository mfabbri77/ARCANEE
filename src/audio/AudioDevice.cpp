/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * @file AudioDevice.cpp
 * @brief SDL Audio device implementation.
 * @ref specs/Chapter 8B §8B.3
 */

#include "AudioDevice.h"
#include "common/Log.h"
#include <SDL.h>
#include <cstring>

namespace arcanee::audio {

AudioDevice::AudioDevice() = default;

AudioDevice::~AudioDevice() { shutdown(); }

bool AudioDevice::initialize() {
  LOG_INFO("AudioDevice: Initializing...");

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    LOG_ERROR("AudioDevice: Failed to init SDL audio: %s", SDL_GetError());
    return false;
  }

  SDL_AudioSpec desired, obtained;
  SDL_zero(desired);

  desired.freq = 48000;
  desired.format = AUDIO_F32SYS;
  desired.channels = 2;
  desired.samples = 512;
  desired.callback = audioCallback;
  desired.userdata = this;

  m_deviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained,
                                   SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

  if (m_deviceId == 0) {
    LOG_ERROR("AudioDevice: Failed to open device: %s", SDL_GetError());
    return false;
  }

  m_sampleRate = obtained.freq;
  m_channels = obtained.channels;
  m_bufferFrames = obtained.samples;

  LOG_INFO("AudioDevice: Opened (%u Hz, %u ch, %u frames buffer)", m_sampleRate,
           m_channels, m_bufferFrames);

  // Start audio
  SDL_PauseAudioDevice(m_deviceId, 0);
  return true;
}

void AudioDevice::shutdown() {
  if (m_deviceId != 0) {
    SDL_CloseAudioDevice(m_deviceId);
    m_deviceId = 0;
    LOG_INFO("AudioDevice: Shutdown");
  }
}

void AudioDevice::setPaused(bool paused) {
  m_paused.store(paused);
  if (m_deviceId != 0) {
    SDL_PauseAudioDevice(m_deviceId, paused ? 1 : 0);
  }
}

bool AudioDevice::isPaused() const { return m_paused.load(); }

void AudioDevice::setMasterVolume(f32 volume) {
  m_masterVolume.store(volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume));
}

void AudioDevice::audioCallback(void *userdata, u8 *stream, i32 len) {
  auto *device = static_cast<AudioDevice *>(userdata);
  auto *buffer = reinterpret_cast<f32 *>(stream);
  u32 frames = static_cast<u32>(len) / (sizeof(f32) * device->m_channels);

  // Clear buffer
  std::memset(stream, 0, static_cast<size_t>(len));

  // Mix audio
  device->mixAudio(buffer, frames);

  // Apply master volume
  f32 masterVol = device->m_masterVolume.load();
  if (masterVol < 1.0f) {
    u32 samples = frames * device->m_channels;
    for (u32 i = 0; i < samples; ++i) {
      buffer[i] *= masterVol;
    }
  }
}

void AudioDevice::mixAudio(f32 *buffer, u32 frames) {
  (void)buffer;
  (void)frames;
  // TODO: Mix module player and SFX here
}

} // namespace arcanee::audio
