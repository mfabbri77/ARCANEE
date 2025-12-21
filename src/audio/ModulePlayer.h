#pragma once

#include "common/Types.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

// Forward declare openmpt
struct openmpt_module;

namespace arcanee::audio {

/**
 * @brief Tracker module player using libopenmpt.
 *
 * Supports MOD, XM, IT, S3M, and other tracker formats.
 *
 * @ref specs/Chapter 8B §8B.4
 */
class ModulePlayer {
public:
  ModulePlayer();
  ~ModulePlayer();

  // Non-copyable
  ModulePlayer(const ModulePlayer &) = delete;
  ModulePlayer &operator=(const ModulePlayer &) = delete;

  /**
   * @brief Load module from file data.
   * @param data Raw file data
   * @param size Data size in bytes
   * @return true on success
   */
  bool load(const u8 *data, size_t size);

  /**
   * @brief Unload current module.
   */
  void unload();

  /**
   * @brief Check if module is loaded.
   */
  bool isLoaded() const { return m_module != nullptr; }

  // Playback control
  void play();
  void stop();
  void pause();
  void resume();
  bool isPlaying() const { return m_playing.load(); }
  bool isPaused() const { return m_paused.load(); }

  // Volume (0.0 - 1.0)
  void setVolume(f32 volume);
  f32 getVolume() const { return m_volume.load(); }

  // Position control
  void setPosition(f64 seconds);
  f64 getPosition() const;
  f64 getDuration() const;

  // Module info
  i32 getNumOrders() const;
  i32 getNumPatterns() const;
  i32 getCurrentOrder() const;
  i32 getCurrentPattern() const;
  i32 getCurrentRow() const;

  /**
   * @brief Render audio into buffer.
   * @param buffer Output buffer (interleaved stereo float)
   * @param frames Number of frames to render
   * @param sampleRate Output sample rate
   * @return Number of frames rendered
   */
  u32 render(f32 *buffer, u32 frames, u32 sampleRate);

private:
  openmpt_module *m_module = nullptr;
  std::atomic<bool> m_playing{false};
  std::atomic<bool> m_paused{false};
  std::atomic<f32> m_volume{1.0f};
};

} // namespace arcanee::audio
