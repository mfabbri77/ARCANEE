# Chapter 8 — Audio System and API (Tracker Modules via libopenmpt, SFX Mixing) (ARCANEE v0.1)

## 8.1 Overview

ARCANEE v0.1 provides an audio subsystem designed for a “modern home computer” feel with **tracker-first music** and conventional sound effects. Music playback is implemented using **libopenmpt** for module formats (e.g., MOD, XM, IT, S3M and others). Sound effects are mixed as PCM samples. Output is delivered via the platform audio layer (SDL2).

This chapter specifies:

* Audio architecture, threads, and real-time constraints
* Output format and latency requirements
* Tracker module playback semantics (libopenmpt-backed)
* SFX sample playback semantics
* Public `audio.*` API
* Determinism and synchronization rules
* Quotas and limits
* Error handling and Workbench diagnostics

---

## 8.2 Audio Architecture (Normative)

### 8.2.1 Threads and Responsibilities

ARCANEE audio operates with three conceptual roles:

1. **Main Thread (Control Plane)**

   * Receives cartridge `audio.*` calls.
   * Updates audio state (what is playing, volumes, parameters).
   * Enqueues commands to the audio engine.

2. **Audio Render Thread / Callback (Real-Time Plane)**

   * Generates output PCM frames at the device sample rate.
   * Executes mixing of module music and SFX voices.
   * MUST NOT call into the Squirrel VM, PhysFS, GPU APIs, or any blocking system calls.
   * MUST NOT take unbounded locks.

3. **Worker Threads (Decode/Prep Plane, optional)**

   * May perform file decode/loading, resampling preparation, and module pre-rendering into ring buffers.
   * Must communicate with the audio callback via bounded, lock-free (or bounded-lock) buffers.

### 8.2.2 Command Queue

All cartridge audio actions MUST be translated into **audio commands** placed on a thread-safe queue:

* play/stop/pause/resume module
* start/stop SFX voices
* parameter changes (volume, pan, pitch)
* set master volume

The audio callback consumes commands at safe points (e.g., at buffer boundaries) to avoid mid-buffer discontinuities.

#### 8.2.2.1 Reference Implementation (Normative)

```c++
// Lock-free audio command queue - NORMATIVE reference implementation

enum class AudioCmdType : uint8_t {
    PlayModule, StopModule, PauseModule, ResumeModule,
    SetModuleVolume, SetModuleTempo,
    PlaySound, StopVoice,
    SetVoiceVolume, SetVoicePan, SetVoicePitch,
    SetMasterVolume, StopAll
};

struct AudioCommand {
    AudioCmdType type;
    uint32_t handle;     // Module/Sound/Voice handle
    float param1;        // Volume, pitch, or tempo
    float param2;        // Pan
};

// Single-producer (main thread), single-consumer (audio thread)
template<size_t N>
class AudioCommandQueue {
    std::array<AudioCommand, N> buffer;
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};
    
public:
    // Called from MAIN THREAD only
    bool push(const AudioCommand& cmd) {
        size_t w = writePos.load(std::memory_order_relaxed);
        size_t next = (w + 1) % N;
        
        if (next == readPos.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        buffer[w] = cmd;
        writePos.store(next, std::memory_order_release);
        return true;
    }
    
    // Called from AUDIO CALLBACK only
    bool pop(AudioCommand& cmd) {
        size_t r = readPos.load(std::memory_order_relaxed);
        
        if (r == writePos.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }
        
        cmd = buffer[r];
        readPos.store((r + 1) % N, std::memory_order_release);
        return true;
    }
};

// Usage in audio callback
void audioCallback(float* output, size_t frames) {
    // Process commands at buffer start (safe point)
    AudioCommand cmd;
    while (g_cmdQueue.pop(cmd)) {
        processCommand(cmd);  // Updates mixer state
    }
    
    // Mix audio
    mixModuleAudio(output, frames);
    mixSfxVoices(output, frames);
    applyMasterVolume(output, frames);
}
```

---

## 8.3 Output Format and Device Configuration (Normative)

