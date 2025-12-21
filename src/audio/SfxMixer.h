#pragma once

#include "common/Types.h"
#include <atomic>
#include <memory>
#include <vector>

namespace arcanee::audio {

/**
 * @brief PCM sample data for SFX playback.
 */
struct SoundData {
  std::vector<f32> samples; // Interleaved stereo
  u32 sampleRate = 48000;
  u32 channels = 2;
};

/**
 * @brief A single voice in the mixer.
 */
struct Voice {
  SoundData *sound = nullptr;
  size_t position = 0;
  f32 volume = 1.0f;
  f32 pan = 0.0f; // -1 = left, 0 = center, +1 = right
  bool playing = false;
  bool looping = false;
};

/**
 * @brief Multi-voice SFX mixer.
 *
 * Manages a pool of voices for sound effect playback.
 * Supports volume, pan, and voice stealing.
 *
 * @ref specs/Chapter 8B §8B.5
 */
class SfxMixer {
public:
  static constexpr u32 MAX_VOICES = 16;

  SfxMixer();
  ~SfxMixer();

  /**
   * @brief Play a sound on an available voice.
   * @param sound Pointer to sound data (must remain valid during playback)
   * @param volume Volume (0.0 - 1.0)
   * @param pan Pan position (-1 = left, 0 = center, 1 = right)
   * @param loop Whether to loop the sound
   * @return Voice index or -1 if no voice available
   */
  i32 play(SoundData *sound, f32 volume = 1.0f, f32 pan = 0.0f,
           bool loop = false);

  /**
   * @brief Stop a specific voice.
   */
  void stopVoice(u32 voiceIndex);

  /**
   * @brief Stop all voices.
   */
  void stopAll();

  /**
   * @brief Check if a voice is playing.
   */
  bool isVoicePlaying(u32 voiceIndex) const;

  /**
   * @brief Set volume for a playing voice.
   */
  void setVoiceVolume(u32 voiceIndex, f32 volume);

  /**
   * @brief Set pan for a playing voice.
   */
  void setVoicePan(u32 voiceIndex, f32 pan);

  /**
   * @brief Mix all active voices into buffer.
   * @param buffer Output buffer (interleaved stereo float)
   * @param frames Number of frames to mix
   * @param sampleRate Output sample rate
   */
  void mix(f32 *buffer, u32 frames, u32 sampleRate);

  /**
   * @brief Get number of active voices.
   */
  u32 getActiveVoiceCount() const;

private:
  i32 findFreeVoice();
  i32 stealVoice(); // Voice stealing when all voices in use

  Voice m_voices[MAX_VOICES];
  std::atomic<u32> m_activeCount{0};
};

} // namespace arcanee::audio
