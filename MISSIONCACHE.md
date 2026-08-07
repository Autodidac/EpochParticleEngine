# Mission cache

No requested work is silently discarded. This cache separates the completed v0.1.0 baseline from explicitly deferred expansion work.

## Completed in v0.1.0

- [x] C++23 static-library-first `EpochParticleEngine` repository.
- [x] Canonical `epochengine::particle` namespace and cross-platform export macros.
- [x] Fixed-step scheduler, pause, time scale, bounded catch-up, reset, and single-step.
- [x] Persistent multi-thread task arena with deterministic phase rules, exception propagation, and safe shutdown.
- [x] Q16.16 fixed-point arithmetic and strict deterministic fountain example.
- [x] PCG32 stream RNG, coordinate hashing, stable IDs, and state hashing.
- [x] Structure-of-arrays particle pool with stable compaction.
- [x] Deterministic CSR spatial index with periodic-neighbor support.
- [x] Cellular material grid with sand, water, stone, wood, fire, smoke, oil, and acid.
- [x] Grid/physics hybrid scene with liquid response, solid collision, emission, and particle-to-cell deposition.
- [x] Conway Life cellular automata with live editing.
- [x] Particle Life species-attraction example.
- [x] Reactive fire/smoke, fireworks, galaxy, boids, and SPH fluid examples.
- [x] Renderer-neutral primitive and compact text command stream.
- [x] Raw Vulkan instanced renderer with validation fallback, portability support, swapchain recreation, and dynamic storage growth.
- [x] Direct Win32, Xlib, and Cocoa/Metal Vulkan surface adapters.
- [x] GLFW removed from the renderer and application dependency graph.
- [x] EpochPlatformEngine `IApplication` integration and static internal entrypoint.
- [x] Owner-thread initialization, input, rendering, title update, resize, and shutdown callbacks.
- [x] macOS keyboard and pointer bridge while EpochPlatformEngine retains native-window ownership.
- [x] Interactive scene browser, metrics panel, keyboard controls, and mouse tools.
- [x] Optional EpochGui scene-list layout adapter; EpochGui itself is not modified.
- [x] Simulation events suitable for EpochAudioEngine routing.
- [x] Render-frame/media boundary suitable for EpochMediaEngine capture adapters.
- [x] Headless scene sweep, serial/parallel replay validator, and CPU benchmark.
- [x] CMake install/export package, pinned vcpkg manifest, presets, and PowerShell/batch/shell scripts.
- [x] Static and shared library ABI/export validation plus an installed-package consumer smoke test.
- [x] GCC and Clang warning-clean CPU builds.
- [x] Deterministic serial/multi-thread replay tests across all nine scenes.
- [x] AddressSanitizer and UndefinedBehaviorSanitizer configuration.
- [x] CI for Linux, macOS, Windows, sanitizers, and Vulkan compilation.
- [x] Tag-driven source, CPU SDK, and Windows Vulkan release packaging.
- [x] Architecture, scene-authoring, Epoch integration, changelog, and release documentation.

## Next-stage candidates

- [ ] Vulkan compute backends for Particle Life, boids, SPH, and large cellular grids.
- [ ] Deterministic GPU integer-compute experiment with subgroup-independent ordering.
- [ ] GPU indirect draw and device-local staging for multi-million-particle scenes.
- [ ] Texture/atlas grid rendering to replace one quad per cell for very large grids.
- [ ] 3D particles, camera, depth buffer, billboards, ribbons, and volumetric effects.
- [ ] Collision shapes beyond cells: segments, circles, signed-distance fields, and engine-physics adapters.
- [ ] Versioned emitter, material, and scene asset serialization.
- [ ] Tick-stamped input recordings and snapshot/checkpoint files.
- [ ] Lockstep/network validation sample around the fixed-point scene.
- [ ] Full EpochGui docked editor with material palette, emitter graph, property panels, and timeline.
- [ ] EpochAudioEngine example mapping events to pooled spatial voices.
- [ ] EpochMediaEngine example for screenshots, command capture, and video export.
- [ ] RenderDoc labels, Vulkan debug names, timestamp queries, and per-pass GPU profiling.
- [ ] VMA or a dedicated allocation strategy after profiling shows a real need.
- [ ] Wayland, Android, iOS, WebAssembly/WebGPU, and mobile touch paths.
- [ ] GPU/CPU backend parity tests and performance regression thresholds.
