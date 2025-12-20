# Chapter 3 — Cartridge Format, VFS, Permissions, and Sandbox (ARCANEE v0.1)

## 3.1 Overview

ARCANEE executes **cartridges** as sandboxed projects. A cartridge contains Squirrel source code and assets. At runtime, cartridges may only access data through the ARCANEE **Virtual Filesystem (VFS)**, which is implemented using PhysFS and exposed to scripts via the `fs.*` API.

This chapter defines:

- Cartridge package formats and required files.
- Mount rules and VFS namespaces (`cart:/`, `save:/`, `temp:/`).
- Permission model and enforcement.
- Path normalization and security requirements.
- Save data rules, quotas, and portability constraints.
- Development vs Distribution behavior.

Normative keywords **MUST**, **MUST NOT**, **SHOULD**, **MAY** are used as in RFC-style specifications.

------

## 3.2 Cartridge Package Formats

### 3.2.1 Supported Sources

ARCANEE v0.1 MUST support loading a cartridge from:

1. **Directory cartridge (Dev source)**
   A host directory on disk representing the project layout. This is the primary mode for Workbench editing and hot reload.
2. **Archive cartridge (Distribution source)**
   A single archive file (recommended: `.zip`) containing the same layout as the directory cartridge.

ARCANEE MUST present both sources identically via `cart:/` regardless of host OS paths.

### 3.2.2 Cartridge Identity

Each cartridge MUST define a stable unique identifier `id` in its manifest. The `id` MUST remain stable across versions of the same cartridge to ensure consistent save directory mapping.

------

## 3.3 Cartridge Layout and Required Files

### 3.3.1 Required Files

A cartridge MUST contain:

- `cartridge.toml` (or `cartridge.json`; v0.1 mandates at least one supported format; TOML is recommended)
- Entry script indicated by manifest field `entry` (default `main.nut` if omitted by policy)

If any required file is missing, the runtime MUST refuse to run the cartridge and MUST report a user-visible error in Workbench.

### 3.3.2 Recommended Directory Structure

This structure is RECOMMENDED but not required:

```
/cartridge.toml
/main.nut
/src/...
/assets/
  /img/...
  /fonts/...
  /3d/...
  /music/...
/docs/...
```

### 3.3.3 Canonical Virtual Paths

Within the VFS, the following root namespaces are reserved:

- `cart:/` — cartridge content (read-only at runtime)
- `save:/` — per-cartridge persistent writable storage
- `temp:/` — per-cartridge ephemeral writable storage (cache)

Scripts MUST NOT be able to access host filesystem paths directly.

------

## 3.4 Manifest Specification (v0.1)

### 3.4.1 Manifest Fields (Normative)

The manifest MUST include at minimum:

| Field         | Type   | Required | Notes                                    |
| ------------- | ------ | -------- | ---------------------------------------- |
| `id`          | string | YES      | Stable unique ID; used for save mapping  |
| `title`       | string | YES      | User-facing                              |
| `version`     | string | YES      | Semver recommended                       |
| `api_version` | string | YES      | MUST be `"0.1"` for v0.1                 |
| `entry`       | string | YES      | Virtual path relative to `cart:/`        |
| `display`     | table  | YES      | See §3.4.2                               |
| `permissions` | table  | YES      | See §3.4.3                               |
| `caps`        | table  | NO       | Budget hints; enforced by runtime policy |

The runtime MAY extend the manifest with additional fields but MUST ignore unknown fields.

### 3.4.2 `display` (Normative)

```toml
[display]
aspect = "16:9" | "4:3" | "any"
preset = "low" | "medium" | "high" | "ultra"
scaling = "fit" | "integer_nearest" | "fill" | "stretch"
allow_user_override = true|false
```

#### 3.4.5 Complete Manifest Example (Normative)

```toml
# cartridge.toml - Complete example

id = "com.example.mygame"
title = "My Awesome Game"
version = "1.0.0"
api_version = "0.1"
entry = "main.nut"

[display]
aspect = "16:9"
preset = "medium"           # 960×540
scaling = "integer_nearest"  # Pixel-perfect
allow_user_override = true

[permissions]
save_storage = true
audio = true
net = false
native = false

[caps]
cpu_ms_per_update = 2.0
vm_memory_mb = 64
audio_channels = 32
```

Rules:

- If `aspect="any"`, the runtime MAY choose a default based on user settings; otherwise MUST respect the requested aspect.
- If `allow_user_override=false`, Workbench MUST NOT let the user persistently override the cartridge’s requested aspect/preset/scaling (temporary preview overrides MAY exist but MUST be visually indicated as overrides).

### 3.4.3 `permissions` (Normative)

```
[permissions]
save_storage = true|false
audio = true|false
net = true|false
native = true|false
```

Rules:

- `save_storage=false` MUST make all write operations to `save:/` fail (and MUST not create write directories).
- `audio=false` MUST make `audio.*` either unavailable or no-op with clear errors (implementation choice; v0.1 RECOMMENDS “API exists but fails safely” for consistent scripting).
- `net=false` MUST prevent all networking (v0.1 does not require a network subsystem).
- `native=false` MUST prevent any host-native extension calls from script (v0.1 does not define a native plugin interface).

### 3.4.4 `caps` (Advisory)

`caps` provides hints; the runtime is authoritative. Example:

```
[caps]
cpu_ms_per_update = 2.0
vm_memory_mb = 64
max_draw_calls = 20000
max_canvas_pixels = 16777216
audio_channels = 32
```

Rules:

- The runtime MAY clamp these values to platform defaults or user policy.
- In Player Mode, the runtime SHOULD ignore cartridge requests that exceed user policy.

------

## 3.5 VFS Mount Rules (PhysFS-backed)

### 3.5.1 Mount Order and Search Path

ARCANEE MUST mount resources in a deterministic order:

1. `cart:/` — cartridge source (directory or archive)
2. `temp:/` — runtime cache mount (per-cartridge, writable)
3. `save:/` — persistent save mount (per-cartridge, writable if permitted)

Read resolution rules:

- Reads against `cart:/path` MUST resolve only in the cartridge mount.
- Reads against `save:/path` MUST resolve only in the save mount.
- Reads against `temp:/path` MUST resolve only in the temp mount.

ARCANEE MUST NOT implement implicit “search” across mounts for these explicit namespaces. (This prevents ambiguity and security issues.)

### 3.5.2 Read/Write Policy

- `cart:/` MUST be read-only during runtime execution.
- `save:/` and `temp:/` MAY be writable depending on permission and policy.
- Writes to `cart:/` MUST fail.

### 3.5.3 Save and Temp Root Mapping

The runtime MUST derive a unique host directory for `save:/` and `temp:/` based on:

- the cartridge `id`
- the user identity/profile (if applicable)
- the engine version policy (to preserve compatibility)

Recommended host mapping (informative):

- Save root: `…/ARCANEE/save/<cartridge_id>/`
- Temp root: `…/ARCANEE/temp/<cartridge_id>/`

The mapping MUST be stable across sessions.

------

## 3.6 Path Canonicalization and Security

### 3.6.1 Normalization Rules

ARCANEE MUST normalize VFS paths before resolution:

- Accept both `/` and `\` in inputs, normalize to `/`.
- Collapse repeated separators (`//` → `/`).
- Resolve `.` segments.
- **Reject** any path containing `..` segments after normalization.

If a path violates rules, the operation MUST fail with an error.

#### 3.6.1.1 Reference Implementation (Normative Pseudocode)

```c++
// Path normalization - NORMATIVE reference implementation

std::optional<std::string> normalizePath(const std::string& input) {
    // Step 1: Extract and validate namespace prefix
    size_t colonPos = input.find(":/*");
    if (colonPos == std::string::npos) {
        return std::nullopt;  // Missing namespace
    }
    
    std::string ns = input.substr(0, colonPos);  // "cart", "save", or "temp"
    if (ns != "cart" && ns != "save" && ns != "temp") {
        return std::nullopt;  // Invalid namespace
    }
    
    std::string path = input.substr(colonPos + 2);  // After ":/"
    
    // Step 2: Replace backslashes with forward slashes
    std::replace(path.begin(), path.end(), '\\', '/');
    
    // Step 3: Split into segments and process
    std::vector<std::string> segments;
    std::istringstream stream(path);
    std::string segment;
    
    while (std::getline(stream, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;  // Skip empty and current-dir segments
        }
        if (segment == "..") {
            return std::nullopt;  // REJECT parent traversal
        }
        segments.push_back(segment);
    }
    
    // Step 4: Reconstruct normalized path
    std::string result = ns + ":/";
    for (size_t i = 0; i < segments.size(); ++i) {
        result += segments[i];
        if (i < segments.size() - 1) result += "/";
    }
    
    return result;
}
```

### 3.6.2 Namespace Enforcement

All VFS operations MUST require an explicit namespace prefix:

- `cart:/…`, `save:/…`, or `temp:/…`

Paths without a namespace MUST be rejected.

### 3.6.3 Symlinks and Host Escapes

When using directory cartridges (Dev mode), the runtime MUST prevent escaping the project root via:

- symlinks/junctions
- OS-specific path tricks

Minimum requirement (normative):

- Any resolved host path MUST be verified to be within the canonical project root directory for `cart:/` operations.

------

## 3.7 VFS API Semantics (`fs.*`)

### 3.7.1 Text Encoding

- `fs.readText()` MUST return UTF-8 text.
- `fs.writeText()` MUST write UTF-8 text.
- If the file is not valid UTF-8, `fs.readText()` MUST fail.

