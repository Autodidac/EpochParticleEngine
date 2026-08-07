# EpochParticleEngine

EpochParticleEngine is a cross-platform C++23 library for deterministic particles, physics particles, cellular automata, and simulations that couple particle and grid representations. The repository ships a headless SDK, nine reference scenes, a native Vulkan renderer, and an interactive lab hosted by EpochPlatformEngine's internal entrypoint.

## What is included

| Key | Scene | Model |
|---:|---|---|
| 1 | Deterministic Fountain | Q16.16 fixed-point integration, PCG32 emission, stable IDs, and replay hashes |
| 2 | Cellular Automata | Double-buffered toroidal Conway Life with paint and erase input |
| 3 | Particle Life | Multi-species attraction and repulsion over a deterministic CSR spatial index |
| 4 | Hybrid Sand Lab | Falling-material cells coupled to ballistic particles, collisions, liquids, and deposition |
| 5 | Fire and Smoke | Reactive wood, oil, water, acid, fire, smoke, and ember particles |
| 6 | Fireworks | Emitters, trails, timed bursts, and simulation events |
| 7 | Binary Galaxy | Two orbiting attractors and gravitational particles |
| 8 | Boids | Separation, alignment, cohesion, obstacle avoidance, and pointer steering |
| 9 | SPH Fluid | Grid-accelerated density, pressure, viscosity, gravity, and boundaries |

The core also provides:

- a fixed-step scheduler with bounded catch-up, pause, time scale, reset, and single-step;
- structure-of-arrays particle storage with stable IDs and stable compaction;
- a persistent `std::jthread` task arena with deterministic phase rules;
- deterministic compressed-sparse-row spatial indexing;
- a material grid for granular, liquid, gaseous, flammable, and corrosive cells;
- Q16.16 fixed-point arithmetic, PCG32, coordinate hashing, and stable state hashes;
- renderer-neutral circles, rectangles, rounded rectangles, lines, and compact text;
- simulation events for emission, collision, explosion, ignition, and material deposition;
- deterministic tests, sanitizers, a replay validator, a headless scene sweep, and a benchmark.
- static and shared linkage, an installable CMake package, and an external package-consumer smoke test.

## Targets and dependency boundaries

| Target | Purpose | Required dependencies |
|---|---|---|
| `EpochParticleEngine::Core` | Simulation library | C++23 and threads only |
| `EpochParticleEngine::Vulkan` | Native Vulkan renderer | Vulkan, shaderc, and the platform window library selected by the OS |
| `EpochParticleLab` | Interactive example | Core, Vulkan renderer, and EpochPlatformEngine static/internal-entrypoint targets |
| `EpochParticleHeadless` | CSV scene sweep | Core |
| `EpochParticleReplay` | Serial versus parallel hash validation | Core |
| `EpochParticleBenchmark` | Optional CPU benchmark | Core |

The core never includes Vulkan, native-window, EpochGui, audio, or media headers. EpochGui is optional and is only used for layout geometry in the lab. EpochAudioEngine and EpochMediaEngine remain optional consumers of events and rendered frames rather than mandatory dependencies.

## Vulkan renderer

`EpochParticleEngineVulkan` consumes a finalized `RenderFrame`, uploads a 48-byte record per item to persistently mapped storage buffers, and renders the batch as instanced six-vertex quads. Circle and rounded-box coverage are evaluated in the fragment shader.

The renderer includes:

- direct Win32, Xlib, and Cocoa/Metal Vulkan surface creation;
- no GLFW dependency and no Vulkan types in the public header;
- physical-device scoring and graphics/present queue selection;
- validation fallback when the requested layer is unavailable;
- Vulkan portability enumeration and portability-subset handling for MoltenVK;
- sRGB swapchains, FIFO vsync, and mailbox/immediate fallback;
- per-frame command pools, semaphores, fences, and image ownership;
- resize/out-of-date swapchain recreation;
- automatically growing storage buffers;
- runtime GLSL-to-SPIR-V compilation through shaderc.

The renderer accepts a platform-neutral native descriptor:

```cpp
#include <epochengine/particle/vulkan/renderer.hpp>

using namespace epochengine::particle::vulkan;

NativeSurface surface{
    .kind = NativeSurfaceKind::win32,
    .window = native_hwnd
};

auto renderer = Renderer::create(surface, {
    .enable_validation = true,
    .vertical_sync = true
});
```

The included lab converts `epochengine::platform::NativeWindowHandle` to this descriptor inside the EpochPlatformEngine context initialization callback. Application code does not define `main()` or `WinMain()`; EpochPlatformEngine supplies the internal entrypoint archive.

## Requirements

- CMake 3.28 or newer for Ninja and Visual Studio 2022 builds
- CMake 4.2 or newer for the native Visual Studio 2026 generator
- A C++23 compiler
- Ninja, Visual Studio 2026/v145, Visual Studio 2022/v143, GCC, Clang, or Apple Clang
- Vulkan loader and headers plus shaderc for `EpochParticleEngine::Vulkan`
- EpochPlatformEngine for `EpochParticleLab`; a sibling checkout, installed package, explicit path, or pinned FetchContent fallback is supported
- X11 development headers on Linux for the current native Linux surface path
- MoltenVK or another Vulkan portability implementation on macOS

`vcpkg.json` pins the Vulkan and shaderc package baseline. GLFW is not used.

## Windows build

