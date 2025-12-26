# Blueprint v0.3 — Chapter 6: Concurrency, Threading, Determinism

## Concurrency Baseline
inherit from blueprint_v0.2_ch6.md

---

## [ARCH-12] Config Hot-Apply Concurrency Model (v0.3 delta)
### Threads involved
- **Main/UI thread**: ImGui frame loop, editor rendering, input dispatch, Problems UI, and all “apply” operations (theme, keymap, fonts). [REQ-91], [API-17]
- **Worker thread**: config parse + validation + snapshot build (TOML + schema checks). [ARCH-12], [DEC-64]

### Hard rule
- **No UI, renderer, font atlas, or ImGui calls occur off the main thread.** [REQ-91], [REQ-92]

### Event source and trigger
- Config reload is triggered **only** by IDE “save” events from `DocumentSystem` for paths under `config/*.toml`. No filesystem watcher. [REQ-91]

---

## [DEC-71] Snapshot Publication Strategy
**Decision**
- Publish configuration as `std::shared_ptr<const ConfigSnapshot>` and swap on the main thread only.
- `ConfigSystem::Current()` is main-thread only (consistent with existing UI ownership patterns).

**Rationale**
- Avoids atomic shared_ptr contention and simplifies correctness guarantees.
- Main-thread-only access matches the apply pipeline and avoids cross-thread lifetime pitfalls.

**Alternatives**
- atomic shared_ptr publication readable from any thread.
- RCU-like epoch scheme.

**Consequences**
- Any subsystem needing config off-thread must receive a copy explicitly (not required in v0.3).

---

## [CONC-01] Debounce + Cancellation (v0.3)
### Rules
- Each config save event increments a monotonically increasing `reload_seq`.
- Debounce timer schedules a reload “tick” at `t + debounce_ms`.
- When the worker finishes building a snapshot, it returns `(seq, snapshot, diagnostics)`.
- The main thread applies a snapshot **only if** `seq` matches the latest requested `reload_seq`.
  - Older results are discarded (still allowed to publish diagnostics, but must replace per-file diagnostics deterministically—see below).

### Determinism and UX
- Debounce interval is constant (configurable in code; not exposed in TOML for v0.3).
- Apply ordering is fixed:
  1) Scheme registry + active scheme resolution
  2) Theme apply (editor + syntax + ImGui)
  3) Keymap apply
  4) Font apply (with last-known-good fallback)
  5) Repaint/re-highlight request [REQ-94]

---

## [CONC-02] Diagnostics Publication and Replacement
### Rules
- Diagnostics are produced on the worker thread as plain value objects (no UI calls). [REQ-97]
- On main thread:
  - Replace diagnostics **per file** (`config/editor.toml`, etc.) atomically in the Problems model (single replace call per file). [API-18]
  - If an older worker result arrives after a newer one, it must **not** overwrite newer diagnostics for that file. Use `reload_seq` gating identical to snapshot apply.

### Consequence
- Problems window always reflects the most recently saved config state, even when saves are rapid. [REQ-91], [REQ-97]

---

## [CONC-03] Font Rebuild Safety
### Rules
- Font rebuild occurs only on the main thread. [REQ-92]
- Apply uses a two-phase approach:
  1) Build new font resources/atlas into a temporary object.
  2) Swap into live editor/ImGui state only if build succeeds.
- On failure:
  - Keep previous resources untouched (last-known-good).
  - Emit an error diagnostic tied to `font.*` key paths (file-level if exact range unavailable). [REQ-92], [REQ-97]

---

## [CONC-04] Tree-sitter Highlight Updates
### Rules
- Parsing and incremental tree updates follow existing ParseService threading rules (inherit from v0.2).
- Theme changes do **not** require re-parsing:
  - Only the token→color lookup changes, so buffers request repaint/re-highlight using the same capture spans. [REQ-94]
- Capture→token mapping tables are immutable and shared safely (const storage). [REQ-94]

---

## [CONC-05] Locks and Shared State Rules
- Config TOML parsing and validation must not lock UI/editor global locks for long durations.
- Any shared mutable state between ConfigSystem and UIShell must be updated via:
  - main-thread queue (preferred), or
  - short-lived mutex sections with no re-entrancy into UI.
- Never hold a lock while calling into:
  - ImGui, renderer, font atlas build, Problems sink, or DocumentSystem callbacks.

---

## [CONC-06] Stress / Sanitizers
- Run ThreadSanitizer (TSan) for config hot-apply tests where supported.
- Add a stress harness that:
  - repeatedly saves config files (synthetic) and triggers reload
  - interleaves with rapid UI frames and input events
  - asserts no deadlocks, no crashes, and deterministic applied snapshot selection by `reload_seq`.

---

## [TEST-45] Hot-Apply Thread Safety + `reload_seq` Determinism
- Simulate rapid successive saves of:
  - `config/editor.toml` (scheme id toggles)
  - `config/keys.toml` (binding changes)
- Force worker completion out-of-order (older completes after newer).
- Assert:
  - only latest seq snapshot is applied
  - diagnostics for each file match latest seq
  - UI remains responsive; no deadlocks
- Run under TSan where available and treat data races as test failures.

