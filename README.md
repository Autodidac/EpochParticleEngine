# EpochParticleEngine

EpochParticleEngine is a cross-platform C++23 library for deterministic particles, physics particles, cellular automata, grid/particle hybrids, and interactive simulation tooling. The repository ships a renderer-neutral core library, a native Vulkan backend, EpochPlatformEngine internal-entrypoint integration, headless/replay tools, and **Particle Studio**: a live particle scene builder.

## Particle Studio

Particle Studio is scene 1 and opens by default. It is not a canned animation: it is an editable simulation playground.

Place and combine:

- particle emitters;
- attractors and repulsors;
- vortices;
- circular collision obstacles;
- burst emitters;
- randomized layouts;
- live erase/clear operations.

Controls inside Particle Studio:

- **Left mouse:** place the selected tool.
- **Right mouse:** erase the nearest editor object.
- **Middle mouse:** instant particle burst.
- **Shift:** larger/stronger placement or larger erase radius.
- **Ctrl+S:** save `EpochParticleStudio.epscene` in the working directory.
- **Ctrl+L:** load `EpochParticleStudio.epscene`.

Scene documents are validated before application. A malformed document is rejected without mutating the current editor state. The serialization hook is optional at the `IScene` level, so ordinary simulation scenes are not forced into an editor/document model.

## Included scenes

| # | Scene | Model |
|---:|---|---|
| 1 | **Particle Studio** | Editable emitters, fields, vortices, obstacles, collisions, bursts, save/load |
| 2 | Deterministic Fountain | Q16.16 fixed-point integration, PCG32 emission, stable IDs, replay hashes |
| 3 | **Flow Field** | 5,200 particles advected through a changing analytic vector field |
| 4 | Particle Life | Multi-species attraction/repulsion over deterministic CSR spatial indexing |
| 5 | Cellular Automata | Double-buffered toroidal Conway Life with paint/erase |
| 6 | **Reaction Diffusion** | Gray-Scott grid chemistry with deterministic seeding and live paint/erase |
| 7 | Hybrid Sand Lab | Falling-material cells coupled to ballistic particles, liquids, collisions, deposition |
| 8 | Fire and Smoke | Reactive wood, oil, water, acid, fire, smoke, and embers |
| 9 | Fireworks | Emitters, trails, timed bursts, and simulation events |
| 10 | Binary Galaxy | Orbiting attractors and gravitational particles |
| 11 | Boids | Separation, alignment, cohesion, obstacle avoidance, pointer steering |
| 12 | SPH Fluid | Grid-accelerated density, pressure, viscosity, gravity, boundaries |
| 13 | **Spring Cloth** | Verlet cloth, structural/shear constraints, pulling, collision, tearing |
| 14 | **Physarum Trails** | Agent/grid slime-mold hybrid with chemotaxis and diffusing trails |
| 15 | **Weather Lab** | Rain, snow, hail, gusts, bounce, drag, pointer-driven vortex wind |

The original scenes remain intentionally different models rather than cosmetic presets. v0.2.0 adds five new simulation families plus the editor and wraps reference scenes in a lightweight showcase overlay so scene identity and controls are obvious while testing.

## Global controls

- `1` through `9`: select the first nine scenes directly.
- `Tab` / `Shift+Tab`: next / previous scene across all 15.
- Click any scene in the side panel, including scenes 10–15.
- `Space`: pause/resume.
- `.`: advance one fixed tick.
- `R`: deterministic reset.
- `-` / `+`: halve/double simulation speed.
- `F1`: hide/show the side panel.
- Left/right mouse: scene-specific interaction.
- `Ctrl+S` / `Ctrl+L`: save/load when the active scene exposes an editable document.
- `Esc`: exit.

## Core library

The core is independent of Vulkan, native windowing, EpochGui, audio, and media. It provides:

- frame-rate-independent fixed-step scheduling with a bounded per-frame catch-up budget that retains backlog, plus pause, time scale, reset, and single-step;
- structure-of-arrays particle storage with stable IDs and stable compaction;
- persistent `std::jthread` task execution;
- deterministic CSR spatial indexing;
- reactive material grids for granular/liquid/gas/fire/corrosion simulations;
- Q16.16 fixed-point math, PCG32, coordinate hashing, and stable state hashes;
- renderer-neutral circles, rectangles, rounded rectangles, lines, and text;
- simulation events for emissions, impacts, explosions, ignition, and deposits;
- optional scene-document serialization through `IScene::scene_document()` and `apply_scene_document()`;
- static/shared linkage and installable CMake package exports.

Basic consumer:

```cpp
#include <epochengine/particle/epoch_particle_engine.hpp>

using namespace epochengine::particle;

int main()
{
    Simulation simulation({
        .fixed_step = std::chrono::nanoseconds{16'666'667},
        .maximum_catch_up_steps = 8,
        .seed = 42,
        .worker_count = TaskArena::recommended_worker_count()
    });

    simulation.resize({1280.0F, 720.0F});
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
```

## Targets