The PowerShell build script checks `VCPKG_ROOT`, then `C:\Users\iammi\source\repos\vcpkg`. It automatically uses a sibling `EpochPlatformEngine` checkout when present; otherwise CMake can fetch the pinned commit.

```powershell
# Vulkan lab, core examples, and tests
.\build.ps1 -Configuration Release -Generator Ninja

# Explicit supporting repository
.\build.ps1 -Configuration Release `
  -EpochPlatformPath ..\EpochPlatformEngine

# Native Visual Studio 2026/v145 solution
.\build.ps1 -Configuration Release -Generator VisualStudio2026

# Visual Studio 2022/v143 fallback
.\build.ps1 -Configuration Release -Generator VisualStudio2022

# CPU-only strict validation
.\build.ps1 -Configuration Debug -CpuOnly -WarningsAsErrors

# Build and run the native lab
.\run.ps1 -Configuration Release
```

Batch wrappers are included:

```bat
build.bat -Configuration Release
run.bat -Configuration Release
```

## Linux and macOS build

```bash
export VCPKG_ROOT="$HOME/vcpkg"
./build.sh Release --epochplatform ../EpochPlatformEngine
./run.sh Release
```

CPU-only and sanitizer builds do not require Vulkan or EpochPlatformEngine:

```bash
./build.sh Debug --cpu-only --warnings-as-errors
./build.sh Debug --cpu-only --sanitize
```

## CMake presets

```bash
cmake --preset cpu-debug
cmake --build --preset cpu-debug
ctest --preset cpu-debug

cmake --preset vulkan-release
cmake --build --preset vulkan-release
ctest --preset vulkan-release
```

The Vulkan presets expect `VCPKG_ROOT`. `vs2026-vulkan` creates a Visual Studio 2026/v145 solution; `vs2022-vulkan` remains available for v143.

## Controls

- `1` through `9`: select a scene
- `Tab` / `Shift+Tab`: next / previous scene
- `Space`: pause or resume
- `.`: advance one fixed tick
- `R`: reset from the current deterministic seed
- `-` / `+`: halve / double time scale
- `F1`: hide or show the control panel
- Left and right mouse buttons: scene-specific paint, erase, attract, repel, or emit action
- `Esc`: exit

## Core library use

```cpp
#include <epochengine/particle/epoch_particle_engine.hpp>

using namespace epochengine::particle;

int main()
{
    Simulation simulation({
        .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
        .maximum_catch_up_steps = 8,
        .seed = 42,
        .worker_count = TaskArena::recommended_worker_count()
    });

    simulation.resize({ 1280.0F, 720.0F });
    simulation.step_once();

    RenderFrame frame;
    frame.begin(simulation.bounds());
    simulation.render(frame);
    frame.finalize();
    return frame.items().empty() ? 1 : 0;
}
```

CMake consumer:

```cmake
find_package(EpochParticleEngine CONFIG REQUIRED)

target_link_libraries(MySimulation PRIVATE EpochParticleEngine::Core)
# Link EpochParticleEngine::Vulkan only when the installed package contains it.
```

## Determinism contract

The deterministic fountain stores authoritative position, velocity, lifetime, RNG state, and IDs in fixed-point/integer form. Given the same engine version, seed, dimensions, fixed tick count, and ordered inputs, its state hash is repeatable across supported platforms.

Floating-point scenes are deterministic across worker counts because parallel phases write disjoint ranges and reductions occur in stable order. Their hashes are intended for replay within a defined compiler/CPU floating-point contract; they are not promised bit-identical across every architecture. Fast-math is deliberately disabled.

Run the replay validator:

```bash
./out/build/ninja-cpu-release/EpochParticleReplay 180 42 8
```

## Epoch ecosystem integration

- **EpochPlatformEngine:** owns process entry, native windows, event pumps, context routes, and owner-thread callbacks. The lab uses it directly.
- **EpochGui:** optional. Enable with `-DEPOCH_PARTICLE_WITH_EPOCHGUI=ON -DEPOCHGUI_SOURCE_DIR=<path>`. This repository consumes its public layout API without modifying EpochGui.
- **EpochAudioEngine:** consume `Simulation::events()` and map explosions, impacts, ignition, and emission to voices. Playback never feeds back into deterministic state.
- **EpochMediaEngine:** capture or encode finalized `RenderFrame` output through a renderer/media adapter. Media timing and encoding remain outside simulation hashes.

See `docs/epoch-integration.md` and `docs/architecture.md` for the ownership boundaries.

## Validation

```bash
ctest --test-dir out/build/ninja-cpu-debug --output-on-failure
./out/build/ninja-cpu-release/EpochParticleHeadless 600 42 4
./out/build/ninja-cpu-release/EpochParticleReplay 180 42 4
```

The suite covers fixed-point arithmetic, RNG replay, task partitioning and exception recovery, stable particle compaction, spatial indexing, material-grid replay, render ordering, every scene, reset replay, and serial/parallel hashes.

## Repository layout

```text
include/epochengine/particle/   Public C++23 core API
include/.../vulkan/             Public native-surface Vulkan API
src/core/                       Scheduler, storage, grids, hashing, command stream
src/scenes/                     Nine reference simulations
src/vulkan/                     Vulkan backend and OS-specific surface adapters
app/                            EpochPlatformEngine lab and optional EpochGui layout bridge
examples/                       Headless sweep and replay validator
benchmarks/                     CPU scene benchmark
tests/                          Determinism and safety contracts
docs/                           Architecture and integration notes
scripts/                        Source and SDK packaging helpers
```

## License

MIT. See `LICENSE`.
