#pragma once

#include "common/Types.h"
#include <atomic>

namespace arcanee::audio {

/**
 * @brief SDL Audio Device wrapper.
 *
 * Initializes SDL audio for 48kHz float32 stereo output.
 * Calls AudioMixer to fill audio buffers in callback.
 *
 * @ref specs/Chapter 8B §8B.3
 */
class AudioDevice {
public:
  AudioDevice();
  ~AudioDevice();

  // Non-copyable
  AudioDevice(const AudioDevice &) = delete;
  AudioDevice &operator=(const AudioDevice &) = delete;

  /**
   * @brief Initialize SDL audio device.
   * @return true on success
   */
  bool initialize();

  /**
   * @brief Shutdown audio device.
   */
  void shutdown();

  /**
   * @brief Pause/resume audio playback.
   */
  void setPaused(bool paused);
  bool isPaused() const;

  // Audio format info
  u32 getSampleRate() const { return m_sampleRate; }
  u32 getChannels() const { return m_channels; }
  u32 getBufferFrames() const { return m_bufferFrames; }

  /**
   * @brief Set master volume (0.0 - 1.0).
   */
  void setMasterVolume(f32 volume);
  f32 getMasterVolume() const { return m_masterVolume; }

private:
  static void audioCallback(void *userdata, u8 *stream, i32 len);
  void mixAudio(f32 *buffer, u32 frames);

  u32 m_deviceId = 0;
  u32 m_sampleRate = 48000;
  u32 m_channels = 2;
  u32 m_bufferFrames = 512;
  std::atomic<bool> m_paused{false};
  std::atomic<f32> m_masterVolume{1.0f};
};

} // namespace arcanee::audio
