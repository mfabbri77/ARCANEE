# Chapter 8B — Extended Audio System Specification (libopenmpt + Mixer Architecture)

## 8B.1 Purpose and Scope

This supplementary chapter provides **detailed implementation-level specifications** for the Audio System, extending Chapter 8 with:

* Complete libopenmpt integration for tracker module playback
* Multi-voice SFX mixer architecture
* Thread-safe command queue design
* Sample management and decoding
* Reference pseudocode for implementers

---

## 8B.2 Audio System Architecture Overview

### 8B.2.1 Three-Plane Model

The audio system operates across three execution contexts:

```
┌──────────────────────────────────────────────────────────────────┐
│                    AUDIO SYSTEM ARCHITECTURE                      │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                    MAIN THREAD                               │ │
│  │  • Squirrel API bindings (audio.*)                          │ │
│  │  • Resource loading requests                                 │ │
│  │  • Command enqueue (play, stop, volume, etc.)               │ │
│  │  • Handle management                                         │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                              │                                    │
│                              ▼ CommandQueue (lock-free SPSC)      │
│                              │                                    │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │               AUDIO CALLBACK THREAD                          │ │
│  │  • Real-time audio mixing (SDL callback)                    │ │
│  │  • Module rendering (libopenmpt)                            │ │
│  │  • SFX voice mixing                                         │ │
│  │  • NO file I/O, NO memory allocation                        │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │               WORKER THREADS (optional)                      │ │
│  │  • Pre-decode audio files                                   │ │
│  │  • Module loading/parsing (off main thread)                 │ │
│  │  • Resample preparation                                     │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### 8B.2.2 Data Flow

```
                     Main Thread                    Audio Thread
                         │                               │
  audio.loadSound() ────▶│ decode WAV/OGG ────────────▶ │
                         │ store in SoundBank           │
                         │                               │
  audio.playSound() ────▶│ ─── PlaySFX command ──────▶  │ allocate voice
                         │                               │ mix into buffer
                         │                               │
  audio.loadModule() ───▶│ create openmpt_module ─────▶ │
                         │                               │
  audio.playModule() ───▶│ ── PlayModule command ────▶  │ read samples
                         │                               │ mix into buffer
                         │                               │
                         │        ◀── status updates ── │
                         │                               │
                         │                               ▼
                         │                     SDL_AudioCallback
                         │                     output to device
```

---

## 8B.3 SDL Audio Setup (Normative)

### 8B.3.1 Audio Device Initialization

```cpp
class AudioDevice {
public:
    bool initialize() {
        SDL_AudioSpec desired, obtained;
        
        SDL_zero(desired);
        desired.freq = 48000;           // 48kHz preferred
        desired.format = AUDIO_F32SYS;  // 32-bit float, system byte order
        desired.channels = 2;           // Stereo
        desired.samples = 512;          // Buffer size (~10.6ms at 48kHz)
        desired.callback = audioCallback;
        desired.userdata = this;
        
        m_deviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained,
                                          SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        
        if (m_deviceId == 0) {
            setLastError("audio: failed to open device: " + 
                         std::string(SDL_GetError()));
            return false;
        }
        
        m_sampleRate = obtained.freq;
        m_channels = obtained.channels;
        m_bufferFrames = obtained.samples;
        
        // Start audio processing
        SDL_PauseAudioDevice(m_deviceId, 0);
        
        return true;
    }
    
    void shutdown() {
        if (m_deviceId != 0) {
            SDL_CloseAudioDevice(m_deviceId);
            m_deviceId = 0;
        }
    }
    
private:
    static void SDLCALL audioCallback(void* userdata, Uint8* stream, int len) {
        AudioDevice* device = static_cast<AudioDevice*>(userdata);
        device->mixAudio(reinterpret_cast<float*>(stream), 
                          len / sizeof(float) / device->m_channels);
    }
    
