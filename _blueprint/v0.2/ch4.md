<!-- _blueprint/v0.2/ch4.md -->

# Blueprint v0.2 — Chapter 4: Interfaces, API/ABI, Errors

## 4.1 Public API / ABI Policy
inherit from /blueprint/blueprint_v0.1_ch4.md

## 4.2 Error Handling Model
inherit from /blueprint/blueprint_v0.1_ch4.md

### 4.2.1 IDE-specific error rules (additive)
- [REQ-76] UI thread must never block on error recovery; all IDE-service failures degrade gracefully (disable feature, show notification/log) and editing must remain available.
- [DEC-41] **IDE service failure containment**
  - Decision: each IDE service boundary returns either `Status` (non-throwing) or `Expected<T, Status>`; exceptions are allowed only for programmer errors (contract violations) and must not cross service boundaries.
  - Rationale: IDE subsystems add many I/O + protocol failure modes; making error returns explicit keeps UI responsive and makes tests deterministic.
  - Alternatives:
    - (A) Exceptions everywhere.
    - (B) `std::error_code` only.
  - Consequences:
    - Define a single `Status` type in `src/core/Status.h` with stable codes and formatted message.
    - Each service defines a small code enum and maps to `Status::Code` values.

## 4.3 Threading & Ownership Conventions (IDE)
- [REQ-76] All service APIs below declare:
  - ownership rules (who owns what)
  - threading (which thread calls which method)
  - cancellation behavior
  - error returns
  - tests and CI gates
- [DEC-38] The UIShell-owned **main-thread apply queue** is the sole mutation ingress for UI-visible models.

## 4.4 IDE APIs (v0.2 Additions)

> Location policy: headers under `src/ide/**` are internal. Keep **declarations in headers** and implementations in `.cpp` (matches repo style and keeps rebuild times reasonable). If later we introduce `/include`, we can promote stable parts via a CR.

### 4.4.1 [API-07] UIShell
**Purpose:** Dockspace, panes, command palette, focus/shortcuts, and the main-thread apply queue.

**Ownership**
- Owns pane view-models, command registry, and `MainThreadQueue`.
- Does not own DocumentSystem/ProjectSystem instances; holds references.

**Threading**
- Must run on UI thread only.
- Receives immutable result packets from services via `MainThreadQueue::Push()` (thread-safe).

**Errors**
- UI actions never throw; log and surface in IDE Log Console on failures.

**Header sketch**
```cpp
// src/ide/UIShell.h
#pragma once
#include <functional>
#include <string_view>
#include "core/Status.h"

namespace ide {

struct CommandId { uint32_t v{}; };

struct CommandContext {
  // active doc, selection, etc. (read-only snapshot)
};

using CommandFn = std::function<Status(const CommandContext&)>;

class MainThreadQueue {
public:
  using Job = std::function<void()>;
  void Push(Job job) noexcept;           // thread-safe
  void DrainAll() noexcept;              // UI-thread only
};

class UIShell {
public:
  UIShell(MainThreadQueue& q) noexcept;
  Status RegisterCommand(std::string_view name, CommandFn fn, CommandId* out) noexcept;
  void RenderFrame() noexcept;           // UI-thread only
  MainThreadQueue& Queue() noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-27] verifies `RenderFrame()` creates dockspace + required panes; layout persistence roundtrip using a temp project.

**Build/CI**

* Included in existing CMake targets; test runs in CI preset (see Ch7).

---

### 4.4.2 [API-08] ProjectSystem

**Purpose:** Workspace root (`samples/<project>`), file tree model, filters, ignore rules, recents.

**Ownership**

* Owns `ProjectModel` (immutable snapshots) + optional watcher (platform wrapper).
* Supplies file tree snapshots to UI.

**Threading**

* UI thread reads snapshot handles.
* Background indexing/watching posts changes to `MainThreadQueue`.

**Errors**

* IO failures return `Status`; UI shows banner + logs.

**Header sketch**

```cpp
// src/ide/ProjectSystem.h
#pragma once
#include <filesystem>
#include <vector>
#include "core/Status.h"

