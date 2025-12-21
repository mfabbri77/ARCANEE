#pragma once

#include "AudioDevice.h"
#include "AudioQueue.h"
#include "ModulePlayer.h"
#include "SfxMixer.h"
#include "common/Types.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace arcanee::audio {

/**
 * @brief Central audio manager integrating all audio subsystems.
 *
 * Manages:
 * - AudioDevice (SDL output)
 * - ModulePlayer (tracker music)
 * - SfxMixer (sound effects)
 * - AudioCommandQueue (thread-safe commands)
 *
 * @ref specs/Chapter 8B §8B.5
 */
class AudioManager {
public:
  AudioManager();
  ~AudioManager();

  // Non-copyable
  AudioManager(const AudioManager &) = delete;
  AudioManager &operator=(const AudioManager &) = delete;

  /**
   * @brief Initialize audio subsystem.
   */
  bool initialize();

  /**
   * @brief Shutdown audio subsystem.
   */
  void shutdown();

  // ===== Sound Management =====
  /**
   * @brief Load a sound from memory.
   * @return Sound handle or 0 on failure
   */
  u32 loadSound(const u8 *data, size_t size, u32 sampleRate, u32 channels);

  /**
   * @brief Free a loaded sound.
   */
  void freeSound(u32 handle);

  /**
   * @brief Play a sound.
   * @return Voice index or -1 on failure
   */
  i32 playSound(u32 handle, f32 volume = 1.0f, f32 pan = 0.0f,
                bool loop = false);

  /**
   * @brief Stop a voice.
   */
  void stopVoice(u32 voiceIndex);

  /**
   * @brief Stop all sounds.
   */
  void stopAllSounds();

  // ===== Module Management =====
  /**
   * @brief Load a module from memory.
   * @return Module handle or 0 on failure
   */
  u32 loadModule(const u8 *data, size_t size);

  /**
   * @brief Free a loaded module.
   */
  void freeModule(u32 handle);

  /**
   * @brief Play a module.
   */
  void playModule(u32 handle, bool loop = true);

  /**
   * @brief Stop the current module.
   */
  void stopModule();

  /**
   * @brief Pause/resume module.
   */
  void pauseModule();
  void resumeModule();

  /**
   * @brief Set module volume.
   */
  void setModuleVolume(f32 volume);

  // ===== Master Control =====
  void setMasterVolume(f32 volume);
  f32 getMasterVolume() const;

  bool isModulePlaying() const;
  u32 getActiveVoiceCount() const;

  // ===== Audio Callback (called by AudioDevice) =====
  void mixAudio(f32 *buffer, u32 frames, u32 sampleRate);

private:
  void processCommands();

  std::unique_ptr<AudioDevice> m_device;
  std::unique_ptr<ModulePlayer> m_modulePlayer;
  std::unique_ptr<SfxMixer> m_sfxMixer;
  AudioCommandQueue m_commandQueue;

  // Sound storage (handle -> SoundData)
  std::unordered_map<u32, std::unique_ptr<SoundData>> m_sounds;
  u32 m_nextSoundHandle = 1;

  // Module storage (only one active at a time for now)
  u32 m_currentModuleHandle = 0;
  std::vector<u8> m_currentModuleData;

  std::atomic<f32> m_masterVolume{1.0f};
};

// Global audio manager instance (set from Runtime)
AudioManager *getAudioManager();
void setAudioManager(AudioManager *mgr);

} // namespace arcanee::audio
