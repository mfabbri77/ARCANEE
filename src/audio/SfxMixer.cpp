/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * @file SfxMixer.cpp
 * @brief Multi-voice SFX mixer implementation.
 * @ref specs/Chapter 8B §8B.5
 */

#include "SfxMixer.h"
#include "common/Log.h"
#include <cmath>
#include <cstring>

namespace arcanee::audio {

SfxMixer::SfxMixer() {
  for (u32 i = 0; i < MAX_VOICES; ++i) {
    m_voices[i] = Voice{};
  }
}

SfxMixer::~SfxMixer() { stopAll(); }

i32 SfxMixer::findFreeVoice() {
  for (u32 i = 0; i < MAX_VOICES; ++i) {
    if (!m_voices[i].playing) {
      return static_cast<i32>(i);
    }
  }
  return -1;
}

i32 SfxMixer::stealVoice() {
  // Simple voice stealing: steal the oldest (lowest index) voice
  // A more advanced implementation would track start times
  return 0;
}

i32 SfxMixer::play(SoundData *sound, f32 volume, f32 pan, bool loop) {
  if (!sound || sound->samples.empty()) {
    return -1;
  }

  i32 voiceIdx = findFreeVoice();
  if (voiceIdx < 0) {
    voiceIdx = stealVoice();
    LOG_WARN("SfxMixer: Voice stealing, index %d", voiceIdx);
  }

  Voice &voice = m_voices[voiceIdx];
  voice.sound = sound;
  voice.position = 0;
  voice.volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
  voice.pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);
  voice.looping = loop;
  voice.playing = true;

  m_activeCount.fetch_add(1, std::memory_order_relaxed);
  return voiceIdx;
}

void SfxMixer::stopVoice(u32 voiceIndex) {
  if (voiceIndex < MAX_VOICES && m_voices[voiceIndex].playing) {
    m_voices[voiceIndex].playing = false;
    m_activeCount.fetch_sub(1, std::memory_order_relaxed);
  }
}

void SfxMixer::stopAll() {
  for (u32 i = 0; i < MAX_VOICES; ++i) {
    if (m_voices[i].playing) {
      m_voices[i].playing = false;
    }
  }
  m_activeCount.store(0, std::memory_order_relaxed);
}

bool SfxMixer::isVoicePlaying(u32 voiceIndex) const {
  if (voiceIndex < MAX_VOICES) {
    return m_voices[voiceIndex].playing;
  }
  return false;
}

void SfxMixer::setVoiceVolume(u32 voiceIndex, f32 volume) {
  if (voiceIndex < MAX_VOICES) {
    m_voices[voiceIndex].volume =
        volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
  }
}

void SfxMixer::setVoicePan(u32 voiceIndex, f32 pan) {
  if (voiceIndex < MAX_VOICES) {
    m_voices[voiceIndex].pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);
  }
}

u32 SfxMixer::getActiveVoiceCount() const {
  return m_activeCount.load(std::memory_order_relaxed);
}

void SfxMixer::mix(f32 *buffer, u32 frames, u32 sampleRate) {
  (void)sampleRate; // TODO: Implement sample rate conversion if needed

  for (u32 v = 0; v < MAX_VOICES; ++v) {
    Voice &voice = m_voices[v];
    if (!voice.playing || !voice.sound) {
      continue;
    }

    const SoundData *sound = voice.sound;
    const f32 *samples = sound->samples.data();
    const size_t sampleCount = sound->samples.size();
    const u32 channels = sound->channels;

    // Calculate pan gains (constant power panning)
    f32 panAngle = (voice.pan + 1.0f) * 0.5f * 1.5707963f; // 0 to π/2
    f32 gainL = voice.volume * std::cos(panAngle);
    f32 gainR = voice.volume * std::sin(panAngle);

    for (u32 f = 0; f < frames; ++f) {
      if (voice.position >= sampleCount / channels) {
        if (voice.looping) {
          voice.position = 0;
        } else {
          voice.playing = false;
          m_activeCount.fetch_sub(1, std::memory_order_relaxed);
          break;
        }
      }

      size_t idx = voice.position * channels;
      f32 sampleL = samples[idx];
      f32 sampleR = channels > 1 ? samples[idx + 1] : sampleL;

      buffer[f * 2 + 0] += sampleL * gainL;
      buffer[f * 2 + 1] += sampleR * gainR;

      voice.position++;
    }
  }
}

} // namespace arcanee::audio
