/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * @file ModulePlayer.cpp
 * @brief libopenmpt module player implementation.
 * @ref specs/Chapter 8B §8B.4
 */

#include "ModulePlayer.h"
#include "common/Log.h"

#ifdef HAS_LIBOPENMPT
#include <libopenmpt/libopenmpt.h>
#endif

namespace arcanee::audio {

ModulePlayer::ModulePlayer() = default;

ModulePlayer::~ModulePlayer() { unload(); }

bool ModulePlayer::load(const u8 *data, size_t size) {
  unload();

#ifdef HAS_LIBOPENMPT
  m_module =
      openmpt_module_create_from_memory2(data, size, nullptr, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, nullptr);

  if (!m_module) {
    LOG_ERROR("ModulePlayer: Failed to load module");
    return false;
  }

  LOG_INFO("ModulePlayer: Loaded module (%d orders, %d patterns)",
           getNumOrders(), getNumPatterns());
  return true;
#else
  (void)data;
  (void)size;
  LOG_WARN("ModulePlayer: libopenmpt not available, module loading disabled");
  return false;
#endif
}

void ModulePlayer::unload() {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    openmpt_module_destroy(m_module);
    m_module = nullptr;
    m_playing.store(false);
    m_paused.store(false);
  }
#endif
}

void ModulePlayer::play() {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    openmpt_module_set_position_seconds(m_module, 0.0);
    m_playing.store(true);
    m_paused.store(false);
  }
#endif
}

void ModulePlayer::stop() {
  m_playing.store(false);
  m_paused.store(false);
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    openmpt_module_set_position_seconds(m_module, 0.0);
  }
#endif
}

void ModulePlayer::pause() { m_paused.store(true); }

void ModulePlayer::resume() { m_paused.store(false); }

void ModulePlayer::setVolume(f32 volume) {
  m_volume.store(volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume));
}

void ModulePlayer::setPosition(f64 seconds) {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    openmpt_module_set_position_seconds(m_module, seconds);
  }
#else
  (void)seconds;
#endif
}

f64 ModulePlayer::getPosition() const {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    return openmpt_module_get_position_seconds(m_module);
  }
#endif
  return 0.0;
}

f64 ModulePlayer::getDuration() const {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    return openmpt_module_get_duration_seconds(m_module);
  }
#endif
  return 0.0;
}

i32 ModulePlayer::getNumOrders() const {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    return openmpt_module_get_num_orders(m_module);
  }
#endif
  return 0;
}

i32 ModulePlayer::getNumPatterns() const {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    return openmpt_module_get_num_patterns(m_module);
  }
#endif
  return 0;
}

i32 ModulePlayer::getCurrentOrder() const {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    return openmpt_module_get_current_order(m_module);
  }
#endif
  return 0;
}

i32 ModulePlayer::getCurrentPattern() const {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    return openmpt_module_get_current_pattern(m_module);
  }
#endif
  return 0;
}

i32 ModulePlayer::getCurrentRow() const {
#ifdef HAS_LIBOPENMPT
  if (m_module) {
    return openmpt_module_get_current_row(m_module);
  }
#endif
  return 0;
}

u32 ModulePlayer::render(f32 *buffer, u32 frames, u32 sampleRate) {
#ifdef HAS_LIBOPENMPT
  if (!m_module || !m_playing.load() || m_paused.load()) {
    return 0;
  }

  size_t rendered = openmpt_module_read_interleaved_float_stereo(
      m_module, static_cast<int32_t>(sampleRate), static_cast<size_t>(frames),
      buffer);

  // Apply volume
  f32 vol = m_volume.load();
  if (vol < 1.0f) {
    for (size_t i = 0; i < rendered * 2; ++i) {
      buffer[i] *= vol;
    }
  }

  // Check if module ended
  if (rendered == 0) {
    m_playing.store(false);
  }

  return static_cast<u32>(rendered);
#else
  (void)buffer;
  (void)frames;
  (void)sampleRate;
  return 0;
#endif
}

} // namespace arcanee::audio