| Target | Purpose | Required dependencies |
|---|---|---|
| `EpochParticleEngine::Core` | Simulation library | C++23 + threads |
| `EpochParticleEngine::Vulkan` | Native Vulkan renderer | Vulkan + shaderc + OS surface support |
| `EpochParticleLab` | Particle Studio / interactive scene browser | Core + Vulkan + EpochPlatformEngine |
| `EpochParticleHeadless` | Headless scene sweep | Core |
| `EpochParticleReplay` | Serial-vs-parallel replay validation | Core |
| `EpochParticleBenchmark` | Optional CPU benchmark | Core |

## Vulkan renderer

`EpochParticleEngine::Vulkan` consumes finalized `RenderFrame` batches and renders instanced quads. Circle, rounded-rectangle, and packed 5x7 glyph coverage are evaluated in the fragment shader, so text costs one GPU instance per character.

The backend includes:

- direct Win32, Xlib, and Cocoa/Metal surface creation;
- WSI entrypoint resolution through `vkGetInstanceProcAddr`;
- no GLFW dependency;
- physical-device scoring and graphics/present queue selection;
- validation fallback;
- Vulkan portability enumeration for MoltenVK;
- sRGB swapchains with FIFO/mailbox/immediate selection;
- resize/out-of-date swapchain recreation;
- persistently mapped, automatically growing GPU buffers;
- runtime GLSL-to-SPIR-V compilation through shaderc.

## Epoch ecosystem boundaries

- **EpochPlatformEngine:** owns process entry, native windows, event pumps, context routes, and owner-thread callbacks. `EpochParticleLab` uses its static internal entrypoint.
- **EpochGui:** optional and read-only from this repository's perspective. The adapter targets v0.87.43's public `epoch/gui` layout API; font rasterization remains in EpochParticleEngine's GPU renderer.
- **EpochAudioEngine:** can consume `Simulation::events()` for impacts, explosions, ignition, and emission sounds without entering deterministic state.
- **EpochMediaEngine:** can capture finalized render frames through a separate adapter without entering simulation hashes.

## Requirements

- CMake 3.28+ for Ninja / Visual Studio 2022 workflows.
- CMake 4.2+ for the native Visual Studio 2026 generator.
- C++23 compiler.
- Ninja, Visual Studio 2026/v145, Visual Studio 2022/v143, GCC, Clang, or Apple Clang.
- Vulkan loader/headers and shaderc for the Vulkan target.
- EpochPlatformEngine for the interactive lab.
- X11 development headers on Linux for the current Linux surface path.
- MoltenVK or another Vulkan portability implementation on macOS.

The vcpkg manifest carries shaderc/Vulkan and enables `vulkan-loader[xlib]` on Linux. The build does not mutate the user's vcpkg checkout.

## Windows

```powershell
# Full Vulkan lab + examples + tests
.\build.bat -Configuration Release

# Run Particle Studio
.\run.bat -Configuration Release

# Strict CPU-only validation
.\build.bat -Configuration Debug -CpuOnly -WarningsAsErrors

# Strict CPU-only shared-library validation
.\build.bat -Configuration Release -CpuOnly -Shared -WarningsAsErrors
```

PowerShell exposes additional generator/platform arguments:

```powershell
.\build.ps1 -Configuration Release -Generator VisualStudio2026
.\build.ps1 -Configuration Release -Generator Ninja
.\build.ps1 -Configuration Release -EpochPlatformPath ..\EpochPlatformEngine
```

## Linux / macOS

```bash
export VCPKG_ROOT="$HOME/vcpkg"
./build.sh Release --epochplatform ../EpochPlatformEngine
./run.sh Release
```

CPU-only validation does not require Vulkan or EpochPlatformEngine:

```bash
./build.sh Debug --cpu-only --warnings-as-errors
./build.sh Debug --cpu-only --sanitize
./build.sh Release --cpu-only --shared --warnings-as-errors
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

## Determinism and validation

The deterministic fountain stores authoritative motion/RNG/ID state in fixed-point/integer form. Floating-point scenes preserve deterministic worker-count behavior by using stable update order or disjoint parallel writes with stable reductions. Fast-math remains disabled.

Every default scene is exercised by the replay suite. v0.2.0 also tests Particle Studio document roundtripping and malformed-document rejection.

```bash
ctest --test-dir out/build/ninja-cpu-debug --output-on-failure
./out/build/ninja-cpu-release/EpochParticleReplay 180 42 8
./out/build/ninja-cpu-release/EpochParticleHeadless 600 42 4
```

CI covers GCC, Clang, Apple Clang, Visual Studio 2026, ASan/UBSan, Linux and Windows shared-library installed-package consumption, and a complete Vulkan/vcpkg build.

Tagged releases publish a source archive and a portable SHA-256 checksum that is verified before upload.

## Repository layout

```text
include/epochengine/particle/   Public C++23 API
include/.../vulkan/             Public native-surface Vulkan API
src/core/                       Scheduler, storage, grids, hashing, command stream
src/scenes/                     Fifteen default scenes including Particle Studio
src/vulkan/                     Vulkan backend and OS-specific surface adapters
app/                            EpochPlatformEngine lab and optional EpochGui bridge
examples/                       Headless sweep and replay validator
benchmarks/                     Optional CPU benchmark
tests/                          Determinism, safety, serialization contracts
docs/                           Architecture, integration, release notes
scripts/                        Build/package helpers
```

## License

MIT. See `LICENSE`.
