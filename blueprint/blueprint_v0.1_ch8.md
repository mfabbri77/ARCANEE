# Blueprint v0.1 — Chapter 8: Tooling (Workbench), Debugging, Profiling

## 8.1 Workbench Features (v1.0)
- [REQ-28] Workbench must provide:
  - cartridge/project browser
  - code editor (minimum: text editor with syntax highlighting best-effort)
  - run/stop/reload + hot reload for scripts
  - log console viewer + filter
  - basic debugger: breakpoints + step/continue (minimum viable)
  - profiler overlay: tick time, draw time, memory, and log rate

## 8.2 UI Tech Choice
- [DEC-25] Workbench UI stack: **ImGui** + docking.
  - Rationale: cross-platform, fast iteration, common tooling patterns.
  - Alternatives: SDL-only UI; Qt.
  - Consequences: add FetchContent dep + style/asset integration.

## 8.3 Hot Reload
- [DEC-26] Hot reload strategy for v1.0:
  - scripts: reload VM and re-run cartridge entry
  - assets: best-effort reload for touched files; otherwise require restart
  - file watching: polling fallback if native watchers are too complex for v1.0
- [TEST-13] ensures script reload works without crash/leaks.

## 8.4 Debugger (Minimum Viable)
- [REQ-58] Provide:
  - breakpoints set by file:line
  - step/continue
  - stack trace and locals (best-effort; at least call stack)
- [TEST-24] `debugger_smoke`: set breakpoint in sample, run until hit, step N times.

## 8.5 Profiling
- [REQ-59] Provide in-app profiler overlay:
  - tick duration histogram (recent window)
  - per-subsystem timers (runtime/script/render/audio/workbench)
  - allocation counters (frame + total)
- [TEST-25] `profiler_overlay_smoke`: validates instrumentation emits non-empty metrics.