    SDL_AudioDeviceID m_deviceId = 0;
    int m_sampleRate = 48000;
    int m_channels = 2;
    int m_bufferFrames = 512;
};
```

### 8B.3.2 Output Format Requirements (Normative)

| Parameter | Required Value | Notes |
|-----------|----------------|-------|
| Sample Rate | 44100 or 48000 Hz | 48000 preferred |
| Channels | 2 (stereo) | Mono downmix not supported |
| Format | Float32 | Internally; SDL may request int16 fallback |
| Buffer Size | 256-1024 samples | Target ~10ms latency |

---

## 8B.4 Lock-Free Command Queue (Normative)

### 8B.4.1 Command Types

```cpp
enum class AudioCommand : uint8_t {
    // Module commands
    PlayModule,
    StopModule,
    PauseModule,
    ResumeModule,
    SetModuleVolume,
    SetModuleTempo,
    SeekModule,
    
    // SFX commands
    PlaySound,
    StopVoice,
    SetVoiceVolume,
    SetVoicePan,
    SetVoicePitch,
    
    // Global commands
    SetMasterVolume,
    StopAll,
};

struct AudioCommandData {
    AudioCommand cmd;
    
    union {
        struct { ModuleHandle handle; bool loop; } playModule;
        struct { float volume; } moduleVolume;
        struct { float factor; } moduleTempo;
        struct { float seconds; } moduleSeek;
        
        struct { SoundHandle sound; VoiceId voice; float vol, pan, pitch; } playSound;
        struct { VoiceId voice; } stopVoice;
        struct { VoiceId voice; float value; } voiceParam;
        
        struct { float volume; } masterVolume;
    };
};
```

### 8B.4.2 SPSC Ring Buffer Implementation

```cpp
// Single-Producer Single-Consumer Lock-Free Queue
// Producer: Main Thread
// Consumer: Audio Thread