namespace ide {

struct FileNode {
  std::filesystem::path rel;
  bool is_dir{};
  std::vector<uint32_t> children; // indices into flat array
};

struct ProjectSnapshot {
  std::filesystem::path root;
  std::vector<FileNode> nodes; // node[0] is root
};

class ProjectSystem {
public:
  Status OpenWorkspace(std::filesystem::path root) noexcept; // expects samples/<project>
  const ProjectSnapshot& Snapshot() const noexcept;         // UI-thread access
  Status CreateFile(std::filesystem::path rel) noexcept;
  Status Rename(std::filesystem::path rel_from, std::filesystem::path rel_to) noexcept;
  Status Delete(std::filesystem::path rel) noexcept;
};

} // namespace ide
```

**Tests**

* Covered by [TEST-27] (pane behavior) and by filesystem model unit tests (added under [TEST-27] suite).

---

### 4.4.3 [API-09] DocumentSystem

**Purpose:** Open buffers, encoding/EOL normalization, autosave/crash recovery hooks, cursor state, open tabs.

**Ownership**

* Owns `Document` objects (each owns TextBuffer + per-doc service handles).
* Owns session persistence model (open files + cursors) but not the ImGui layout file itself (Ch2).

**Threading**

* UI thread invokes document edits/commands.
* Background save/autosave allowed but must not mutate buffer; it serializes a snapshot captured on UI thread.

**Errors**

* Load/save return `Status`; failed save must keep buffer dirty and show message.

**Header sketch**

```cpp
// src/ide/DocumentSystem.h
#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include "core/Status.h"

namespace ide {

class Document;

struct CursorState { uint32_t line{}, col{}; };

class DocumentSystem {
public:
  Status Open(std::filesystem::path abs_path, Document** out) noexcept;
  Status New(std::filesystem::path abs_path, Document** out) noexcept;
  Status Save(Document& doc) noexcept;
  Status Close(Document& doc) noexcept;

  void TickAutosave() noexcept; // UI thread calls periodically
  const std::vector<std::unique_ptr<Document>>& OpenDocuments() const noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-27] session restore includes open docs + cursors.
* [TEST-28] buffer invariants validated through Document commands.

---

### 4.4.4 [API-10] TextBuffer

**Purpose:** Core editing model: piece-table/rope storage, multi-cursor ops, column selection, undo tree, find/replace in file.

**Ownership**

* Document owns TextBuffer.
* TextBuffer owns its own undo tree; no external references to internal nodes.

**Threading**

* UI thread only for mutations.
* Read-only snapshots may be created for background services (Parse/Search/Timeline) and must be immutable.

**Errors**

* Editing operations are infallible for valid inputs; invalid ranges return `Status::InvalidArgument`.

**Header sketch**

```cpp
// src/ide/TextBuffer.h
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/Status.h"

namespace ide {

struct TextPos { uint32_t line{}, col{}; };
struct TextRange { TextPos a{}, b{}; };

struct Cursor {
  TextPos caret{};
  TextRange selection{}; // empty == caret only
};

enum class EditOpKind { Insert, Delete, Replace };

struct EditOp {
  EditOpKind kind{};
  TextRange range{};
  std::string text; // UTF-8
};

class TextSnapshot {
public:
  std::string ToString() const; // for tests/diff; avoid on hot path
  // plus iterators/rope readers (impl-defined)
};

class TextBuffer {
public:
  Status Apply(const EditOp& op) noexcept;
  Status ApplyBatch(const std::vector<EditOp>& ops) noexcept;

  // multi-cursor
  std::vector<Cursor>& Cursors() noexcept;
  const std::vector<Cursor>& Cursors() const noexcept;

  // undo tree
  bool CanUndo() const noexcept;
  bool CanRedo() const noexcept;
  Status Undo() noexcept;
  Status Redo() noexcept;

  TextSnapshot Snapshot() const noexcept; // immutable snapshot for bg services
};

} // namespace ide
```

**Tests**

* [TEST-28] property-based randomized edit streams; asserts:

