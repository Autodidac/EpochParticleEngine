# Epoch ecosystem integration

## EpochEngine

Link `EpochParticleEngine::Core` into the static-library graph. Keep particle code under `epochengine::particle`.

Recommended ownership:

```text
World or effects system     owns Simulation
Renderer backend            consumes RenderFrame
Audio system                consumes SimulationEvent
Media/capture system        consumes rendered images or frame commands
Editor/tools                select scenes and inject ordered input
```

The core does not own a process, window, graphics device, audio device, or encoder.

## EpochPlatformEngine

The included `EpochParticleLab` is a real EpochPlatformEngine application:

1. `epoch_platform_add_application` links the static internal-entrypoint archive.
2. `ParticleLabApplication` implements `IApplication` and exports `epoch_platform_application_factory`.
3. `on_start` creates a `ContextType::vulkan` context.
4. The context initialization callback receives `NativeWindowHandle`.
5. The lab converts Win32, X11, or Cocoa handles to `vulkan::NativeSurface`.
6. Simulation and Vulkan work stay on the context owner thread.
7. The shutdown callback waits for the device, removes the macOS input bridge, and destroys resources before the native window.

CMake resolves EpochPlatformEngine in this order:

1. an already-defined target;
2. `EPOCH_PLATFORM_SOURCE_DIR`;
3. a sibling `../EpochPlatformEngine` checkout;
4. an installed CMake package;
5. a pinned FetchContent commit when enabled.

Example:

```powershell
.\build.ps1 -Configuration Release `
  -EpochPlatformPath ..\EpochPlatformEngine
```

Do not create a second window when embedding the core in another EpochEngine application. Reuse that application's platform context and renderer.

## EpochGui

EpochGui is optional and is treated as an external public API. This repository does not modify it.

```powershell
.\build.ps1 -EpochGuiPath ..\EpochGui
```

Equivalent CMake options:

```cmake
-DEPOCH_PARTICLE_WITH_EPOCHGUI=ON
-DEPOCHGUI_SOURCE_DIR=/path/to/EpochGui
```

The current adapter uses EpochGui selectable-list geometry. Visual output still goes through `RenderFrame` and the particle Vulkan renderer. A larger editor can replace the compact overlay with EpochGui panels, splitters, timelines, palettes, and property controls without changing scene code.

## EpochAudioEngine

Audio is deliberately event-driven and non-authoritative:

```cpp
for (const auto& event : simulation.events()) {
    switch (event.type) {
    case SimulationEventType::explosion:
        // Submit a pooled explosion voice at event.position.
        break;
    case SimulationEventType::collision:
        // Scale volume and pitch from event.intensity.
        break;
    default:
        break;
    }
}
simulation.clear_events();
```

Rules:

- voice allocation and random pitch do not feed back into simulation;
- transform particle coordinates to listener/world coordinates in the audio adapter;
- apply source cooldowns and voice caps in EpochAudioEngine;
- do not include playback handles or mixer state in hashes.

## EpochMediaEngine

A media adapter can record either:

- finalized `RenderFrame` commands for a deterministic visualization replay; or
- renderer output images for video encoding or screenshots.

Command capture is smaller and renderer-independent but must retain engine version, fonts/glyph behavior, bounds, and frame timing. Image capture preserves exact visual output but is GPU/format dependent. Encoding, timestamps, dropped-frame policy, and file I/O stay in EpochMediaEngine and are excluded from simulation hashes.

## Networking and replay

Record:

- engine version and scene ID;
- seed and fixed-step duration;
- initial bounds;
- ordered input commands with target ticks;
- periodic `Simulation::state_hash()` checkpoints.

Use the fixed-point scene as the strict cross-platform lockstep reference. Floating-point scenes need a defined compiler, target ISA, and floating-point contract or an authoritative server.