template<typename T, size_t Capacity>
class SPSCQueue {
public:
    bool push(const T& item) {
        size_t write = m_writePos.load(std::memory_order_relaxed);
        size_t nextWrite = (write + 1) % Capacity;
        
        if (nextWrite == m_readPos.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        m_buffer[write] = item;
        m_writePos.store(nextWrite, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) {
        size_t read = m_readPos.load(std::memory_order_relaxed);
        
        if (read == m_writePos.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }
        
        item = m_buffer[read];
        m_readPos.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }
    
private:
    std::array<T, Capacity> m_buffer;
    std::atomic<size_t> m_readPos{0};
    std::atomic<size_t> m_writePos{0};
};

using AudioCommandQueue = SPSCQueue<AudioCommandData, 256>;
```

### 8B.4.3 Command Processing

```cpp
void AudioMixer::processCommands() {
    AudioCommandData cmd;
    
    // Process all pending commands (called at start of audio callback)
    while (m_commandQueue.pop(cmd)) {
        switch (cmd.cmd) {
        case AudioCommand::PlayModule:
            startModulePlayback(cmd.playModule.handle, cmd.playModule.loop);
            break;
            
        case AudioCommand::StopModule:
            stopModulePlayback();
            break;
            
        case AudioCommand::PlaySound:
            allocateVoice(cmd.playSound.sound, cmd.playSound.voice,
                          cmd.playSound.vol, cmd.playSound.pan, 
                          cmd.playSound.pitch);
            break;
            
        case AudioCommand::StopVoice:
            releaseVoice(cmd.stopVoice.voice);
            break;
            
        case AudioCommand::SetMasterVolume:
            m_masterVolume = cmd.masterVolume.volume;
            break;
            
        case AudioCommand::StopAll:
            stopAllPlayback();
            break;
            
        // ... handle other commands
        }
    }
}
```

---

## 8B.5 libopenmpt Integration (Normative)

### 8B.5.1 Module Player Architecture

```cpp
class ModulePlayer {
public:
    ModuleHandle loadModule(const std::vector<uint8_t>& data) {
        // Create openmpt module from memory
        openmpt_module* mod = openmpt_module_create_from_memory2(
            data.data(), data.size(),
            &openmpt_log_func_silent,
            nullptr,  // loguser
            nullptr,  // errfunc
            nullptr,  // erruser
            nullptr,  // error
            nullptr,  // error_message
            nullptr   // ctls
        );
        
        if (!mod) {
            setLastError("audio.loadModule: failed to parse module");
            return 0;
        }
        
        ModuleHandle h = m_modules.allocate();
        ModuleData& modData = m_modules.get(h);
        modData.module = mod;
        modData.durationSeconds = openmpt_module_get_duration_seconds(mod);
        modData.title = openmpt_module_get_metadata(mod, "title");
        
        return h;
    }
    
    void freeModule(ModuleHandle h) {
        if (!m_modules.valid(h)) return;
        
        ModuleData& modData = m_modules.get(h);
        
        // If this module is currently playing, stop it first
        if (m_currentModule == modData.module) {
            stopPlayback();
        }
        
        openmpt_module_destroy(modData.module);
        m_modules.free(h);
    }
    
    void play(ModuleHandle h, bool loop) {
        if (!m_modules.valid(h)) return;
        
        ModuleData& modData = m_modules.get(h);
        m_currentModule = modData.module;
        m_looping = loop;
        m_playing = true;
        m_paused = false;
        
        openmpt_module_set_repeat_count(m_currentModule, loop ? -1 : 0);
        openmpt_module_set_position_seconds(m_currentModule, 0.0);
    }
    
    void stop() {
        m_playing = false;
        m_currentModule = nullptr;
    }
    
    void pause() { m_paused = true; }
    void resume() { m_paused = false; }
    
    void setVolume(float v) { m_volume = std::clamp(v, 0.0f, 1.0f); }
    void setTempo(float factor) { 
        if (m_currentModule) {
            openmpt_module_set_tempo_factor(m_currentModule, 
                                             std::clamp(factor, 0.5f, 2.0f));
        }
    }
    
    void seekSeconds(float seconds) {
        if (m_currentModule) {
            openmpt_module_set_position_seconds(m_currentModule, seconds);
        }
    }
    
    // Called from audio thread
    size_t render(float* buffer, size_t frames, int sampleRate) {
        if (!m_playing || m_paused || !m_currentModule) {
            return 0;
        }
        
        size_t rendered = openmpt_module_read_interleaved_float_stereo(
            m_currentModule, sampleRate, frames, buffer
        );
        
        // Apply volume
        if (m_volume != 1.0f) {
            for (size_t i = 0; i < rendered * 2; ++i) {
                buffer[i] *= m_volume;
            }
        }
        
        // Check if finished
        if (rendered < frames && !m_looping) {
            m_playing = false;
        }
        
        return rendered;
    }
    
private:
    struct ModuleData {
        openmpt_module* module = nullptr;
        double durationSeconds = 0;
        std::string title;
    };
    
    HandlePool<ModuleData> m_modules;
    openmpt_module* m_currentModule = nullptr;
    float m_volume = 1.0f;
    bool m_playing = false;
    bool m_paused = false;
    bool m_looping = true;
};
```

### 8B.5.2 Supported Module Formats (Normative)

libopenmpt supports the following tracker formats:

| Format | Extensions | Notes |
|--------|-----------|-------|
| ProTracker | `.mod` | 4-channel classic |
| ScreamTracker 3 | `.s3m` | 16-32 channels |
| FastTracker II | `.xm` | Envelopes, instruments |
| Impulse Tracker | `.it` | Full-featured |
| Others | `.mptm`, `.669`, `.umx`, etc. | See libopenmpt docs |

### 8B.5.3 Module Metadata API

```cpp
table Gfx3DBindings::getModuleInfo(HSQUIRRELVM vm, ModuleHandle h) {
    if (!m_modules.valid(h)) {
        sq_pushnull(vm);
        return;
    }
    
    ModuleData& mod = m_modules.get(h);
    openmpt_module* m = mod.module;
    
    sq_newtable(vm);
    
    // Title
    const char* title = openmpt_module_get_metadata(m, "title");
    pushTableString(vm, "title", title ? title : "");
    openmpt_free_string(title);
    
    // Duration
    sq_pushstring(vm, "duration", -1);
    sq_pushfloat(vm, (float)openmpt_module_get_duration_seconds(m));
    sq_newslot(vm, -3, SQFalse);
    
    // Channels
    sq_pushstring(vm, "channels", -1);
    sq_pushinteger(vm, openmpt_module_get_num_channels(m));
    sq_newslot(vm, -3, SQFalse);
    
    // Patterns
    sq_pushstring(vm, "patterns", -1);
    sq_pushinteger(vm, openmpt_module_get_num_patterns(m));
    sq_newslot(vm, -3, SQFalse);
    
    // Instruments/Samples
    sq_pushstring(vm, "instruments", -1);
    sq_pushinteger(vm, openmpt_module_get_num_instruments(m));
    sq_newslot(vm, -3, SQFalse);
    
    sq_pushstring(vm, "samples", -1);
    sq_pushinteger(vm, openmpt_module_get_num_samples(m));
    sq_newslot(vm, -3, SQFalse);
}
```

---

## 8B.6 Sound Effect Mixer (Normative)

### 8B.6.1 Sound Bank

```cpp
struct LoadedSound {
    std::vector<float> samples;  // Interleaved stereo (L R L R...)
    int sampleRate;
    int channels;  // 1 or 2
    size_t frameCount;
};

class SoundBank {
public:
    SoundHandle loadSound(const std::string& vfsPath) {
        // 1. Read file
        std::vector<uint8_t> data;
        if (!m_vfs->readBytes(vfsPath, data)) {
            setLastError("audio.loadSound: file not found: " + vfsPath);
            return 0;
        }
        
        // 2. Decode based on format
        LoadedSound sound;
        if (isWAV(data)) {
            if (!decodeWAV(data, sound)) return 0;
        } else if (isOGG(data)) {
            if (!decodeOGG(data, sound)) return 0;
        } else {
            setLastError("audio.loadSound: unsupported format");
            return 0;
        }
        
        // 3. Convert to target sample rate if needed
        if (sound.sampleRate != m_targetSampleRate) {
            resample(sound, m_targetSampleRate);
        }
        
        // 4. Convert mono to stereo if needed
        if (sound.channels == 1) {
            monoToStereo(sound);
        }
        
        // 5. Store
        SoundHandle h = m_sounds.allocate();
        m_sounds.get(h) = std::move(sound);
        
        return h;
    }
    
    void freeSound(SoundHandle h) {
        m_sounds.free(h);
    }
    
    const LoadedSound* getSound(SoundHandle h) const {
        return m_sounds.valid(h) ? &m_sounds.get(h) : nullptr;
    }
    
private:
    HandlePool<LoadedSound> m_sounds;
    VFS* m_vfs;
    int m_targetSampleRate = 48000;
};
```

### 8B.6.2 Voice System

```cpp
using VoiceId = uint32_t;

struct Voice {
    bool active = false;
    SoundHandle sound = 0;
    
    size_t position = 0;      // Current playback position (frames)
    float volume = 1.0f;
    float pan = 0.0f;         // -1 left, 0 center, +1 right
    float pitch = 1.0f;       // Playback rate multiplier
    
    // For voice stealing priority
    uint64_t startTime = 0;
    
    // Fractional position for resampling
    double fractionalPos = 0.0;
};

class VoiceMixer {
public:
    static constexpr int MAX_VOICES = 32;
    
    VoiceId playSound(SoundHandle sound, float vol, float pan, float pitch) {
        // Find free voice or steal oldest
        int voiceIdx = findFreeVoice();
        
        if (voiceIdx < 0) {
            voiceIdx = stealOldestVoice();
        }
        
        Voice& voice = m_voices[voiceIdx];
        voice.active = true;
        voice.sound = sound;
        voice.position = 0;
        voice.fractionalPos = 0.0;
        voice.volume = std::clamp(vol, 0.0f, 1.0f);
        voice.pan = std::clamp(pan, -1.0f, 1.0f);
        voice.pitch = std::clamp(pitch, 0.1f, 4.0f);
        voice.startTime = m_nextVoiceTime++;
        
        return voiceIdFromIndex(voiceIdx);
    }
    
    void stopVoice(VoiceId id) {
        int idx = indexFromVoiceId(id);
        if (idx >= 0 && idx < MAX_VOICES) {
            m_voices[idx].active = false;
        }
    }
    
    void setVoiceVolume(VoiceId id, float vol) {
        int idx = indexFromVoiceId(id);
        if (idx >= 0 && idx < MAX_VOICES && m_voices[idx].active) {
            m_voices[idx].volume = std::clamp(vol, 0.0f, 1.0f);
        }
    }
    
    void setVoicePan(VoiceId id, float pan) {
        int idx = indexFromVoiceId(id);
        if (idx >= 0 && idx < MAX_VOICES && m_voices[idx].active) {
            m_voices[idx].pan = std::clamp(pan, -1.0f, 1.0f);
        }
    }
    
    void setVoicePitch(VoiceId id, float pitch) {
        int idx = indexFromVoiceId(id);
        if (idx >= 0 && idx < MAX_VOICES && m_voices[idx].active) {
            m_voices[idx].pitch = std::clamp(pitch, 0.1f, 4.0f);
        }
    }
    
    // Called from audio thread
    void mixIntoBuffer(float* buffer, size_t frames, const SoundBank& bank) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice& voice = m_voices[i];
            if (!voice.active) continue;
            
            const LoadedSound* sound = bank.getSound(voice.sound);
            if (!sound) {
                voice.active = false;
                continue;
            }
            
            mixVoice(voice, *sound, buffer, frames);
        }
    }
    
private:
    int findFreeVoice() {
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (!m_voices[i].active) return i;
        }
        return -1;
    }
    
    int stealOldestVoice() {
        int oldest = 0;
        uint64_t oldestTime = m_voices[0].startTime;
        
        for (int i = 1; i < MAX_VOICES; ++i) {
            if (m_voices[i].startTime < oldestTime) {
                oldest = i;
                oldestTime = m_voices[i].startTime;
            }
        }
        
        return oldest;
    }
    
    void mixVoice(Voice& voice, const LoadedSound& sound, 
                   float* buffer, size_t frames);
    
    Voice m_voices[MAX_VOICES];
    uint64_t m_nextVoiceTime = 1;
};
```

### 8B.6.3 Voice Mixing with Resampling

```cpp
void VoiceMixer::mixVoice(Voice& voice, const LoadedSound& sound,
                           float* buffer, size_t frames) {
    // Calculate panning gains
    float panLeft = (1.0f - voice.pan) * 0.5f;
    float panRight = (1.0f + voice.pan) * 0.5f;
    float vol = voice.volume;
    
    const float* samples = sound.samples.data();
    size_t totalFrames = sound.frameCount;
    
    for (size_t i = 0; i < frames; ++i) {
        // Calculate source position
        double srcPos = voice.fractionalPos;
        size_t srcFrame = static_cast<size_t>(srcPos);
        
        if (srcFrame >= totalFrames) {
            voice.active = false;
            break;
        }
        
        // Linear interpolation for quality
        float frac = static_cast<float>(srcPos - srcFrame);
        size_t nextFrame = std::min(srcFrame + 1, totalFrames - 1);
        
        float sampleL = samples[srcFrame * 2 + 0] * (1.0f - frac) +
                        samples[nextFrame * 2 + 0] * frac;
        float sampleR = samples[srcFrame * 2 + 1] * (1.0f - frac) +
                        samples[nextFrame * 2 + 1] * frac;
        
        // Apply volume and panning, mix into buffer
        buffer[i * 2 + 0] += sampleL * vol * panLeft;
        buffer[i * 2 + 1] += sampleR * vol * panRight;
        
        // Advance position by pitch-adjusted rate
        voice.fractionalPos += voice.pitch;
    }
    
    voice.position = static_cast<size_t>(voice.fractionalPos);
}
```

---

## 8B.7 Main Audio Callback (Normative)

### 8B.7.1 Complete Mixing Pipeline

```cpp
class AudioMixer {
public:
    void mixAudio(float* buffer, size_t frames) {
        // 1. Clear output buffer
        memset(buffer, 0, frames * 2 * sizeof(float));
        
        // 2. Process pending commands from main thread
        processCommands();
        
        // 3. Render module music
        m_moduleBuffer.resize(frames * 2);
        size_t moduleFrames = m_modulePlayer.render(
            m_moduleBuffer.data(), frames, m_sampleRate
        );
        
        // Mix module into output
        for (size_t i = 0; i < moduleFrames * 2; ++i) {
            buffer[i] += m_moduleBuffer[i];
        }
        
        // 4. Mix sound effects
        m_voiceMixer.mixIntoBuffer(buffer, frames, m_soundBank);
        
        // 5. Apply master volume
        for (size_t i = 0; i < frames * 2; ++i) {
            buffer[i] *= m_masterVolume;
        }
        
        // 6. Clamp output to [-1, 1]
        for (size_t i = 0; i < frames * 2; ++i) {
            buffer[i] = std::clamp(buffer[i], -1.0f, 1.0f);
        }
    }
    
private:
    ModulePlayer m_modulePlayer;
    VoiceMixer m_voiceMixer;
    SoundBank m_soundBank;
    AudioCommandQueue m_commandQueue;
    