### 8.3.1 Device Sample Rate and Channels

The runtime MUST support at minimum:

* Output channels: stereo (2)
* Output sample format: 32-bit float (preferred) or 16-bit signed integer (fallback)

The device sample rate is platform-dependent. The runtime MUST:

* Query the actual device sample rate from SDL.
* Configure internal resampling so that module playback and SFX produce correct pitch at the device rate.

### 8.3.2 Buffer Size and Latency

ARCANEE SHOULD target low interactive latency appropriate for a “home computer”:

* Default audio buffer target: 10–30 ms of audio (platform-dependent).
* Workbench MAY expose an “Audio Latency” setting.

The runtime MUST remain stable if the user selects larger buffers.

### 8.3.3 Clipping and Headroom

The mixer MUST:

* Mix into a float accumulator (recommended) with headroom.
* Apply a limiter or soft clipper (recommended) or at minimum clamp to [-1,1] before converting to device format.
* Avoid NaNs/infs; any invalid values must be sanitized.

---

## 8.4 Tracker Module Playback Model (Normative)

### 8.4.1 Supported Formats

ARCANEE uses libopenmpt for module decoding. The runtime MUST document which module formats are supported by the embedded libopenmpt build. (libopenmpt supports a wide range; ARCANEE must still document the shipped configuration.)

### 8.4.2 Module Loading

Modules are loaded from VFS paths subject to permissions:

* Allowed namespaces: `cart:/`, `save:/`, `temp:/` (read rules apply)
* `permissions.audio` MUST be `true` or module loading fails.

### 8.4.3 Playback Semantics

ARCANEE v0.1 supports exactly **one active music module** at a time (simplifies “home computer” UX). If a new module is started while one is playing:

* The previous module MUST stop (or fade out if implemented; v0.1 fade is optional).

Playback state includes:

* playing / paused / stopped
* loop enabled/disabled
* volume, tempo factor (optional), and position

### 8.4.4 Seeking and Looping

* Looping is defined as restarting playback from time 0 when end-of-module is reached.
* `seek` moves to an approximate time position. Because tracker timing may be pattern-based, time-to-position mapping may be approximate; the runtime MUST document the behavior and remain deterministic.

### 8.4.5 Determinism

For a given module file and identical control inputs (play/stop/seek/tempo changes at the same simulation ticks), ARCANEE MUST produce stable behavior. Absolute sample-level determinism across different audio devices and sample rates is not required in v0.1; however:

* Musical timing and state progression MUST be consistent (no random jumps).

---

## 8.5 SFX Playback Model (Normative)

### 8.5.1 Voices and Channels

ARCANEE supports multiple concurrent SFX voices.

* Default policy: `caps.audio_channels` (e.g., 32).
* Exceeding voice count MUST deterministically steal voices by policy:

  * RECOMMENDED policy: steal the oldest quietest or oldest instance (document which).

### 8.5.2 Sound Asset Types

v0.1 requires at least:

* WAV (PCM) support for SFX
  Optional:
* OGG/Vorbis, FLAC (document if included)

### 8.5.3 Resampling and Pitch

`pitch` changes playback rate. Implementation MUST:

* Use deterministic resampling per platform build (e.g., linear interpolation resampler is acceptable for v0.1).
* Document pitch range and behavior.

### 8.5.4 Pan Model

`pan ∈ [-1, +1]` where:

* -1 = full left, 0 = center, +1 = full right
  Pan law MUST be documented. v0.1 RECOMMENDS equal-power panning.

---

## 8.6 Public Audio API (`audio.*`) (Normative)

### 8.6.1 Permission Enforcement

If `permissions.audio=false`:

* All `audio.*` calls MUST fail safely (return `false/0/null`) and set last error.

### 8.6.2 Module API

* `audio.loadModule(path: string) -> ModuleHandle|0`
* `audio.freeModule(m: ModuleHandle) -> void`

  * If `m` is currently playing, it MUST stop first.
