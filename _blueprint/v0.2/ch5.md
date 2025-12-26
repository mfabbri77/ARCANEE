<!-- _blueprint/v0.2/ch5.md -->

# Blueprint v0.2 — Chapter 5: Data Layout, Hot Paths, Memory

## 5.1 Data Model Overview
inherit from /blueprint/blueprint_v0.1_ch5.md

### 5.1.1 v0.2 IDE Data Model Additions
- [REQ-67] TextBuffer introduces:
  - canonical UTF-8 text storage
  - multi-cursor + column selection state
  - undo tree nodes referencing edit spans
- [REQ-69] ParseService introduces:
  - per-document tree-sitter CST state
  - query result caches (highlights/folds/outline) per doc revision
- [REQ-68] SearchService introduces:
  - compiled PCRE2 patterns per search session
  - streaming result buffers
- [REQ-74] TimelineStore introduces:
  - SQLite metadata DB
  - blob store with zstd-compressed content keyed by xxHash
- [REQ-72] DapClient and [REQ-73] LspClient introduce:
  - protocol message buffers
  - per-session state (breakpoints/stack/vars/diagnostics)

## 5.2 Memory Ownership & Lifetime Rules
inherit from /blueprint/blueprint_v0.1_ch5.md

### 5.2.1 IDE-specific ownership rules
- [MEM-03] **Document owns buffer + services**: `Document` owns `TextBuffer`, and owns/creates per-doc handles to `ParseService` request state and LSP document state. Destroying a `Document` must cancel in-flight parse/search/snapshot jobs and drop pending result packets for that document.
- [MEM-04] **Immutable snapshots for background work**: Background workers must never reference `TextBuffer` internal storage directly. They receive `TextSnapshot` objects that are immutable and self-contained.
- [MEM-05] **Result packets are move-only**: All service result packets enqueued to the main-thread queue are move-only, own their payloads, and must not alias mutable storage.

## 5.3 TextBuffer Internal Representation (Hot Path)
> This is the primary IDE hot path.

- [DEC-28] **TextBuffer representation**
  - Decision: implement `TextBuffer` as a **piece-table** with:
    - `original` immutable buffer (loaded file contents)
    - `add` append-only buffer (inserted text)
    - a piece list describing the current document view
    - an auxiliary **line index** (vector of line start byte offsets in the logical view) rebuilt incrementally on edit batches
  - Rationale:
    - fast inserts/deletes without shifting large contiguous buffers
    - undo tree can store pieces/ranges efficiently
    - snapshots can be materialized deterministically when required (tests, Timeline, tree-sitter input)
  - Alternatives:
    - (A) rope (tree of chunks) — good for very large files but more complex to implement + debug.
    - (B) gap buffer — excellent locality but expensive for multi-cursor edits far apart.
  - Consequences:
    - Provide two snapshot surfaces:
      1) **Rope-like reader API** (iterate spans) for services that can consume ranges
      2) **Materialized contiguous UTF-8 string** for tree-sitter (requires contiguous input)
    - Multi-cursor/column edits are applied as a **normalized batch** (sorted, non-overlapping, back-to-front byte ranges).

### 5.3.1 UTF-8 indexing rules
- [MEM-06] **Canonical indexing**
  - Store and edit in UTF-8 bytes.
  - User-facing caret positions are `(line, col)` where `col` is **Unicode scalar count** within the line (not bytes).
  - Internally convert `(line, col)` ⇄ `byte_offset` using:
    - the line index for `line → line_start_byte`
    - a UTF-8 scan from line start for `col → byte_delta` (bounded by typical line lengths)
- [DEC-44] **Column selection definition**
  - Decision: column selection uses `(line, col)` (Unicode-scalar columns) as the rectangle basis; operations translate to per-line byte ranges during execution.
  - Rationale: predictable behavior in presence of multibyte UTF-8; matches mainstream editors.
  - Alternatives: byte-columns (fast but confusing), grapheme-cluster columns (correct but expensive).
  - Consequences: editor tests must include multibyte UTF-8 lines ([TEST-28]).

### 5.3.2 Undo tree data
- [MEM-07] Each undo node stores:
  - operation kind (insert/delete/replace)
  - affected byte ranges in the logical view at the time of edit (normalized)
  - references to inserted/deleted text as slices (pieces) so content is not duplicated
  - parent pointer and optional branch children indices

## 5.4 ParseService Data & Memory
- [MEM-08] tree-sitter CST state is per-document and must be destroyed with the document.
- [MEM-09] Query execution outputs:
  - highlight spans as `(byte_a, byte_b, style_id)` sorted by `byte_a`
  - fold ranges as `(line_a, line_b)` sorted
  - outline items as `(name, line, kind)` sorted by appearance
- [REQ-69] Parse results must include the originating `DocRevision`; UIShell discards stale revisions (Ch3).

### 5.4.1 Materialization for tree-sitter
- [DEC-45] **tree-sitter input materialization**
  - Decision: for each parse batch, `TextSnapshot` materializes a contiguous UTF-8 string for tree-sitter input. The materialization happens on the **worker thread** as part of the parse job.
  - Rationale: tree-sitter APIs typically require contiguous input; keeping this off the UI thread preserves [REQ-76].
  - Alternatives: maintain a continuously materialized buffer on UI thread.
  - Consequences: parse jobs must be cancellable while materializing (check cancel token between chunk copies).

