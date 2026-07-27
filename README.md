# n1mbus

A lightweight OpenGL overlay injector for Minecraft Java Edition.

## Overview

n1mbus consists of two components:

- **Injector** (`n1mbus_injector.exe`) — A GUI application that detects Minecraft processes by their loaded render modules (LWJGL). Click a detected target to inject.
- **DLL** (`n1mbus_dll.dll`) — The injected module that hooks into the game's OpenGL pipeline.

## Building

Requires Visual Studio 2022 and CMake 3.20+.

```
cmake -B build
cmake --build build --config Release
```

The injector binary and DLL will be placed in `build/Release/` along with required assets.

## Usage

1. Launch `n1mbus_injector.exe`.
2. Start Minecraft Java Edition.
3. Targets appear automatically — click one to inject.

## Target detection

Scans `java.exe` / `javaw.exe` processes for loaded LWJGL modules. Falls back to window title matching if module access is unavailable.

## License

Custom — see [LICENSE](LICENSE).
