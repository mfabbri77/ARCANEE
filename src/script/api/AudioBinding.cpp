/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * @file AudioBinding.cpp
 * @brief Squirrel audio.* API bindings.
 * @ref specs/Chapter 8 §8.4
 */

#include "AudioBinding.h"
#include "audio/AudioManager.h"
#include "common/Types.h"
#include "vfs/Vfs.h"
#include <sqstdaux.h>
#include <sqstdio.h>
#include <vector>

namespace arcanee::script {

using audio::AudioManager;
using audio::getAudioManager;

// Global VFS pointer for audio loading
// Global VFS pointer for audio loading
static arcanee::vfs::IVfs *g_audioVfs = nullptr;

void setAudioVfs(arcanee::vfs::IVfs *vfs) { g_audioVfs = vfs; }

// ===== Module Functions =====
static SQInteger audio_loadModule(HSQUIRRELVM vm) {
  const SQChar *path = nullptr;
  if (SQ_FAILED(sq_getstring(vm, 2, &path))) {
    return sq_throwerror(vm, "Invalid path argument");
  }

  if (!g_audioVfs || !getAudioManager()) {
    return sq_throwerror(vm, "Audio system not initialized");
  }

  // Load file via VFS
  auto dataOpt = g_audioVfs->readBytes(path);
  if (!dataOpt) {
    sq_pushinteger(vm, 0); // Return 0 on failure
    return 1;
  }
  const auto &buffer = *dataOpt;

  // Load into AudioManager
  u32 modHandle = getAudioManager()->loadModule(buffer.data(), buffer.size());
  sq_pushinteger(vm, static_cast<SQInteger>(modHandle));
  return 1;
}

static SQInteger audio_freeModule(HSQUIRRELVM vm) {
  SQInteger handle;
  sq_getinteger(vm, 2, &handle);
  if (auto *mgr = getAudioManager()) {
    mgr->freeModule(static_cast<u32>(handle));
  }
  return 0;
}

static SQInteger audio_playModule(HSQUIRRELVM vm) {
  SQInteger handle;
  SQBool loop = SQTrue;
  sq_getinteger(vm, 2, &handle);
  if (sq_gettop(vm) >= 3) {
    sq_getbool(vm, 3, &loop);
  }
  if (auto *mgr = getAudioManager()) {
    mgr->playModule(static_cast<u32>(handle), loop == SQTrue);
  }
  return 0;
}

static SQInteger audio_stopModule(HSQUIRRELVM /*vm*/) {
  if (auto *mgr = getAudioManager()) {
    mgr->stopModule();
  }
  return 0;
}

static SQInteger audio_pauseModule(HSQUIRRELVM /*vm*/) {
  if (auto *mgr = getAudioManager()) {
    mgr->pauseModule();
  }
  return 0;
}

static SQInteger audio_resumeModule(HSQUIRRELVM /*vm*/) {
  if (auto *mgr = getAudioManager()) {
    mgr->resumeModule();
  }
  return 0;
}

static SQInteger audio_setModuleVolume(HSQUIRRELVM vm) {
  SQFloat volume;
  sq_getfloat(vm, 2, &volume);
  if (auto *mgr = getAudioManager()) {
    mgr->setModuleVolume(volume);
  }
  return 0;
}

static SQInteger audio_isModulePlaying(HSQUIRRELVM vm) {
  if (auto *mgr = getAudioManager()) {
    sq_pushbool(vm, mgr->isModulePlaying() ? SQTrue : SQFalse);
  } else {
    sq_pushbool(vm, SQFalse);
  }
  return 1;
}

// ===== Sound Functions =====
static SQInteger audio_loadSound(HSQUIRRELVM vm) {
  const SQChar *path = nullptr;
  if (SQ_FAILED(sq_getstring(vm, 2, &path))) {
    return sq_throwerror(vm, "Invalid path argument");
  }

  if (!g_audioVfs || !getAudioManager()) {
    return sq_throwerror(vm, "Audio system not initialized");
  }

  auto dataOpt = g_audioVfs->readBytes(path);
  if (!dataOpt) {
    sq_pushinteger(vm, 0); // Return 0 on failure
    return 1;
  }
  const auto &buffer = *dataOpt;

  // Assuming 44100/2 for raw loading default as discussed
  u32 sndHandle =
      getAudioManager()->loadSound(buffer.data(), buffer.size(), 44100, 2);
  sq_pushinteger(vm, static_cast<SQInteger>(sndHandle));
  return 1;
}

static SQInteger audio_freeSound(HSQUIRRELVM vm) {
  SQInteger handle;
  sq_getinteger(vm, 2, &handle);
  if (auto *mgr = getAudioManager()) {
    mgr->freeSound(static_cast<u32>(handle));
  }
  return 0;
}

static SQInteger audio_playSound(HSQUIRRELVM vm) {
  SQInteger handle;
  SQFloat volume = 1.0f, pan = 0.0f;
  SQBool loop = SQFalse;

  sq_getinteger(vm, 2, &handle);
  if (sq_gettop(vm) >= 3)
    sq_getfloat(vm, 3, &volume);
  if (sq_gettop(vm) >= 4)
    sq_getfloat(vm, 4, &pan);
  if (sq_gettop(vm) >= 5)
    sq_getbool(vm, 5, &loop);

  if (auto *mgr = getAudioManager()) {
    i32 voice =
        mgr->playSound(static_cast<u32>(handle), volume, pan, loop == SQTrue);
    sq_pushinteger(vm, voice);
  } else {
    sq_pushinteger(vm, -1);
  }
  return 1;
}

static SQInteger audio_stopVoice(HSQUIRRELVM vm) {
  SQInteger voice;
  sq_getinteger(vm, 2, &voice);
  if (auto *mgr = getAudioManager()) {
    mgr->stopVoice(static_cast<u32>(voice));
  }
  return 0;
}

static SQInteger audio_stopAllSounds(HSQUIRRELVM /*vm*/) {
  if (auto *mgr = getAudioManager()) {
    mgr->stopAllSounds();
  }
  return 0;
}

// ===== Master Control =====
static SQInteger audio_setMasterVolume(HSQUIRRELVM vm) {
  SQFloat volume;
  sq_getfloat(vm, 2, &volume);
  if (auto *mgr = getAudioManager()) {
    mgr->setMasterVolume(volume);
  }
  return 0;
}

static SQInteger audio_getMasterVolume(HSQUIRRELVM vm) {
  if (auto *mgr = getAudioManager()) {
    sq_pushfloat(vm, mgr->getMasterVolume());
  } else {
    sq_pushfloat(vm, 1.0f);
  }
  return 1;
}

static SQInteger audio_getActiveVoiceCount(HSQUIRRELVM vm) {
  if (auto *mgr = getAudioManager()) {
    sq_pushinteger(vm, static_cast<SQInteger>(mgr->getActiveVoiceCount()));
  } else {
    sq_pushinteger(vm, 0);
  }
  return 1;
}

// ===== Registration =====
void registerAudioBinding(HSQUIRRELVM vm) {
  sq_pushroottable(vm);
  sq_pushstring(vm, "audio", -1);
  sq_newtable(vm);

  // Module functions
  sq_pushstring(vm, "loadModule", -1);
  sq_newclosure(vm, audio_loadModule, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "freeModule", -1);
  sq_newclosure(vm, audio_freeModule, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "playModule", -1);
  sq_newclosure(vm, audio_playModule, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "stopModule", -1);
  sq_newclosure(vm, audio_stopModule, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "pauseModule", -1);
  sq_newclosure(vm, audio_pauseModule, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "resumeModule", -1);
  sq_newclosure(vm, audio_resumeModule, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "setModuleVolume", -1);
  sq_newclosure(vm, audio_setModuleVolume, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "isModulePlaying", -1);
  sq_newclosure(vm, audio_isModulePlaying, 0);
  sq_newslot(vm, -3, SQFalse);

  // Sound functions
  sq_pushstring(vm, "loadSound", -1);
  sq_newclosure(vm, audio_loadSound, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "freeSound", -1);
  sq_newclosure(vm, audio_freeSound, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "playSound", -1);
  sq_newclosure(vm, audio_playSound, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "stopVoice", -1);
  sq_newclosure(vm, audio_stopVoice, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "stopAllSounds", -1);
  sq_newclosure(vm, audio_stopAllSounds, 0);
  sq_newslot(vm, -3, SQFalse);

  // Master control
  sq_pushstring(vm, "setMasterVolume", -1);
  sq_newclosure(vm, audio_setMasterVolume, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "getMasterVolume", -1);
  sq_newclosure(vm, audio_getMasterVolume, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "getActiveVoiceCount", -1);
  sq_newclosure(vm, audio_getActiveVoiceCount, 0);
  sq_newslot(vm, -3, SQFalse);

  // Add audio table to root
  sq_newslot(vm, -3, SQFalse);
  sq_pop(vm, 1);
}

} // namespace arcanee::script
