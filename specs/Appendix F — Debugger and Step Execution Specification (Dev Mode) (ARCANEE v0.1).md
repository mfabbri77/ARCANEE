# Appendix F — Debugger and Step Execution Specification (Dev Mode) (ARCANEE v0.1)

## F.1 Scope

This appendix specifies the **Dev Mode debugging architecture** for ARCANEE v0.1, enabling:

* breakpoints (file:line)
* step execution (into/over/out)
* call stack inspection
* local variable inspection (scope-visible locals)
* watch expressions (evaluated safely)
* error/exception break
* integration with the Workbench UI
* optional external debug adapter (DAP-like) constrained to localhost

ARCANEE v0.1 requires a minimum debugging feature set; advanced integration is optional but formally defined for consistency.

---

## F.2 Debugging Levels (Normative)

ARCANEE defines three debugging levels:

### F.2.1 Level 0 — Minimal (Required)

Must support:

* Pause/Resume cartridge execution
* Display last error and stack trace for uncaught exceptions
* Navigate to file:line in Workbench editor

### F.2.2 Level 1 — Integrated Step Debugger (Strongly Recommended)

Adds:

* Breakpoints at source file:line
* Step Into / Step Over / Step Out
* Call stack view while paused
* Locals view for the selected stack frame

### F.2.3 Level 2 — External Debug Adapter (Optional)

Adds:

* External client connection (IDE integration) via a DAP-like protocol
* Remote control of breakpoints/stepping/stack/locals
* Watch expressions

Implementations may ship Level 1 only and still conform to v0.1. If Level 2 exists, it MUST follow the constraints in §F.10.

---

## F.3 Build and Compilation Requirements (Normative)

### F.3.1 Debug Metadata

To support stepping and breakpoints, scripts MUST be compiled with debug metadata enabled in Dev Compilation profile:

* canonical source names (`cart:/...`) MUST be embedded as the “source”
* line number mapping MUST be available to the debug subsystem

If the runtime cannot provide line mapping for a script, it MUST mark that script as “non-debuggable” and Workbench MUST indicate it.

### F.3.2 Canonical Source Identity

Breakpoints MUST be keyed by:

* canonical VFS path (normalized) and
* 1-based line number

Example breakpoint identity:

* `cart:/src/player.nut:137`

---

## F.4 Execution Model Under Debugger Control (Normative)

### F.4.1 Pause Semantics

When paused:

* `update(dt)` MUST NOT execute.
* `draw(alpha)` MAY execute (Workbench-configurable) to allow overlays; if it executes, it MUST NOT advance simulation state unless the cartridge does so explicitly.

### F.4.2 Stepping Granularity

Stepping is **line-based**:

* Step operations stop execution at the next “line event” boundary in Squirrel bytecode debug info.

### F.4.3 Safe Stop Points

ARCANEE MUST only stop at safe points:

* at the boundary before executing a line
* after returning from a function (for Step Out)
* at explicit breakpoints

ARCANEE MUST NOT stop inside host-native API calls. If a breakpoint falls on a line that calls a native function, it stops before entering that line’s execution.

---

## F.5 Breakpoints (Normative)

### F.5.1 Breakpoint Types

ARCANEE v0.1 supports:

* **Line breakpoints**: file:line
* **Exception breakpoints** (optional but recommended): break on thrown/uncaught errors

### F.5.2 Breakpoint Validation

When a breakpoint is set:

* If the exact line is not a valid stop location, the runtime SHOULD move it to the nearest subsequent valid line within the same function (if deterministically resolvable).
* If no valid line exists, the breakpoint is rejected with a clear message.

Workbench MUST show the final resolved breakpoint location.

### F.5.3 Breakpoint Hit Behavior

On hit:

* execution pauses before executing the line
* call stack and locals become available

---

## F.6 Step Operations (Normative)

### F.6.1 Step Into

**Definition:** Resume execution and stop at the next line event, including inside called functions.

### F.6.2 Step Over

**Definition:** Execute the current line and stop at the next line event in the current stack frame, skipping over called functions.

Implementation note (informative):

* Achieved by setting a temporary “target depth” and stopping when returning to that depth and reaching a new line event.

### F.6.3 Step Out

**Definition:** Continue until returning from the current function and stop at the next line event in the caller.

### F.6.4 Step Safety Rules

* Step operations must not run multiple `update()` ticks automatically; they operate within the currently paused tick context.
* If stepping is invoked while not in script execution, the runtime MUST begin a controlled execution slice (e.g., single update tick) and stop at the next stop point.

---

## F.7 Call Stack Inspection (Normative)

### F.7.1 Stack Frames

When paused, the runtime MUST provide an ordered stack trace:

* top frame = current location
* each frame includes:

  * function name (if available)
  * canonical file path
  * line number

Minimum frame object:

* `{ func: string|null, file: string, line: int }`

### F.7.2 Selecting Frames

Workbench MUST allow selecting a stack frame. Selecting a frame changes:

