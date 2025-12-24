#pragma once

#include "common/Types.h"
#include <array>
#include <atomic>
#include <cstddef>

namespace arcanee::audio {

/**
 * @brief Audio command types.
 */
enum class AudioCommand : u8 {
  None = 0,
  PlayModule,
  StopModule,
  PauseModule,
  ResumeModule,
  SetModuleVolume,
  PlaySound,
  StopVoice,
  StopAllSounds,
  SetMasterVolume
};

struct PlayModuleData {
  u32 handle;
  bool loop;
};
struct SetVolumeData {
  u32 handle;
  f32 volume;
};
struct PlaySoundData {
  u32 soundHandle;
  f32 volume;
  f32 pan;
  bool loop;
};
struct StopVoiceData {
  u32 voiceIndex;
};
struct MasterVolumeData {
  f32 volume;
};

/**
 * @brief Audio command data (union for space efficiency).
 */
struct AudioCommandData {
  AudioCommand cmd = AudioCommand::None;
  union {
    PlayModuleData playModule;
    SetVolumeData setVolume;
    PlaySoundData playSound;
    StopVoiceData stopVoice;
    MasterVolumeData masterVolume;
  };
};

/**
 * @brief Lock-free Single-Producer Single-Consumer queue.
 *
 * Used for thread-safe communication from main thread to audio thread.
 *
 * @ref specs/Chapter 8B §8B.4.2
 */
template <typename T, size_t Capacity> class SPSCQueue {
public:
  /**
   * @brief Push an item (producer side).
   * @return true if pushed, false if queue full
   */
  bool push(const T &item) {
    size_t write = m_writePos.load(std::memory_order_relaxed);
    size_t nextWrite = (write + 1) % Capacity;

    if (nextWrite == m_readPos.load(std::memory_order_acquire)) {
      return false; // Queue full
    }

    m_buffer[write] = item;
    m_writePos.store(nextWrite, std::memory_order_release);
    return true;
  }

  /**
   * @brief Pop an item (consumer side).
   * @return true if popped, false if queue empty
   */
  bool pop(T &item) {
    std::size_t read = m_readPos.load(std::memory_order_relaxed);

    if (read == m_writePos.load(std::memory_order_acquire)) {
      return false; // Queue empty
    }

    item = m_buffer[read];
    m_readPos.store((read + 1) % Capacity, std::memory_order_release);
    return true;
  }

  /**
   * @brief Check if queue is empty.
   */
  bool empty() const {
    return m_readPos.load(std::memory_order_acquire) ==
           m_writePos.load(std::memory_order_acquire);
  }

private:
  std::array<T, Capacity> m_buffer;
  std::atomic<std::size_t> m_readPos{0};
  std::atomic<std::size_t> m_writePos{0};
};

// Command queue with 256 slots
using AudioCommandQueue = SPSCQueue<AudioCommandData, 256>;

} // namespace arcanee::audio
