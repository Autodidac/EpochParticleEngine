# Changelog

All notable changes are recorded here. The project follows semantic versioning.

## 0.2.1 - 2026-08-08

### Fixed

- Fixed strict MSVC shared-library builds failing on the intentional public standard-library value-type ABI.
- Made source-package checksum files portable and verified them before release upload.
- Removed an obsolete hard-coded branch deletion from the generic release cleanup job.

### Build and CI

- Added `-Shared` and `--shared` to the Windows and Unix build wrappers.
- Added Windows shared-library tests plus an installed `find_package` consumer to CI.
- Required release tags to match the CMake, public-header, and vcpkg manifest versions.

### Validation

- Passed Debug/static and Release/shared MSVC builds, all 11 test groups, installed-package consumption, replay/headless sweeps, the benchmark, the C++23 module facade, and MSVC static analysis.

## 0.2.0 - 2026-08-08

### Particle Studio

- Replaced the old first-scene showcase with **Particle Studio**, an interactive particle scene builder.
- Added placeable emitters, attractors, repulsors, vortices, and circular collision obstacles.
- Added live erase, burst, randomized-layout, and clear tools with Shift-modified larger/stronger placement.
- Added deterministic editor-scene documents plus `Ctrl+S`/`Ctrl+L` persistence to `EpochParticleStudio.epscene`.
- Added an optional `IScene` scene-document contract and routed it through `Simulation` without forcing persistence on ordinary scenes.

### New simulations

- Added Flow Field advection with 5,200 particles and interactive attract/repel steering.
- Added Gray-Scott Reaction Diffusion with deterministic seeding and live chemical paint/erase.
- Added tearable Verlet Spring Cloth with structural/shear constraints and mouse pulling/cutting.
- Added Physarum Trails, an agent/grid slime-mold hybrid with chemotaxis and trail diffusion.
- Added Weather Lab combining rain, snow, hail, wind gusts, hail bounce, and pointer-driven vortex wind.
- Expanded the default catalog from 9 to 15 scenes and kept every scene inside the deterministic replay sweep.

### Existing scene presentation

- Wrapped the existing reference simulations in a lightweight showcase overlay with clear scene identity and controls.
- Kept the original simulation models intact while making the catalog easier to explore with Tab, Shift+Tab, and the clickable scene panel.

### Validation

- Added Particle Studio document roundtrip and malformed-document regression coverage.
- Expanded the default-suite contract to all 15 scenes.
- Preserved strict GCC, Clang, Apple Clang, MSVC/Visual Studio 2026, sanitizer, shared-package consumer, and full Vulkan/vcpkg validation.

## 0.1.5 - 2026-08-08

### Fixed

- Fixed the Vulkan lab's GCC `-Wsubobject-linkage` failure by giving `LaunchOptions` and `ApplicationState` normal `epochengine::particle::demo` namespace linkage.
- Resolved platform Vulkan WSI creation functions through `vkGetInstanceProcAddr` instead of requiring extension symbols to be directly link-exported by the loader.
- Enabled the vcpkg Vulkan loader's `xlib` feature on Linux so `VK_KHR_xlib_surface` is available at runtime as well as compile time.
- Preserved the v0.1.4 logical font-height and per-monitor DPI fixes while making the full Vulkan example warning-clean under the strict CI build.

## 0.1.4 - 2026-08-08

### Fixed

- Replaced ambiguous bitmap-cell text scale with an explicit logical-height plus DPI-scale contract.
- Added `TextSize`, `BitmapTextMetrics`, and shared text metric helpers to the public API and module facade.
- Updated `RenderFrame` text rasterization so logical font height controls the actual glyph height; the legacy cell-scale overload remains source-compatible.
- Raised the interactive lab to readable 12-18 logical-pixel font roles and made row/footer spacing derive from the same metrics used to render text.
- Added per-monitor DPI event handling to the lab and separated font DPI scaling from geometry scaling.
- Added compile-time and runtime regression checks for 150% DPI text sizing and emitted glyph geometry.
- Added `screen_space.hpp` and `text.hpp` to the installed public-header set.

## 0.1.3 - 2026-08-08

### Fixed

- Corrected the Vulkan pixel-to-NDC Y conversion so upper-left-origin `RenderFrame` content is no longer vertically mirrored.
- Restored visual alignment between rendered controls and native platform pointer coordinates.
- Added a public upper-left, positive-Y-down screen-space contract shared by header and module consumers.
- Added compile-time corner-mapping checks and a shader-source regression guard against reintroducing the Y inversion.

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
