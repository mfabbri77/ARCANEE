# Blueprint v0.1 — Chapter 2: Architecture (C4), Platforms, Packaging

## 2.1 C4 — System Context
```mermaid
flowchart LR
  User((Developer/User))
  Cart[Cartridge Package\n(manifest + scripts + assets)]
  Arc[ARCANEE Runtime + Workbench]
  OS[(OS: Linux/Windows/macOS)]
  GPU[(GPU Driver)]
  Audio[(Audio Device)]
  FS[(Host Filesystem)]

  User --> Arc
  Cart --> Arc
  Arc --> OS
  Arc --> GPU
  Arc --> Audio
  Arc --> FS
```

## 2.2 C4 — Containers
```mermaid
flowchart TB
  subgraph App[ARCANEE App]
    RT[Runtime Core]
    WB[Workbench UI]
    VM[Squirrel VM]
    VFS[Sandboxed VFS (PhysFS)]
    R2D[2D Canvas (ThorVG)]
    R3D[3D Renderer (Diligent)]
    AUD[Audio Engine]
    INP[Input Manager]
    OBS[Logging/Diagnostics]
  end

  WB <--> RT
  RT --> VM
  RT --> VFS
  RT --> INP
  RT --> R2D
  RT --> R3D
  RT --> AUD
  RT --> OBS
```

## 2.3 Platform Matrix (v1.0)
Area	Linux	Windows	macOS
Window/Input	SDL2	SDL2	SDL2
GPU backend	Vulkan (preferred), OpenGL (optional)	DX12 (preferred), Vulkan (optional)	Metal (preferred)
Audio	SDL_Audio	SDL_Audio	SDL_Audio
Filesystem sandbox	PhysFS	PhysFS	PhysFS
Workbench UI	ImGui (planned)	ImGui (planned)	ImGui (planned)

## 2.4 Packaging & Distribution

    [DEC-14] Single-folder portable build for v1.0.

    [REQ-33] Install layout (portable folder):

        arcanee executable

        assets/ (engine assets)

        samples/ optional bundled samples

        licenses/ third-party notices

        workbench/ (if separate data/plugins)

    [REQ-34] Runtime must locate its data relative to executable directory (no global install required).

## 2.5 Repo Map (target end-state for v1.0)

    [REQ-35] Required directories:

        /blueprint/ (this spec)

        /src/ implementation

        /tests/ test harness + fixtures

        /tools/ CI helper scripts (checks, packaging)

        /third_party_licenses/ (or /licenses/) aggregated notices

    [DEC-19] /include/ is optional until a stable C++ SDK is required; v1.0 is primarily an app + script API.

        Consequences: “public API” for v1.0 is the cartridge/script API + file formats, not a C++ ABI.