    std::vector<float> m_moduleBuffer;
    
    float m_masterVolume = 1.0f;
    int m_sampleRate = 48000;
};
```

### 8B.7.2 Latency and Buffer Sizing (Normative)

| Scenario | Buffer Size | Latency | Notes |
|----------|-------------|---------|-------|
| Low latency | 256 samples | ~5.3ms | May cause underruns on slow systems |
| Balanced | 512 samples | ~10.6ms | Recommended default |
| High stability | 1024 samples | ~21.3ms | For problematic audio drivers |

The runtime SHOULD allow configuration via Workbench settings.

---

## 8B.8 WAV and OGG Decoding (Normative)

### 8B.8.1 WAV Decoder

```cpp
bool decodeWAV(const std::vector<uint8_t>& data, LoadedSound& out) {
    // Minimal WAV parser
    if (data.size() < 44) return false;
    if (memcmp(data.data(), "RIFF", 4) != 0) return false;
    if (memcmp(data.data() + 8, "WAVE", 4) != 0) return false;
    
    // Find fmt chunk
    size_t pos = 12;
    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    
    while (pos + 8 < data.size()) {
        const char* chunkId = reinterpret_cast<const char*>(data.data() + pos);
        uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(data.data() + pos + 4);
        
        if (memcmp(chunkId, "fmt ", 4) == 0) {
            audioFormat = *reinterpret_cast<const uint16_t*>(data.data() + pos + 8);
            numChannels = *reinterpret_cast<const uint16_t*>(data.data() + pos + 10);
            sampleRate = *reinterpret_cast<const uint32_t*>(data.data() + pos + 12);
            bitsPerSample = *reinterpret_cast<const uint16_t*>(data.data() + pos + 22);
        }
        
        if (memcmp(chunkId, "data", 4) == 0) {
            // Found audio data
            const uint8_t* audioData = data.data() + pos + 8;
            size_t audioBytes = std::min<size_t>(chunkSize, data.size() - pos - 8);
            
            return decodeWAVData(audioData, audioBytes, audioFormat,
                                  numChannels, sampleRate, bitsPerSample, out);
        }
        
        pos += 8 + chunkSize;
        if (chunkSize & 1) pos++;  // Pad to word boundary
    }
    
    return false;
}