* `audio.playModule(m: ModuleHandle, loop: bool=true) -> bool`
* `audio.stopModule() -> void`
* `audio.pauseModule() -> void`
* `audio.resumeModule() -> void`
* `audio.setModuleVolume(v: float) -> void`  // clamps [0,1]
* `audio.setModuleTempo(factor: float) -> bool`

  * factor range is implementation-defined; v0.1 RECOMMENDS [0.5, 2.0]
* `audio.seekModuleSeconds(t: float) -> bool`
* `audio.getModuleInfo(m: ModuleHandle) -> table|null`

  * Minimum returned keys:

    * `title: string`
    * `type: string` (format name)
    * `durationApprox: float`
    * `channels: int`
    * `patterns: int` (if available)
    * `instruments: int` (if available)

### 8.6.3 SFX API

* `audio.loadSound(path: string) -> SoundHandle|0`
* `audio.freeSound(s: SoundHandle) -> void`
* `audio.playSound(s: SoundHandle, vol: float=1.0, pan: float=0.0, pitch: float=1.0) -> VoiceId|0`
* `audio.stopVoice(voice: VoiceId) -> void`
* `audio.setVoiceVolume(voice: VoiceId, vol: float) -> void`
* `audio.setVoicePan(voice: VoiceId, pan: float) -> void`
* `audio.setVoicePitch(voice: VoiceId, pitch: float) -> void`

### 8.6.4 Master Controls

* `audio.setMasterVolume(v: float) -> void`
* `audio.getMasterVolume() -> float`
* `audio.stopAll() -> void`

  * Stops module and all SFX voices.

---

## 8.7 Scheduling and Tick Alignment (Normative)

### 8.7.1 Command Timing

Because the audio callback runs asynchronously, ARCANEE MUST define how `audio.*` commands map to audible changes.

v0.1 requirement:

* Commands issued during `update()` MUST be applied no later than the next audio buffer boundary.
* Workbench MAY display an approximate “audio command latency” based on buffer size.

### 8.7.2 Deterministic Ordering

If multiple audio commands are issued within the same `update()` tick, they MUST be applied in the order they were issued.

---

## 8.8 Limits, Quotas, and Memory (Normative)

### 8.8.1 Module Limits

The runtime SHOULD enforce:

* Maximum module file size (policy-defined; recommended: 64–128 MB)
* Maximum decoded buffer residency (avoid unbounded caching)

### 8.8.2 Sound Limits

* Maximum simultaneous voices: `caps.audio_channels` clamped by runtime policy.
* Maximum number of loaded sounds (policy-defined).
* Maximum total sound memory per cartridge (policy-defined).

### 8.8.3 Failure Behavior

If limits are exceeded:

* Loading fails safely with last error.
* If voice limit exceeded, deterministic voice stealing policy is applied (documented).

---

## 8.9 Error Handling and Diagnostics (Normative)

### 8.9.1 API Failures

All `audio.*` functions MUST:

* Validate handle validity and type.
* Validate parameters (finite values; reasonable ranges).
* On failure: return `false/0/null`, set `sys.getLastError()`.
* In Workbench Dev Mode: log a diagnostic message.

### 8.9.2 Backend Errors

If the audio device fails or is lost:

* The runtime MUST attempt to reinitialize the device.
* If reinit fails, audio APIs MUST fail safely and Workbench MUST show an audio error indicator.

---

## 8.10 Compliance Checklist (Chapter 8)

An implementation is compliant with Chapter 8 if it:

1. Implements the thread separation: main thread control, real-time audio callback mixing, optional worker decode.
2. Enforces “no VM, no I/O, no blocking” constraints in the audio callback.
3. Supports one active module music stream via libopenmpt with play/stop/pause/seek and volume controls.
4. Supports SFX sound loading and multi-voice mixing with deterministic voice-stealing policy.
5. Enforces permission checks and sandboxed VFS loading for all audio assets.
6. Provides stable command ordering and bounded command-to-audio latency.
7. Enforces limits and reports errors via `sys.getLastError()` and Workbench diagnostics.