  * cursor ordering rules
  * undo/redo returns to identical text snapshots
  * column selection invariants (rectangular edits)
  * deterministic behavior given same command stream

---

### 4.4.5 [API-11] ParseService (tree-sitter)

**Purpose:** Incremental parse per document revision, query execution → highlight spans, fold ranges, outline/symbols.

**Ownership**

* Document owns ParseService handle; ParseService owns tree-sitter parser and per-doc CST.
* Query files are loaded from repo assets (Ch2).

**Threading**

* Parse computation runs on worker thread(s).
* Only publishes immutable `ParseResult` packets to the main-thread queue.

**Revisioning**

* [REQ-69] every request includes a `doc_revision`; UIShell discards stale results.

**Header sketch**

```cpp
// src/ide/ParseService.h
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "core/Status.h"
#include "ide/TextBuffer.h"

namespace ide {

using DocRevision = uint64_t;

struct HighlightSpan { uint32_t byte_a{}, byte_b{}; uint32_t style_id{}; };
struct FoldRange { uint32_t line_a{}, line_b{}; };
struct OutlineItem { std::string name; uint32_t line{}; uint32_t kind{}; };

struct ParseResult {
  DocRevision revision{};
  std::vector<HighlightSpan> highlights;
  std::vector<FoldRange> folds;
  std::vector<OutlineItem> outline;
};

class CancelToken;

class ParseService {
public:
  Status InitSquirrel(std::string queries_root_dir) noexcept;
  Status RequestParse(DocRevision rev, TextSnapshot snap, CancelToken& cancel) noexcept;
  // result delivery is via callback set at init:
  using PublishFn = void(*)(ParseResult&&);
  void SetPublisher(PublishFn fn) noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-30] golden fixtures for Squirrel + incremental edits; ensures:

  * query loading works from `assets/ide/treesitter/...`
  * highlights/folds/outline match expected outputs for sample inputs
  * stale results are dropped (revisioning)

---

### 4.4.6 [API-12] SearchService (PCRE2)

**Purpose:** workspace search/replace with streaming results, cancel, replace preview.

**Ownership**

* UIShell owns current search sessions.
* SearchService owns worker jobs, compiled regex cache per session.

**Threading**

* Runs on workers; streams results through main-thread queue.

**Header sketch**

```cpp
// src/ide/SearchService.h
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "core/Status.h"

namespace ide {

using SearchSessionId = uint64_t;

struct SearchQuery {
  std::string pattern;   // PCRE2
  bool case_sensitive{};
  bool regex{};
};

struct SearchHit {
  std::filesystem::path file;
  uint32_t line{};
  uint32_t col{};
  std::string line_text;
};

struct SearchEvent {
  SearchSessionId sid{};
  enum class Kind { Hit, Progress, Done, Canceled, Error } kind{};
  SearchHit hit;
  uint32_t files_scanned{};
  Status error;
};

class CancelToken;

class SearchService {
public:
  using PublishFn = void(*)(SearchEvent&&);
  void SetPublisher(PublishFn fn) noexcept;

  Status StartWorkspaceSearch(
      SearchSessionId sid,
      std::filesystem::path workspace_root,
      SearchQuery q,
      CancelToken& cancel) noexcept;

