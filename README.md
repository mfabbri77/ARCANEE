# ARCANEE

**A**dvanced **R**etro **C**omputing **A**nd **N**ext-gen **E**ntertainment **E**ngine

ARCANEE is a modern fantasy console runtime with an integrated Workbench IDE, designed for developers creating 2D and 3D games using the Squirrel scripting language.

## Features

- **Squirrel Scripting Engine** - Lightweight, fast scripting with sandboxed execution
- **2D Canvas API** - ThorVG-backed vector graphics (paths, gradients, text, images)
- **3D Scene API** - DiligentEngine-based PBR rendering with glTF support
- **Audio System** - Tracker module playback (libopenmpt) and SFX mixing
- **Virtual Filesystem** - Sandboxed file access with namespace isolation
- **Workbench IDE** - Integrated development, debugging, and profiling

## Requirements

### Linux (Debian/Ubuntu)

```bash
# Build tools
sudo apt-get install build-essential cmake

# Required dependencies
sudo apt-get install libsdl2-dev libphysfs-dev

# Optional (for future phases)
# sudo apt-get install libopenmpt-dev
```

### Other Platforms

- **Windows**: Use vcpkg or manual installation
- **macOS**: Use Homebrew (`brew install sdl2 physfs`)

## Building

```bash
# Configure
mkdir -p build && cd build
cmake ..

# Build
make -j$(nproc)

# Run
./bin/arcanee
```

## IDE Configuration

### VS Code with CMake Tools

If you see "Configure Failed" errors in VS Code:

1. **Ensure dependencies are installed** (see Requirements above)

2. **Clean and reconfigure**:
   ```bash
   rm -rf build/
   mkdir build && cd build
   cmake ..
   ```

3. **Reload VS Code**: `Ctrl+Shift+P` → "Developer: Reload Window"

### Clangd Integration

For proper clangd intellisense, generate compile_commands.json:

```bash
cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
ln -sf build/compile_commands.json .
```

## Project Structure

```
ARCANEE/
├── src/
│   ├── app/          # Application entry and runtime
│   ├── common/       # Shared types, logging, utilities  
│   ├── platform/     # SDL2 platform layer
│   ├── runtime/      # Cartridge management, manifest
│   ├── vfs/          # Virtual filesystem (PhysFS)
│   ├── render/       # (future) DiligentEngine rendering
│   └── script/       # (future) Squirrel integration
├── specs/            # Technical specifications
├── cmake/            # CMake find modules
└── build/            # Build output
```

## Development Status

See [specs/](specs/) for complete technical documentation.

### Phase 1: Core Foundation ✓
- [x] Platform Layer (SDL2 window, timing, fullscreen)
- [x] Virtual Filesystem (PhysFS with namespace isolation)
- [x] Manifest Parser (TOML parsing and validation)

### Phase 2: Scripting Engine (Planned)
- [ ] Squirrel VM Integration
- [ ] Module Loader
- [ ] Cartridge Lifecycle
- [ ] API Binding Framework

### Phase 3+: Rendering, Audio, Input, Workbench
See implementation plan for full roadmap.

## Specifications

The `specs/` directory contains complete technical specifications:

| Chapter | Topic |
|---------|-------|
| 1 | Scope, Goals, Definitions |
| 2 | System Architecture |
| 3 | Cartridge Format, VFS, Sandbox |
| 4 | Execution Model, Squirrel |
| 5 | Rendering Architecture |
| 6 | 2D Canvas API |
| 7 | 3D Scene API |
| 8 | Audio System |
| 9 | Input System |
| 10 | Workbench IDE |
| 11 | Error Handling |
| 12 | Performance Budgets |

## License

(License information here)

## Contributing

(Contribution guidelines here)
