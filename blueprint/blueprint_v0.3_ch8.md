# Blueprint v0.3 — Chapter 8: Tooling, IDE Features, Observability (Config/Theming/Keymap/Tree-sitter)

## [ARCH-08] Chapter Intent
This chapter specifies the IDE-facing feature work for v0.3: config editing, menu/explorer integration, Problems diagnostics, Tree-sitter TOML + Squirrel highlighting, unified theming, keybindings, and font handling.

## Tooling Baseline
inherit from blueprint_v0.2_ch8.md

---

## [REQ-95] Configuration Menu + Config Explorer Root
### UX
Add a top-level menu **Configuration**:
- **Configuration → Open Config Folder**
  - Ensures `./config/` exists (create directory if missing).
  - Switches file explorer root mode to **Config Root** (`./config`) without changing the active project root. [DEC-66]
  - Opens `config/color-schemes.toml` by default if no file is open (nice-to-have).

- **Configuration → New Config File** (optional)
  - Creates missing:
    - `config/editor.toml`
    - `config/gui.toml`
    - `config/keys.toml`
  - Populates with commented defaults.
  - If `config/color-schemes.toml` is missing, regenerate it from built-in defaults (but CI also enforces presence in source control). [REQ-90], [BUILD-19]

### Implementation notes
- Implement explorer “root mode” enum:
  - `ExplorerRootMode::Project`
  - `ExplorerRootMode::Config` [DEC-66]
- `Open Config Folder` sets root mode and refreshes tree to show `./config/` contents.

### Test
- [TEST-43] verifies root mode switch and directory creation behavior.

---

## [REQ-90] Config File Set and Shapes (v0.3)
### File list
- `config/color-schemes.toml` (required in distribution; user-editable)
- `config/editor.toml` (optional)
- `config/gui.toml` (optional)
- `config/keys.toml` (optional)

### Required shipped file: `config/color-schemes.toml`
- Contains built-in reference schemes in TOML form; users can edit/extend.
- Also supported: built-in hardcoded defaults embedded in binary to ensure recovery if file is corrupted or missing at runtime (diagnostic emitted). [REQ-90], [REQ-91]

### Suggested shapes (canonical keys)
#### `editor.toml`
```toml
[color_scheme]
active = "ayu-mirage"

[font]
family = "JetBrains Mono"
size_px = 14
weight = "Regular"   # Light|Regular|Medium|SemiBold|Bold
style  = "Normal"    # Normal|Italic
line_height = 1.1
ligatures = true

[indent]
type = "spaces"      # spaces|tabs
width = 2
tab_width = 2
detect = false

[view]
line_numbers = true
highlight_current_line = true

[cursor]
blink = true
blink_rate_ms = 530

[whitespace]
render = "boundary"  # none|boundary|all

[eol]
render = false
```

#### `gui.toml`

```toml
[ui_font]
family = "Inter"
size_px = 14
weight = "Regular"
line_height = 1.0

[metrics]
title_bar_height_px = 0
menu_bar_height_px = 0
dock_padding_px = 6
window_rounding_px = 6
scrollbar_size_px = 14

[behavior]
remember_layout = true
show_fps = false
dpi_scale = 0   # 0 => auto
```

#### `keys.toml`

```toml
[keys]
"file.save" = ["primary+S"]
"file.open" = ["primary+O"]
"command_palette.open" = ["primary+Shift+P"]
"editor.copy"  = ["primary+C"]
"editor.paste" = ["primary+V"]
"editor.find"  = ["primary+F"]
```

#### `color-schemes.toml`

```toml
[[scheme]]
id = "ayu-mirage"
name = "Ayu Mirage"
variant = "dark"

[scheme.editor]
background = "#1F2430"
foreground = "#CBCCC6"
caret      = "#FFCC66"
selection  = "#33415E"
line_highlight = "#232A3A"
gutter_background = "#1F2430"
gutter_foreground = "#707A8C"

[scheme.syntax]
comment  = "#707A8C"
string   = "#BAE67E"
number   = "#D4BFFF"
keyword  = "#FFAD66"
type     = "#73D0FF"
function = "#FFD173"
variable = "#CBCCC6"
operator = "#F29E74"
error    = "#FF6666"

# Optional overrides; otherwise derived. [DEC-63]
[scheme.gui]
window_bg = "#1F2430"
text      = "#CBCCC6"
accent    = "#FFCC66"
```

---

## [REQ-91][REQ-97][REQ-98] Parsing, Validation, Diagnostics, Hot-Apply

### Parser

* Use `toml++` for parsing.
* Enforce UTF-8 IO boundary:

  * read bytes
  * reject invalid UTF-8 sequences (emit error diagnostic) [REQ-98]
  * then parse TOML