bool decodeWAVData(const uint8_t* data, size_t bytes, 
                    uint16_t format, uint16_t channels, 
                    uint32_t rate, uint16_t bits, LoadedSound& out) {
    out.sampleRate = rate;
    out.channels = channels;
    
    if (format == 1) {  // PCM
        if (bits == 16) {
            size_t sampleCount = bytes / 2;
            out.samples.resize(sampleCount);
            const int16_t* src = reinterpret_cast<const int16_t*>(data);
            for (size_t i = 0; i < sampleCount; ++i) {
                out.samples[i] = src[i] / 32768.0f;
            }
        } else if (bits == 8) {
            size_t sampleCount = bytes;
            out.samples.resize(sampleCount);
            for (size_t i = 0; i < sampleCount; ++i) {
                out.samples[i] = (data[i] - 128) / 128.0f;
            }
        } else {
            return false;  // Unsupported bit depth
        }
    } else if (format == 3) {  // IEEE Float
        size_t sampleCount = bytes / sizeof(float);
        out.samples.resize(sampleCount);
        memcpy(out.samples.data(), data, bytes);
    } else {
        return false;  // Unsupported format
    }
    
    out.frameCount = out.samples.size() / channels;
    return true;
}
```

### 8B.8.2 OGG Vorbis Decoder (using stb_vorbis)

```cpp
bool decodeOGG(const std::vector<uint8_t>& data, LoadedSound& out) {
    int channels, sampleRate;
    short* decoded;
    
    int sampleCount = stb_vorbis_decode_memory(
        data.data(), data.size(),
        &channels, &sampleRate, &decoded
    );
    
    if (sampleCount <= 0) {
        setLastError("audio.loadSound: failed to decode OGG");
        return false;
    }
    
    out.sampleRate = sampleRate;
    out.channels = channels;
    out.frameCount = sampleCount;
    out.samples.resize(sampleCount * channels);
    
    // Convert int16 to float
    for (int i = 0; i < sampleCount * channels; ++i) {
        out.samples[i] = decoded[i] / 32768.0f;
    }
    
    free(decoded);
    return true;
}
```

---

## 8B.9 Resampling (Normative)

### 8B.9.1 Linear Interpolation Resampler

```cpp
void resample(LoadedSound& sound, int targetRate) {
    if (sound.sampleRate == targetRate) return;
    
    double ratio = static_cast<double>(targetRate) / sound.sampleRate;
    size_t newFrameCount = static_cast<size_t>(sound.frameCount * ratio);
    
    std::vector<float> newSamples;
    newSamples.resize(newFrameCount * sound.channels);
    
    for (size_t i = 0; i < newFrameCount; ++i) {
        double srcPos = i / ratio;
        size_t srcFrame = static_cast<size_t>(srcPos);
        float frac = static_cast<float>(srcPos - srcFrame);
        
        size_t nextFrame = std::min(srcFrame + 1, sound.frameCount - 1);
        
        for (int ch = 0; ch < sound.channels; ++ch) {
            float s0 = sound.samples[srcFrame * sound.channels + ch];
            float s1 = sound.samples[nextFrame * sound.channels + ch];
            newSamples[i * sound.channels + ch] = s0 * (1.0f - frac) + s1 * frac;
        }
    }
    
    sound.samples = std::move(newSamples);
    sound.frameCount = newFrameCount;
    sound.sampleRate = targetRate;
}
```

### 8B.9.2 Mono to Stereo Conversion

```cpp
void monoToStereo(LoadedSound& sound) {
    if (sound.channels != 1) return;
    
    std::vector<float> stereo;
    stereo.resize(sound.frameCount * 2);
    
    for (size_t i = 0; i < sound.frameCount; ++i) {
        stereo[i * 2 + 0] = sound.samples[i];
        stereo[i * 2 + 1] = sound.samples[i];
    }
    
    sound.samples = std::move(stereo);
    sound.channels = 2;
}
```

---

## 8B.10 API Bindings Summary

### 8B.10.1 Main Thread API Functions

```cpp
// All these functions enqueue commands; they do NOT block
class AudioBindings {
public:
    // Module
    ModuleHandle loadModule(const std::string& path);
    void freeModule(ModuleHandle m);
    bool playModule(ModuleHandle m, bool loop);
    void stopModule();
    void pauseModule();
    void resumeModule();
    void setModuleVolume(float v);
    bool setModuleTempo(float factor);
    bool seekModuleSeconds(float t);
    table getModuleInfo(ModuleHandle m);
    
