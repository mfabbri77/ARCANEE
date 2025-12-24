/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * @file AudioManager.cpp
 * @brief Central audio manager implementation.
 * @ref specs/Chapter 8B §8B.5
 */

#include "AudioManager.h"
#include "common/Log.h"
#include <cstring>

namespace arcanee::audio {

static AudioManager *g_audioManager = nullptr;

AudioManager *getAudioManager() { return g_audioManager; }
void setAudioManager(AudioManager *mgr) { g_audioManager = mgr; }

AudioManager::AudioManager() {
  m_device = std::make_unique<AudioDevice>();
  m_modulePlayer = std::make_unique<ModulePlayer>();
  m_sfxMixer = std::make_unique<SfxMixer>();
}

AudioManager::~AudioManager() { shutdown(); }

bool AudioManager::initialize() {
  LOG_INFO("AudioManager: Initializing...");

  if (!m_device->initialize()) {
    LOG_ERROR("AudioManager: Failed to initialize audio device");
    return false;
  }

  // Ensure default volumes
  m_device->setMasterVolume(1.0f);
  m_modulePlayer->setVolume(1.0f);

  LOG_INFO("AudioManager: Initialized successfully");
  return true;
}

void AudioManager::shutdown() {
  if (m_device) {
    m_device->shutdown();
  }
  m_sounds.clear();
  m_currentModuleData.clear();
  LOG_INFO("AudioManager: Shutdown");
}

// ===== Sound Management =====
u32 AudioManager::loadSound(const u8 *data, size_t size, u32 sampleRate,
                            u32 channels) {
  if (!data || size == 0 || channels == 0) {
    return 0;
  }

  auto sound = std::make_unique<SoundData>();
  sound->sampleRate = sampleRate;
  sound->channels = channels;

  // Convert bytes to float samples (assuming 16-bit PCM input)
  size_t sampleCount = size / 2; // 16-bit = 2 bytes per sample
  sound->samples.resize(sampleCount);

  const i16 *pcm = reinterpret_cast<const i16 *>(data);
  for (size_t i = 0; i < sampleCount; ++i) {
    sound->samples[i] = static_cast<f32>(pcm[i]) / 32768.0f;
  }

  u32 handle = m_nextSoundHandle++;
  m_sounds[handle] = std::move(sound);

  LOG_INFO("AudioManager: Loaded sound %u (%zu samples, %u Hz, %u ch)", handle,
           sampleCount, sampleRate, channels);
  return handle;
}

void AudioManager::freeSound(u32 handle) {
  auto it = m_sounds.find(handle);
  if (it != m_sounds.end()) {
    m_sounds.erase(it);
    LOG_INFO("AudioManager: Freed sound %u", handle);
  }
}

i32 AudioManager::playSound(u32 handle, f32 volume, f32 pan, bool loop) {
  auto it = m_sounds.find(handle);
  if (it == m_sounds.end()) {
    return -1;
  }
  return m_sfxMixer->play(it->second.get(), volume, pan, loop);
}

void AudioManager::stopVoice(u32 voiceIndex) {
  m_sfxMixer->stopVoice(voiceIndex);
}

void AudioManager::stopAllSounds() { m_sfxMixer->stopAll(); }

// ===== Module Management =====
// ===== Module Management =====
u32 AudioManager::loadModule(const u8 *data, size_t size) {
  if (!data || size == 0) {
    return 0;
  }

  // Store module data (ModulePlayer needs it to stay alive)
  // Warning: Modifying this on Main Thread while Audio Thread might be reading?
  // Ideally, we should double-buffer or use a mutex.
  // For V0.1, we assume load handles overlap safely or user stops before load.
  m_currentModuleData.assign(data, data + size);

  if (!m_modulePlayer->load(m_currentModuleData.data(),
                            m_currentModuleData.size())) {
    m_currentModuleData.clear();
    return 0;
  }

  m_currentModuleHandle = 1; // Only one module at a time
  LOG_INFO("AudioManager: Module loaded");
  return m_currentModuleHandle;
}

void AudioManager::freeModule(u32 handle) {
  if (handle == m_currentModuleHandle) {
    // ENQUEUE THIS TOO? For now, direct call might unsafe.
    // Unloading while playing is dangerous.
    // Safe option: Stop first.
    m_modulePlayer->unload();
    m_currentModuleData.clear();
    m_currentModuleHandle = 0;
  }
}

void AudioManager::playModule(u32 handle, bool loop) {
  AudioCommandData cmd;
  cmd.cmd = AudioCommand::PlayModule;
  cmd.playModule.handle = handle;
  cmd.playModule.loop = loop;
  m_commandQueue.push(cmd);
}

void AudioManager::stopModule() {
  AudioCommandData cmd;
  cmd.cmd = AudioCommand::StopModule;
  m_commandQueue.push(cmd);
}

void AudioManager::pauseModule() {
  AudioCommandData cmd;
  cmd.cmd = AudioCommand::PauseModule;
  m_commandQueue.push(cmd);
}

void AudioManager::resumeModule() {
  AudioCommandData cmd;
  cmd.cmd = AudioCommand::ResumeModule;
  m_commandQueue.push(cmd);
}

void AudioManager::setModuleVolume(f32 volume) {
  AudioCommandData cmd;
  cmd.cmd = AudioCommand::SetModuleVolume;
  cmd.setVolume.volume = volume;
  m_commandQueue.push(cmd);
}

// ===== Master Control =====
void AudioManager::setMasterVolume(f32 volume) {
  // Update local atomic for immediate UI feedback if needed
  m_masterVolume.store(volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume));

