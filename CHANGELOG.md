# Changelog

All notable changes are recorded here. The project follows semantic versioning.

## 0.1.2 - 2026-08-08

### Fixed

- Removed the unnecessary vcpkg `builtin-baseline`, allowing manifest installation to use the selected checkout's current ports without Git history lookups.
- Removed automatic fetches and mutations of the user's vcpkg checkout.
- Fixed `vswhere` JSON-array parsing under Windows PowerShell 5.1.
- Normalized Visual Studio versions before sorting and generator selection.

## 0.1.1 - 2026-08-08

### Windows build and packaging fixes

- Replaced the invalid vcpkg baseline with a verified registry commit containing `versions/baseline.json`.
- Changed the default Windows generator from Ninja to automatic Visual Studio selection.
- Added installed Visual Studio detection through `vswhere`, preferring Visual Studio 2026 and falling back to Visual Studio 2022.
- Added graceful fallback from Ninja when `ninja.exe` or a configured C++ compiler environment is unavailable.
- Removed the hard-coded user name from vcpkg discovery and added `-VcpkgRoot` plus `-VcpkgTriplet` overrides.
- Added vcpkg bootstrap and pinned-baseline fetch validation before CMake configuration.
- Made `run.ps1` consume the exact successful build directory recorded by `build.ps1`.
- Updated CI and release jobs to use the same verified vcpkg baseline and to exercise the public Windows batch wrapper.

## 0.1.0 - 2026-08-07

### Core library

- Added the C++23 `EpochParticleEngine::Core` static-library-first target.
- Added fixed-step scheduling, bounded catch-up, pause, reset, time scaling, and single-step control.
- Added Q16.16 fixed-point arithmetic, PCG32, coordinate hashing, and stable state hashing.
- Added a structure-of-arrays particle pool with stable IDs and stable compaction.
- Added a persistent `std::jthread` task arena with exception propagation and deterministic phase rules.
- Added deterministic CSR spatial indexing and a reactive cellular material grid.
- Added renderer-neutral primitive/text commands and simulation event output.

### Reference simulations

- Added Deterministic Fountain, Cellular Automata, Particle Life, Hybrid Sand, Fire and Smoke, Fireworks, Binary Galaxy, Boids, and SPH Fluid scenes.
- Added live pointer tools and per-scene statistics.
- Added serial versus parallel replay validation for all scenes.

### Vulkan and platform

- Added `EpochParticleEngine::Vulkan` with instanced primitive rendering, shaderc compilation, validation fallback, dynamic buffers, and swapchain recreation.
- Added direct Win32, Xlib, and Cocoa/Metal Vulkan surface adapters; GLFW is not used.
- Added `EpochParticleLab` as an EpochPlatformEngine `IApplication` using the static internal entrypoint.
- Added owner-thread event, frame, resize, title, and shutdown integration.
- Added a Cocoa input bridge for keyboard and pointer controls while preserving EpochPlatformEngine window ownership.
- Kept EpochGui optional and read-only from this repository's perspective.

### Tooling and packaging

- Added CMake install/export packages, vcpkg manifest, Ninja and Visual Studio presets, PowerShell/batch/shell build scripts, and source packaging.
- Added explicit cross-platform exports for fixed-point, hashing, RNG, core, scene, and Vulkan APIs; both static and shared consumers are supported.
- Added GCC, Clang, MSVC, sanitizer, deterministic replay, and Vulkan compile CI coverage.
- Added tag-driven release packaging for source, Linux/macOS CPU SDKs, and the Windows Vulkan lab.
