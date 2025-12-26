<!-- _blueprint/project/ch2.md -->

# Blueprint v0.3 — Chapter 2: System Architecture (C4), Platform Matrix, Repo Map, Packaging

## [ARCH-01] C4 Overview
inherit from blueprint_v0.2_ch2.md

## [ARCH-02] C4 — Container Diagram (v0.3 delta)
The following containers/components are added or clarified by [CR-0044]:

- **Config System** (`ConfigSystem`): loads `./config/*.toml`, validates, emits Problems diagnostics, produces an immutable `ConfigSnapshot`, and schedules apply on the main thread with debouncing. [ARCH-12], [API-17]
- **Theme System** (`ThemeSystem`): owns scheme registry and produces:
  - editor UI palette
  - syntax token palette
  - ImGui style colors (derived mapping MVP; optional overrides). [REQ-93], [DEC-63]
- **Keymap System** (`Keymap`): parses `config/keys.toml` and constructs action→chords and chord→action maps with deterministic last-wins resolution and `primary` abstraction. [REQ-96]
- **Font Locator** (`FontLocator_*`): platform backends for system-font selection (DirectWrite/CoreText/fontconfig) and safe rebuild semantics with last-known-good retention. [REQ-92], [DEC-65]
- **Parse/Highlight Service** (`ParseService`): multi-language routing for `.nut` and `.toml` Tree-sitter grammars and stable capture→token mapping. [REQ-94]

```mermaid
flowchart LR
  UI[UIShell / ImGui Dock UI] -->|apply| THEME[ThemeSystem]
  UI -->|apply| KEYMAP[Keymap]
  UI -->|apply| FONTS[FontLocator_*]
  EDITOR[Editor Core] -->|palette| THEME
  EDITOR -->|key events| KEYMAP
  EDITOR --> PARSE[ParseService / Tree-sitter]
  PARSE -->|tokens| THEME

  DOCS[DocumentSystem] -->|OnSaved(config/*.toml)| CFG[ConfigSystem]
  CFG -->|snapshot| UI
  CFG -->|diagnostics| PROB[Problems Window]
```

## [ARCH-03] Data Flow (Hot-Apply)

* Save of `config/*.toml` **inside IDE** triggers `DocumentSystem::OnSaved(path)` event. [REQ-91]
* `ConfigSystem`:

  1. debounce (coalesce burst saves)
  2. parse/validate off-thread
  3. enqueue apply to UI/main thread
  4. apply per-subsystem with last-known-good rollback semantics (fonts). [ARCH-12], [API-17], [REQ-92]

## [ARCH-04] Platform / OS Matrix

inherit from blueprint_v0.2_ch2.md

## [ARCH-05] Repo Map (v0.3 delta)

Add/modify the following repo paths:

* **New (repo-root)**

  * `/config/`

    * `color-schemes.toml` (shipped default; user-modifiable) [REQ-90]
    * `editor.toml` (optional)
    * `gui.toml` (optional)
    * `keys.toml` (optional)
* **New/Updated assets**

  * `/assets/ide/treesitter/toml/queries/highlights.scm` [REQ-94]
* **New/Updated source (expected)**

  * `/src/ide/config/ConfigSystem.{h,cpp}` [ARCH-12]
  * `/src/ide/config/ConfigSchema.{h,cpp}`
  * `/src/ide/config/ThemeSystem.{h,cpp}` [REQ-93]
  * `/src/ide/config/Keymap.{h,cpp}` [REQ-96]
  * `/src/platform/FontLocator_{win,mac,linux}.{h,cpp}` [REQ-92], [DEC-65]
  * Updates to:

    * `/src/ide/DocumentSystem.*` (save event hook) [REQ-91]
    * `/src/ide/UIShell.*` (Configuration menu; apply snapshot; Problems) [REQ-95], [REQ-97]
    * `/src/ide/ParseService.*` (TOML language support; stable mapping) [REQ-94]

## [ARCH-06] Packaging / Distribution

* **Config root:** primary `./config/` relative to process working directory. [DEC-67]
* **Compatibility fallback:** if `./config` is not found, attempt `exe_dir/config` and emit a warning diagnostic indicating fallback path was used. [DEC-67]
* **Shipped default:** `config/color-schemes.toml` must exist in packaged distribution; it is user-editable and treated as “user-defined + built-in reference” source. [REQ-90]

## [DEC-66] Explorer Root Modes

The IDE file explorer supports a “root mode”:

* **Project Root** (existing)
* **Config Root** (`./config`) used by “Configuration → Open Config Folder” without changing project root. [REQ-95]

## [TEST-43] Packaging + Explorer Root Smoke

* Launch from repo root: `Configuration → Open Config Folder` opens `./config`.
* Launch from non-repo cwd: app uses fallback `exe_dir/config` (if present) and emits a warning diagnostic.
* Opening config root does not change project root; returning to project root preserves open project state.