  AudioCommandData cmd;
  cmd.cmd = AudioCommand::SetMasterVolume;
  cmd.masterVolume.volume = m_masterVolume.load();
  m_commandQueue.push(cmd);
}

f32 AudioManager::getMasterVolume() const { return m_masterVolume.load(); }

bool AudioManager::isModulePlaying() const {
  return m_modulePlayer && m_modulePlayer->isPlaying();
}

u32 AudioManager::getActiveVoiceCount() const {
  return m_sfxMixer ? m_sfxMixer->getActiveVoiceCount() : 0;
}

// ===== Internal Command Handlers (Audio Thread) =====

void AudioManager::doPlayModule(u32 handle, bool loop) {
  (void)loop;
  if (handle == m_currentModuleHandle) {
    m_modulePlayer->play();
  }
}

void AudioManager::doStopModule() { m_modulePlayer->stop(); }
void AudioManager::doPauseModule() { m_modulePlayer->pause(); }
void AudioManager::doResumeModule() { m_modulePlayer->resume(); }
void AudioManager::doSetModuleVolume(f32 volume) {
  m_modulePlayer->setVolume(volume);
}
void AudioManager::doSetMasterVolume(f32 volume) {
  if (m_device)
    m_device->setMasterVolume(volume);
}
void AudioManager::doStopAllSounds() { m_sfxMixer->stopAll(); }

// ===== Audio Mixing =====
void AudioManager::processCommands() {
  AudioCommandData cmd;
  while (m_commandQueue.pop(cmd)) {
    switch (cmd.cmd) {
    case AudioCommand::PlayModule:
      doPlayModule(cmd.playModule.handle, cmd.playModule.loop);
      break;
    case AudioCommand::StopModule:
      doStopModule();
      break;
    case AudioCommand::PauseModule:
      doPauseModule();
      break;
    case AudioCommand::ResumeModule:
      doResumeModule();
      break;
    case AudioCommand::SetModuleVolume:
      doSetModuleVolume(cmd.setVolume.volume);
      break;
    case AudioCommand::StopAllSounds:
      doStopAllSounds();
      break;
    case AudioCommand::SetMasterVolume:
      doSetMasterVolume(cmd.masterVolume.volume);
      break;
    default:
      break;
    }
  }
}

void AudioManager::mixAudio(f32 *buffer, u32 frames, u32 sampleRate) {
  // Process pending commands
  processCommands();

  // Clear buffer first (SDL expects mixed data, but we overwrite/add)
  // Actually typically callback provides silence or existing data.
  // We assume clear for our logic if we effectively own the mix.
  std::memset(buffer, 0, frames * 2 * sizeof(f32));

  // 1. Render Module (Music)
  if (m_modulePlayer) {
    m_modulePlayer->render(buffer, frames, sampleRate);
  }

  // 2. Render SFX (Additive)
  if (m_sfxMixer) {
    m_sfxMixer->mix(buffer, frames, sampleRate);
  }
}

} // namespace arcanee::audio