    // Sound
    SoundHandle loadSound(const std::string& path);
    void freeSound(SoundHandle s);
    VoiceId playSound(SoundHandle s, float vol, float pan, float pitch);
    void stopVoice(VoiceId v);
    void setVoiceVolume(VoiceId v, float vol);
    void setVoicePan(VoiceId v, float pan);
    void setVoicePitch(VoiceId v, float pitch);
    
    // Master
    void setMasterVolume(float v);
    float getMasterVolume();
    void stopAll();
};
```

---

## 8B.11 Resource Limits (Normative)

| Resource | Default Limit | Configurable |
|----------|---------------|--------------|
| Loaded modules | 8 | Yes |
| Module file size | 8 MB | Yes |
| Loaded sounds | 64 | Yes |
| Sound memory (decoded) | 32 MB | Yes |
| SFX voices | 32 | Hardcoded |
| Command queue | 256 entries | No |

---

## 8B.12 Compliance Checklist (Chapter 8B)

An implementation is compliant with Chapter 8B if it:

1. Uses a **lock-free command queue** for main-to-audio thread communication.
2. Performs **NO file I/O or memory allocation** in the audio callback.
3. Implements **libopenmpt integration** for tracker module playback.
4. Supports **WAV (PCM 8/16/32f)** and **OGG Vorbis** for sound effects.
5. Implements **voice stealing** (oldest first) when SFX voices are exhausted.
6. Applies **linear interpolation resampling** for pitch and sample rate conversion.
7. Correctly implements **stereo panning** and per-voice volume.
8. Limits output to **[-1, 1]** range to prevent clipping artifacts.
9. Maintains **deterministic voice allocation** order.
10. Cleans up all audio resources on cartridge stop/reload.

---

*End of Chapter 8B*

---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
