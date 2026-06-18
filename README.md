# Godot 4 Luau Core Module

A high-performance, cross-platform, drop-in C++ engine module that embeds Roblox's JIT-optimized **Luau VM** directly into the core of **Godot 4.x**. 

This module provides the raw source code for the C++ bridge, allowing developers to bypass GDScript performance bottlenecks in heavy computational tasks (procedural generation, advanced AI) while maintaining a native, open-source workflow across Windows, macOS, and Linux.

## Key Features

* **Out-of-the-Box Integration:** Designed to be dropped directly into `godot/modules/` and compiled natively.
* **Cross-Platform Support:** Pure C++17 architecture running seamlessly on Windows (MSVC/MinGW), macOS (Intel/Apple Silicon), and Linux (GCC/Clang).
* **JIT-Powered Processing:** Leverages Luau's fast VM execution for heavy mathematical logic loops.
* **Zero-Allocation Vectors:** Moves Vector3 data through ultra-lightweight native Luau tables `{x, y, z}` to prevent garbage collection spikes.

## Installation & Compilation (Drag & Drop)

1. Clone the official Godot Engine repository (v4.x branch):
   ```bash
   git clone https://github.com/godotengine/godot.git
   ```

2. Drop this `godot_luau_bridge` folder directly into the `godot/modules/` directory.

3. Ensure the official Luau source headers are placed in `godot_luau_bridge/thirdparty/luau/`.

4. Build the Godot editor using SCons for your specific platform:

   **macOS:**
   ```bash
   scons platform=macos target=editor
   ```

   **Windows:**
   ```bash
   scons platform=windows target=editor
   ```

   **Linux:**
   ```bash
   scons platform=linuxbsd target=editor
   ```

## License & Status

This repository is currently staging in private mode for core architectural verification.