* displayed source location
* locals view contents

---

## F.8 Locals and Scope Inspection (Normative)

### F.8.1 Definition of “Local Variables”

“Local variables” include:

* function parameters
* locally declared variables in the current frame scope
* closure-captured variables (if Squirrel exposes them distinctly)

In addition, the debugger MAY expose:

* `this` (object context if applicable)
* `::` root table (global) as a separate category

### F.8.2 Visibility Rules

Locals view MUST display only variables that are visible in the selected frame’s scope at the current line.

* Variables out of scope MUST NOT be shown as locals.
* If the underlying VM does not provide exact scope boundaries, the runtime MUST document the approximation used (e.g., “function-level locals”).

### F.8.3 Value Rendering Rules

Values are presented in a debug UI and must be safe:

* Primitive values (int/float/bool/string/null) displayed directly.
* Tables/arrays/classes displayed as expandable nodes with limited depth and count (to avoid hangs).
* Userdata handles displayed with type and id, e.g. `ImageHandle(12)`.

### F.8.4 Expansion Limits (Normative)

The debugger MUST enforce:

* maximum expansion depth (recommended: 4)
* maximum children per node (recommended: 200)
* maximum total nodes rendered per refresh (recommended: 2000)

If limits are exceeded:

* the debugger shows “(truncated)” deterministically.

---

## F.9 Watches and Expression Evaluation (Normative)

### F.9.1 Watch Expressions

A watch is a string expression evaluated in the context of the selected stack frame.

### F.9.2 Safety Constraints (Normative)

To prevent side effects and nondeterminism:

* Watch evaluation MUST be **side-effect-free** by policy.

ARCANEE v0.1 mandates one of these approaches:

**Approach A (Recommended): Restricted Expression Evaluator**

* Implement a small expression parser supporting:

  * identifiers, literals
  * field access (`a.b`)
  * index access (`a[i]` with i numeric or string literal)
  * basic arithmetic (`+ - * / %`)
  * comparisons (`== != < <= > >=`)
  * logical (`&& || !`)
* No function calls
* No assignments
* No object construction

**Approach B: VM Evaluation with Strict Sandboxing**
If using the VM to evaluate expressions:

* must forbid function calls and assignments (hard)
* must apply a CPU time budget per evaluation
* must run under a debug-only instruction budget

In v0.1, Approach A is strongly recommended.

### F.9.3 Evaluation Timing

Watches update:

* when paused and the selected frame changes
* on step completion
* on manual refresh
  Watches MUST NOT evaluate continuously while running.

### F.9.4 Failure Behavior

If a watch fails:

* show a per-watch error message
* do not fault the cartridge

---

## F.10 External Debug Adapter (Optional) (Normative if Implemented)

### F.10.1 Transport and Binding

If implemented, the adapter MUST:

* listen only on `127.0.0.1` by default
* be disabled in Player Mode by default
* be explicitly enabled from Workbench settings

### F.10.2 Session Security

Even on localhost, the adapter SHOULD require:

* a random session token displayed in Workbench
* the client must provide token on connect/initialize

### F.10.3 Protocol (DAP-like)

The adapter SHOULD follow the Debug Adapter Protocol concepts:

* `initialize`, `launch/attach`, `setBreakpoints`, `continue`, `next`, `stepIn`, `stepOut`
* `stackTrace`, `scopes`, `variables`, `evaluate`
* events: `stopped`, `terminated`, `output`

If the protocol deviates from DAP, the runtime MUST provide a formal protocol document.

### F.10.4 Variable References

For external clients, expanded variables must use stable `variablesReference` IDs with lifetime tied to the paused session state. IDs must be invalidated on resume.

---

## F.11 Integration with Workbench UI (Normative)

Workbench must provide:

* Breakpoints gutter in the editor with enable/disable
* Debug toolbar:

  * Run/Continue, Pause, Step Over, Step Into, Step Out, Stop, Reload
* Panels:

  * Call Stack
  * Locals
  * Watches
  * Output/Console (merged with sys.log/warn/error)

While paused, Workbench must show:

* current line highlight (caret + background)
* current file tab selection
* optional inline value display (hover tooltips; recommended)

---

## F.12 Determinism and Side-Effect Rules (Normative)

Debugging necessarily alters execution timing. ARCANEE defines:

* When paused, simulation does not advance.
* When stepping, simulation advances only by the executed script instructions.
* Watch evaluation MUST not mutate simulation state.

Cartridges should be able to resume and continue deterministically relative to the paused point.

---

## F.13 Compliance Checklist (Appendix F)

An implementation is compliant with Appendix F at:

* **Level 0** if it provides pause/resume, error stack traces, and clickable file:line navigation.
* **Level 1** if it also provides deterministic breakpoints, stepping, call stack, and locals inspection with truncation limits.
* **Level 2** if it additionally provides a localhost-only external debug adapter with explicit enablement and session token security, and supports breakpoints/stepping/stack/variables/evaluate safely.
