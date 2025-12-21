# Appendix G — Distribution Packaging and Export Format (ARCANEE v0.1)

## G.1 Scope

This appendix defines the **distribution format** for ARCANEE cartridges and the rules for exporting from Workbench and running in Player Mode. It specifies:

* archive format requirements and canonical layout
* versioning and compatibility metadata
* optional integrity mechanisms (hash manifests)
* export rules (what is included/excluded)
* Player Mode policies (permissions, overrides, and security)
* deterministic mounting and path resolution requirements

This appendix is normative for any implementation that supports archive/distribution cartridges.

---

## G.2 Archive Format and File Extension (Normative)

### G.2.1 Supported Archive Formats

ARCANEE v0.1 MUST support at least one archive container format for distribution:

* **ZIP** (recommended baseline)

The runtime MAY support additional formats (e.g., 7z), but ZIP support is sufficient for v0.1.

### G.2.2 File Extension

ARCANEE SHOULD use a dedicated extension for distribution cartridges:

* Recommended: `.arc` (ARCANEE cartridge) which is internally a ZIP archive.

If `.zip` is used directly, Workbench and Player Mode must still treat it as a cartridge if it contains a valid manifest.

### G.2.3 Deterministic Mounting

When mounting an archive, the VFS must behave identically to directory cartridges:

* `cart:/` is read-only
* path normalization rules apply
* canonical source names remain `cart:/...`

---

## G.3 Archive Layout (Normative)

### G.3.1 Root Requirements

The archive root MUST contain:

* `cartridge.toml` (or `cartridge.json` if supported by the build)
* the entry script referenced by the manifest `entry`

### G.3.2 Internal Paths

All internal paths in the archive MUST:

* use `/` separators
* not contain `..`
* be normalized and treated case-sensitively at the VFS layer (to avoid cross-platform ambiguity)

If the archive contains invalid paths, the runtime MUST reject it.

---

## G.4 Export Behavior from Workbench (Normative)

### G.4.1 Included Content

Export MUST include:

* manifest file
* all source scripts under `cart:/`
* all assets referenced under `cart:/` (by inclusion policy)

v0.1 export policy:

* default export includes **all files in the project directory** except exclusions (§G.4.2).
* Workbench MAY offer an “include list” later, but v0.1 requires deterministic export.

### G.4.2 Exclusions (Normative)

Export MUST exclude:

* `save:/` content
* `temp:/` content
* Workbench-specific editor state files (e.g., `.arcanee/` workspace metadata) unless explicitly configured

Recommended exclusion patterns:

* `.arcanee/`
* `build/`
* `out/`
* `*.tmp`
* OS junk files (`.DS_Store`, `Thumbs.db`)

Workbench MUST document exclusions and ensure export is deterministic.

### G.4.3 Deterministic Archive Creation

To ensure reproducible builds, Workbench SHOULD:

* store ZIP entries in lexicographical order
* normalize timestamps (optional in v0.1 but recommended)
* use consistent compression settings

If timestamps are not normalized, the runtime behavior must still be unaffected.

---

## G.5 Manifest Constraints for Distribution (Normative)

### G.5.1 Required Fields

Distribution cartridges MUST include the manifest fields defined in Chapter 3, including:

* `id`, `title`, `version`, `api_version`, `entry`, `display`, `permissions`

### G.5.2 Compatibility Enforcement

Player Mode MUST reject cartridges if:

* `api_version` is newer than the runtime supports
* `engine_min` (if present) is higher than the runtime version

Workbench may provide a “compatibility warning” rather than hard reject during development.

---

## G.6 Optional Integrity and Signing (v0.1 Optional)

### G.6.1 Hash Manifest (Recommended)

ARCANEE v0.1 MAY include an optional `integrity.json` at archive root containing:

* a list of file paths and their hashes (e.g., SHA-256)
* the manifest hash

If present:

* Player Mode SHOULD validate hashes before running
* Workbench SHOULD regenerate this file at export time

### G.6.2 Signing (Out of Scope for v0.1)

Digital signatures and certificate chains are deferred to v0.2+.

---

## G.7 Player Mode Policies (Normative)

### G.7.1 Default Restrictions

In Player Mode:

* Dev Mode features MUST be disabled by default:

  * no debug adapter
  * no stepping/breakpoints UI
  * no privileged `dev.*` functions
* Workbench editor UI is not available (or is minimal “player UI”)

### G.7.2 Permission Enforcement

Manifest permissions MUST be enforced strictly in Player Mode:

* `save_storage=false` prevents all writes to `save:/`
* `audio=false` disables `audio.*`
* `net=false` disables networking (v0.1: no net subsystem required)
* `native=false` disables host-native extension calls

Player Mode MUST NOT provide UI overrides that violate manifest permissions.

### G.7.3 Display Override Policy

If `display.allow_user_override=false`:

* Player Mode MUST respect `aspect`, `preset`, and `scaling` exactly, unless user policy requires a safer clamp (e.g., hardware limits).
* Any forced clamp MUST be documented and visible in a diagnostics screen (if present).

If `allow_user_override=true`, Player Mode MAY allow:

* changing scaling mode (fit/integer/fill/stretch)
* selecting a different preset
  provided these changes do not violate performance policy.

### G.7.4 Storage Locations

Player Mode MUST store saves in the per-cartridge `save:/` root mapped by `id`, as defined in Chapter 3. Player Mode MUST NOT store saves inside the cartridge archive.

---

## G.8 Runtime Loading and Error Handling (Normative)

### G.8.1 Archive Validation

Before executing, the runtime MUST validate:

* manifest presence and validity
* entry script existence
* absence of forbidden paths (`..`, absolute-like paths)
* optional integrity file (if implemented)

If validation fails, the runtime MUST refuse to run and produce a user-facing error message without leaking host paths.

### G.8.2 Asset Access Rules

All asset loads from scripts in Player Mode must use VFS paths and remain sandboxed. External references in glTF and other formats must not escape the cartridge namespace.

---

## G.9 Multi-Cartridge Install and Library (Optional)

Player Mode MAY implement an installed cartridge library:

* scanning a directory of `.arc` files
* presenting metadata (title, version, icon) from manifest

If implemented, scanning MUST be deterministic and not execute code while indexing.

---

## G.10 Compliance Checklist (Appendix G)

An implementation is compliant with Appendix G if it:

1. Supports running distribution cartridges from a ZIP-based archive format with deterministic VFS mounting.
2. Enforces archive layout rules and rejects invalid paths.
3. Exports cartridges deterministically from Workbench, excluding `save:/` and `temp:/`.
4. Enforces manifest permissions and disallows Dev Mode features in Player Mode by default.
5. Validates compatibility and fails gracefully with safe error messages.
---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
