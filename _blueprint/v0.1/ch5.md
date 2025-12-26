<!-- _blueprint/v0.1/ch5.md -->

# Blueprint v0.1 — Chapter 5: Data Model, Memory, Hot Paths

## 5.1 Memory Budgets & Limits
- [REQ-23] VM memory hard cap default: **128MB**; soft cap: **64MB** (configurable).
- [REQ-43] Cartridge asset limits (defaults; configurable):
  - max single file: 64MB
  - max total mounted assets: 512MB
  - max decoded texture per resource: 256MB (guardrails)
- [REQ-44] Workbench log ring buffer: default 10k entries or 8MB, whichever first.

## 5.2 Allocator Strategy
- [DEC-23] Use a two-tier allocation model:
  - `FrameArena` reset each frame/tick (hot path, avoids per-frame heap churn)
  - `ResourceHeap` for long-lived GPU/audio/script resources (tracked + released on unload)
  - Rationale: stable frame times and easier leak detection.
  - Alternatives: global new/delete only; third-party allocators.
  - Consequences: introduce memory tests and instrumentation.

## 5.3 Data Ownership Rules
- [REQ-45] No raw owning pointers across subsystem boundaries. Use:
  - owning `std::unique_ptr`
  - non-owning `T*`/`std::span` with documented lifetime
  - handles for GPU/audio resources.

## 5.4 Cartridge Data Model
- [REQ-46] Cartridge manifest must be validated before mount:
  - required fields present
  - schema version supported
  - entry script path within cartridge namespace
- [TEST-12] verifies rejection of malformed/unsafe manifests.

## 5.5 Persistence (Save Data)
- [REQ-47] Save data is permitted only within a **cartridge-scoped save namespace** inside the sandbox (e.g., `save:/`).
- [TEST-15] `save_namespace_only`: attempts to write outside save namespace must fail.

## 5.6 Performance Hot Paths
- [REQ-48] Main tick path must avoid unbounded allocations:
  - no per-entity heap allocs in update
  - submit render/audio commands via pre-sized vectors/ring buffers
- [TEST-16] `alloc_budget_tick`: tracks allocations per tick under a sample workload.