* Reject TOML parse errors non-fatally:

  * do not apply; keep last-known-good snapshot; emit error diagnostic. [REQ-91]

### Validation rules

* Unknown keys: warning diagnostic “Unknown configuration key”; ignored. [REQ-97]
* Invalid values: warning diagnostic and field fallback only. [REQ-91]
* Missing referenced scheme id: fallback to default (e.g., `ayu-mirage`), warning diagnostic. [REQ-91]
* Font path keys (any key containing `path`, `file`, or values that look like filesystem paths) are ignored + warning diagnostic. [REQ-92]

### Hot-apply

* Triggered only by IDE save of `config/*.toml`. [REQ-91]
* Debounce applies (150–250ms).
* Apply order is fixed (see Ch3/Ch6) and must be main-thread. [ARCH-12]

---

## [REQ-93] Unified Theming (Editor + Syntax + ImGui)

### Token taxonomy (stable IDE-owned)

Stable scheme syntax keys (v0.3):

* `comment`, `string`, `number`, `keyword`, `type`, `function`, `variable`, `operator`, `error`

### Tree-sitter capture mapping (capture → token → scheme.syntax key)

* `comment` captures (`@comment`) → `SyntaxToken::Comment` → `comment`
* `string` (`@string`) → `String` → `string`
* `number` (`@number`, `@float`) → `Number` → `number`
* `keyword` (`@keyword`) → `Keyword` → `keyword`
* `type` (`@type`, `@type.builtin`) → `Type` → `type`
* `function` (`@function`, `@function.builtin`) → `Function` → `function`
* `variable` (`@variable`, `@constant`) → `Variable` → `variable`
* `operator` (`@operator`) → `Operator` → `operator`
* invalid/error nodes → `Error` → `error`

Unknown capture:

* use `scheme.editor.foreground` (optionally dimmed). [REQ-94]

### ImGui theming

* MVP uses derived deterministic mapping from scheme editor colors. [DEC-69]
* Optional `[scheme.gui]` overrides may override derived values for key roles (window_bg, panel_bg, text, accent, etc.). [DEC-63]

---

## [REQ-94] Tree-sitter Language Support: `.nut` + `.toml`

### `.nut` (Squirrel)

* Confirmed `.nut` extension for Squirrel in project. (user confirmed)
* Use existing tree-sitter-squirrel integration.
* Update highlight pipeline to use ThemeSystem token colors rather than hardcoded colors.

### `.toml` (TOML)

* Add tree-sitter TOML grammar and highlight queries.
* Add `assets/ide/treesitter/toml/queries/highlights.scm` and load it similarly to squirrel.
* Ensure TOML buffers show Tree-sitter-based highlighting immediately.

### Diagnostics mapping for TOML configs

* TOML parse errors and validation diagnostics are routed to Problems window with file and best-effort line/column.
* Note: Tree-sitter TOML is for highlighting, not authoritative parsing for config (toml++ is). [DEC-64]

---

## [REQ-96] Keybindings (Configurable)

### Action IDs

* v0.3 defines the stable action registry as a superset of existing v0.2 actions.
* Freeze initial list in code (string IDs) and document it in v0.3 Walkthrough and Checklist; new actions require SemVer/minor + documentation. [VER-18]

### Chord syntax

* Accept:

  * `primary+S`, `primary+Shift+P`
  * `Ctrl+S`, `Ctrl+Shift+P`, `Alt+X`, `Cmd+S` (platform-specific accepted but normalized)
* Normalize:

  * `primary` → Ctrl (Win/Linux), Cmd (macOS)
* Conflicts:

  * last definition wins
  * emit warning diagnostics including overwritten action/chord.

---

## [REQ-92] Fonts (System Only) + Safe Application

### Editor font and UI font

* Resolve font by family name using platform backends. [DEC-65]
* Weight/style best-match; if not found, pick closest and warn.
* No path loading; any path-like key is ignored with a warning diagnostic. [REQ-92]

### Safe rebuild

* On apply, attempt font rebuild; if failure:

  * keep previous valid font
  * emit error diagnostic and optional log. [REQ-92], [REQ-97]

---

## Observability (Config)

* Every config reload produces structured log entries mirroring Problems diagnostics (optional but recommended). [REQ-02]
* Avoid log spam by logging only on apply completion and only for non-empty diagnostics set.

---

## Tests (v0.3, tooling-focused)

* [TEST-39] parse/validate/fallback + Problems diagnostic emission
* [TEST-43] Configuration menu + config explorer root
* [TEST-45] hot-apply concurrency determinism (seq gating)
* [TEST-40] theme mapping + token palette derived correctness