  Status RequestCancel(SearchSessionId sid) noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-29] correctness + cancel latency; replace preview tests for representative regex patterns.

---

### 4.4.7 [API-13] TaskRunner (+ problem matchers)

**Purpose:** parse `tasks.toml`, run tasks, capture output, emit problem matches.

**Ownership**

* Owns task definitions loaded per project.
* Owns process handles while running and output buffers.

**Threading**

* Process I/O read on background threads; events streamed to UI.
* UI never blocks waiting for process exit.

**Minimal schema (v0.2)**

* [DEC-32] tasks.toml schema is intentionally small:

```toml
# samples/<project>/.arcanee/tasks.toml
[[task]]
name = "build"
cmd  = "cmake"
args = ["--build", "build"]
cwd  = "."              # optional, relative to workspace root
problem = "msvc"        # optional: selects builtin matcher
```

**Header sketch**

```cpp
// src/ide/TaskRunner.h
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "core/Status.h"

namespace ide {

using TaskId = uint64_t;

struct TaskDef {
  std::string name;
  std::string cmd;
  std::vector<std::string> args;
  std::filesystem::path cwd_rel{"."};
  std::string problem; // builtin matcher key (optional)
};

struct TaskEvent {
  TaskId id{};
  enum class Kind { Stdout, Stderr, Exit, Problem } kind{};
  std::string text;
  int exit_code{};
  // For Kind::Problem:
  std::filesystem::path file;
  uint32_t line{}, col{};
  std::string message;
};

class CancelToken;

class TaskRunner {
public:
  using PublishFn = void(*)(TaskEvent&&);
  void SetPublisher(PublishFn fn) noexcept;

  Status LoadTasks(std::filesystem::path tasks_toml) noexcept;
  const std::vector<TaskDef>& Tasks() const noexcept;

  Status Run(TaskId id, const TaskDef& task, std::filesystem::path workspace_root, CancelToken& cancel) noexcept;
  Status Cancel(TaskId id) noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-32] runs a small test tool, captures streams, validates problem matcher emits clickable file:line.

---

### 4.4.8 [API-14] TimelineStore (SQLite + zstd + xxHash)

**Purpose:** snapshot scheduling + storage + restore + diff, with retention policy.

**Ownership**

* Owns SQLite DB connection + blob store directory under `.arcanee/timeline/`.
* Owns retention pruning logic per [REQ-84] and exclusions per [REQ-85].

**Threading**

* Snapshot writes in background; metadata apply via main-thread queue.
* Restore is a UI command that schedules background reads then applies new doc revision on main thread.

**Retention policy**

* [REQ-84] keep 15 snapshots for “script” files, 3 for others (classification defined in Ch8).

**Header sketch**

```cpp
// src/ide/TimelineStore.h
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "core/Status.h"

namespace ide {

using SnapshotId = uint64_t;
using DocRevision = uint64_t;

struct SnapshotMeta {
  SnapshotId id{};
  std::string label;          // optional
  uint64_t unix_ms{};
  std::filesystem::path file_rel;
  bool is_script{};
  DocRevision revision{};
};

struct DiffHunk { uint32_t line_a{}, line_b{}; std::string text; };

class CancelToken;

class TimelineStore {
public:
  Status Open(std::filesystem::path timeline_dir) noexcept; // .arcanee/timeline
  Status ScheduleAutoSnapshot(std::filesystem::path file_rel, bool is_script, DocRevision rev, std::string content_utf8, CancelToken& cancel) noexcept;
  Status CreateCheckpoint(std::filesystem::path file_rel, bool is_script, DocRevision rev, std::string label, std::string content_utf8, CancelToken& cancel) noexcept;

  Status List(std::filesystem::path file_rel, std::vector<SnapshotMeta>* out) noexcept;
  Status LoadContent(SnapshotId id, std::string* out_utf8, CancelToken& cancel) noexcept;
  Status Diff(SnapshotId a, SnapshotId b, std::vector<DiffHunk>* out) noexcept;

  Status PruneToPolicy() noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-31] snapshot/restore/diff + dedup hit sanity + retention enforcement + exclusions.

---

### 4.4.9 [API-15] LspClient (Squirrel-only)

**Purpose:** JSON-RPC LSP client for Squirrel semantics (diagnostics + navigation hooks baseline).

**Threading**

* Runs on background I/O thread.
* Publishes diagnostics + responses via main-thread queue.

**Transport**

* [DEC-42] **LSP transport strategy**

  * Decision: LspClient spawns a **Squirrel LSP server process** (shipped in-repo as a tool target) and communicates over stdio using JSON-RPC 2.0 with `Content-Length` framing.
  * Rationale: keeps boundaries clean, matches common LSP practice, and is testable/deterministic.
  * Alternatives:

    * (A) embed LSP server in-process.
    * (B) require users to install an external LSP server.
  * Consequences:

    * Adds a tools target (e.g., `arcanee_squirrel_lsp`) and a smoke test that launches it ([TEST-34]).
    * If tooling is disabled in a build, Workbench must degrade gracefully (tree-sitter still active).

**Header sketch**

```cpp
// src/ide/LspClient.h
#pragma once
#include <filesystem>
#include <string>
#include "core/Status.h"

namespace ide {

struct LspDiag {
  std::filesystem::path file;
  uint32_t line{}, col{};
  std::string message;
  uint32_t severity{};
};

struct LspEvent {
  enum class Kind { Diagnostics, Ready, Error } kind{};
  LspDiag diag;
  Status error;
};

class LspClient {
public:
  using PublishFn = void(*)(LspEvent&&);
  void SetPublisher(PublishFn fn) noexcept;

  Status StartSquirrel(std::filesystem::path workspace_root) noexcept;
  Status Stop() noexcept;

  // minimal hooks
  Status NotifyDidOpen(std::filesystem::path file_rel, std::string utf8) noexcept;
  Status NotifyDidChange(std::filesystem::path file_rel, std::string utf8) noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-34] layer TOML + keybindings + LSP smoke (launch + diagnostics event path).

---

### 4.4.10 [API-16] DapClient + ARCANEE DAP Adapter baseline

**Purpose:** DAP client UI and a baseline adapter for ARCANEE runtime/Squirrel.

**Transport strategy**

* [DEC-43] **In-process baseline adapter (v0.2)**

  * Decision: implement the ARCANEE “adapter” as an in-process DAP server component with an abstract `IDapTransport`:

    * `DapClient` speaks standard DAP message shapes.
    * Baseline adapter uses an in-process transport channel (queues) but preserves DAP framing semantics in tests.
  * Rationale: IDE and runtime currently live in the same executable; in-process adapter is simplest while still enforcing DAP message contracts for future external adapters.
  * Alternatives:

    * (A) separate adapter process over stdio.
    * (B) adapter process communicating with runtime over local sockets/pipes.
  * Consequences:

    * We still support external adapters later by adding stdio transport without refactoring UI/state machine.
    * [TEST-33] must validate real DAP message flow (initialize/setBreakpoints/stackTrace/variables/evaluate).

**Threading**

* DapClient I/O on background thread(s).
* Debug events applied via main-thread queue.
* Runtime debug hooks must not change non-debug behavior; only active when session running ([REQ-83]).

**Header sketch**

```cpp
// src/ide/DapClient.h
#pragma once
#include <string>
#include <vector>
#include "core/Status.h"

namespace ide {

struct DapBreakpoint { std::string file; uint32_t line{}; bool verified{}; };
struct DapStackFrame { uint32_t id{}; std::string name; std::string file; uint32_t line{}; };
struct DapVariable { std::string name, value, type; uint32_t ref{}; };

struct DapEvent {
  enum class Kind { Ready, Breakpoint, Stopped, Continued, Output, Error, Stack, Variables } kind{};
  std::string text;
  Status error;
  std::vector<DapStackFrame> stack;
  std::vector<DapVariable> vars;
};

class DapClient {
public:
  using PublishFn = void(*)(DapEvent&&);
  void SetPublisher(PublishFn fn) noexcept;

  Status StartArcaneeSession() noexcept;     // baseline adapter
  Status Stop() noexcept;

  Status SetBreakpoints(std::string file, std::vector<uint32_t> lines) noexcept;
  Status Continue() noexcept;
  Status Next() noexcept;
  Status StepIn() noexcept;
  Status StepOut() noexcept;
  Status Evaluate(std::string expr) noexcept;
  Status RequestStackTrace() noexcept;
  Status RequestVariables(uint32_t var_ref) noexcept;
};

} // namespace ide
```

**Tests**

* [TEST-33] deterministic debug scenario: run a sample cartridge/script, hit a breakpoint, validate stack/vars/evaluate + resume.

---

## 4.5 Compatibility & Deprecation (IDE)

inherit from /blueprint/blueprint_v0.1_ch4.md