## 5.5 SearchService Data & Memory
- [MEM-10] Search sessions own:
  - compiled PCRE2 pattern (+ optional JIT if enabled later by decision)
  - scan cursor state (files enumerated, bytes scanned)
  - bounded hit queue per session to prevent memory spikes
- [DEC-46] **Search result buffering**
  - Decision: cap in-memory streaming queue per search session (default 10k hits). Beyond cap, emit “truncated” event and require user refinement.
  - Rationale: prevents pathological regex scans from exhausting memory; keeps UI responsive.
  - Alternatives: unlimited buffering, or spilling to disk.
  - Consequences: UI must show truncation banner; tests validate cap behavior ([TEST-29]).

## 5.6 TimelineStore Storage Layout (SQLite + zstd + xxHash)
- [REQ-74] Timeline snapshots store metadata in SQLite and content blobs compressed with zstd and deduped by xxHash.
- [REQ-84] Retention: 15 snapshots for scripts, 3 for other files.
- [REQ-85] Exclusions: ignore rebuildable/temporary artifacts.

### 5.6.1 On-disk schema (v0.2 baseline)
- [DEC-47] **SQLite schema v1 (TimelineStore)**
  - Decision: TimelineStore maintains:
    - `timeline.sqlite3` under `.arcanee/timeline/`
    - `blobs/` directory under `.arcanee/timeline/`
  - Tables (conceptual; exact DDL in implementation):
    - `snapshots(id INTEGER PK, ts_ms INTEGER, file_rel TEXT, is_script INTEGER, revision INTEGER, label TEXT NULL, blob_hash_u64 INTEGER, orig_bytes INTEGER, zstd_bytes INTEGER)`
    - `events(id INTEGER PK, ts_ms INTEGER, kind INTEGER, detail TEXT)` (optional audit)
  - Blob naming:
    - `blobs/<hash_u64_hex>.zst`
  - Rationale: simplest schema that supports list/restore/diff and retention.
  - Alternatives: store blobs in SQLite (BLOB column), or content-addressed chunks.
  - Consequences:
    - Must implement crash-consistency rules: write blob file first, then insert snapshot row in a transaction; prune orphan blobs on next open.

### 5.6.2 Compression & hashing parameters
- [DEC-48] **zstd compression level**
  - Decision: default zstd level = 3 for TimelineStore blobs.
  - Rationale: good speed/ratio tradeoff for frequent snapshots.
  - Alternatives: level 1 (faster), level 5+ (smaller, slower).
  - Consequences: performance budget in Ch1 snapshot p95 ≤ 500ms assumes level 3.
- [DEC-49] **xxHash width**
  - Decision: use xxHash64 for blob keys.
  - Rationale: fast non-crypto hash; collision risk acceptable for local history with additional safety checks (size + optional content verify on collision).
  - Alternatives: xxHash32 (higher risk), BLAKE3 (crypto-strong but heavier).
  - Consequences: on “hash match but size mismatch”, treat as collision and fall back to “store new blob with salt suffix” and record event.

## 5.7 Protocol Buffers (LSP/DAP) & Message Memory
- [MEM-11] LSP/DAP message bodies are bounded:
  - cap incoming message size (default 8 MB) and reject larger with error to log console
  - cap debug console output ring buffer per session (default 2 MB), oldest drop
- [DEC-50] **Protocol message size caps**
  - Decision: enforce hard caps for protocol payloads and UI buffers.
  - Rationale: prevents pathological adapters/servers from exhausting memory.
  - Alternatives: unlimited, or adaptive.
  - Consequences: add tests for cap behavior under synthetic large messages ([TEST-33], [TEST-34]).

## 5.8 Allocators & Fragmentation
inherit from /blueprint/blueprint_v0.1_ch5.md

### 5.8.1 IDE allocator notes
- [MEM-12] TextBuffer `add` buffer uses growth policy with reserved capacity to reduce realloc churn during typing bursts.
- [MEM-13] Use per-service scratch arenas (or `pmr::monotonic_buffer_resource`) within a single job (parse/search/snapshot) to reduce transient heap fragmentation; scratch arenas must not outlive the job.

## 5.9 Performance Harness Hooks (IDE)
inherit from /blueprint/blueprint_v0.1_ch5.md

### 5.9.1 v0.2 IDE benchmarks (mapped to Ch1 budgets)
- [TEST-35] Editor typing micro-bench: apply 10k small inserts + random deletes; assert p95 under [DEC-34] budgets on CI reference machine.
- [TEST-36] Snapshot micro-bench: create 50 snapshots of 1MB text; verify p95 ≤ 500ms and dedup hits > 0 after repeated identical content.

> Note: [TEST-35] and [TEST-36] are additional (not part of the earlier reserved tests list). They are required to enforce [DEC-34] budgets.

## 5.10 Memory & Data Correctness Tests
inherit from /blueprint/blueprint_v0.1_ch5.md

### 5.10.1 v0.2 IDE additions
- [TEST-28] TextBuffer property tests include UTF-8 multibyte coverage and column selection invariants ([DEC-44]).
- [TEST-30] ParseService golden tests validate span ordering and stale revision discard.
- [TEST-31] TimelineStore tests validate:
  - schema invariants
  - crash-consistency (simulate orphan blob)
  - retention enforcement (15 scripts / 3 others)
  - exclusion patterns applied
- [TEST-29] SearchService tests validate streaming queue cap ([DEC-46]).
- [TEST-33] DAP tests validate message size caps ([DEC-50]) and ring buffer behavior.
- [TEST-34] LSP tests validate message size caps ([DEC-50]).