### 3.7.2 Binary Semantics

- `fs.readBytes()` returns an array of ints in range `[0,255]`.
- `fs.writeBytes()` writes exactly the provided bytes.

### 3.7.3 Atomicity and Consistency

v0.1 requirements:

- `fs.writeText/Bytes` SHOULD be atomic at the file level where feasible:
  - write to temp file
  - flush
  - rename/replace
- If atomicity is not possible on the platform, the runtime MUST document the weaker guarantee and MUST ensure best-effort consistency.

### 3.7.4 Directory Listing Order

- `fs.listDir()` MUST return entries in **stable lexicographical order** to avoid nondeterminism.

### 3.7.5 Stat Fields

`fs.stat(path)` MUST return:

- `type`: `"file"` or `"dir"`
- `size`: integer (0 for directories)
- `mtime`: integer (Unix epoch seconds) or `null` if unavailable
  If `mtime` is present, it MUST refer to the file in that namespace only.

------

## 3.8 Quotas, Limits, and Failure Behavior

### 3.8.1 Storage Quotas

ARCANEE MUST enforce per-cartridge quotas (configurable by user policy):

- `save:/` maximum total bytes (default policy-defined; v0.1 RECOMMENDS at least 10–50 MB)
- `temp:/` maximum total bytes (policy-defined; may be larger but purgeable)

If quota is exceeded:

- Writes MUST fail with a clear error.
- The runtime SHOULD provide a Workbench UI indication and a remediation suggestion (e.g., “clear temp cache”).

### 3.8.2 File Count and Path Length

ARCANEE SHOULD enforce:

- Maximum file count per namespace (to prevent pathological projects)
- Maximum path length (to ensure cross-platform compatibility)

### 3.8.3 Failure Handling

All VFS failures MUST:

- Fail safely (no partial corruption where atomicity is implemented).
- Set `sys.getLastError()` with a human-readable message.
- In Dev Mode, log a detailed message including operation type and normalized path (but MUST NOT leak host absolute paths in Player Mode).

------

## 3.9 Permissions Enforcement (Normative)

### 3.9.1 Enforcement Points

Permissions MUST be enforced at:

1. API exposure time (optional): hide modules not permitted.
2. API call time (required): validate permission on each call.
3. Backend time (required): the underlying system must not allow bypass.

### 3.9.2 Save Storage Permission

If `permissions.save_storage=false`:

- Any `fs.write*`, `fs.mkdir`, `fs.remove` targeting `save:/` MUST fail.
- `fs.read*` against `save:/` MAY succeed if files exist (policy choice), but v0.1 RECOMMENDS: **reads succeed, writes fail** to allow read-only behavior in restricted mode.

### 3.9.3 Temp Storage Permission

`temp:/` is owned by the runtime. Cartridges MAY be allowed to write to `temp:/` even when `save_storage=false`, but this MUST be explicitly documented. v0.1 RECOMMENDS:

- `temp:/` writable by default (cache-friendly), still quota-enforced.

### 3.9.4 Audio Permission

If `permissions.audio=false`, all `audio.*` calls MUST fail with an error.

### 3.9.5 Network and Native Permissions

- v0.1 does not require a networking subsystem; however, the permission flag MUST exist and be enforced if any net features are added later.
- `native` MUST remain disabled by default and MUST prevent any execution of host-native code initiated by script.

------

## 3.10 Development vs Distribution Behavior

### 3.10.1 Workbench (Dev) Cartridge Source

In Workbench, cartridges are loaded from directory sources and edited in-place. Additional rules:

- Hot reload MAY re-mount `cart:/` after file changes.
- The VFS MUST ensure that `cart:/` reflects the editor’s current project state.

### 3.10.2 Distribution Cartridges

When running an archive cartridge:

- `cart:/` MUST be immutable.
- The runtime SHOULD verify a manifest signature or hash in future versions; v0.1 does not require signing.

### 3.10.3 Save Migration

If cartridge `id` remains the same across versions:

- `save:/` MUST map to the same location.
- The runtime MUST NOT delete or overwrite saves automatically.

------

## 3.11 Compliance Checklist (Chapter 3)

An implementation is compliant with Chapter 3 if it satisfies all of the following:

1. Supports both directory and archive cartridges with identical VFS layout.
2. Requires a manifest and entry script; refuses to run missing/invalid cartridges.
3. Implements `cart:/`, `save:/`, `temp:/` namespaces with explicit, non-ambiguous resolution.
4. Enforces read-only `cart:/` and permission-controlled write access to `save:/`.
5. Normalizes paths, rejects `..`, and prevents host filesystem escape (including via symlinks in dev).
6. Enforces quotas and reports errors via `sys.getLastError()` and Workbench diagnostics.
7. Provides deterministic directory listing order and stable behavior across platforms.