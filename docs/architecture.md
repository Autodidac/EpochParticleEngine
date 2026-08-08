# Architecture

## Dependency layers

```text
Game, tool, server, or editor
    ├── EpochParticleEngine::Core
    │      ├── fixed-step Simulation
    │      ├── IScene implementations
    │      ├── structure-of-arrays ParticlePool
    │      ├── deterministic CSR UniformGridIndex
    │      ├── MaterialGrid
    │      ├── persistent TaskArena
    │      ├── state hashing and SimulationEvent output
    │      └── renderer-neutral RenderFrame
    └── EpochParticleEngine::Vulkan (optional)
           ├── Win32 / Xlib / Cocoa-Metal native surface adapter
           ├── shaderc GLSL compilation
           ├── swapchain and frame synchronization
           └── instanced primitive pipeline

EpochParticleLab
    ├── EpochPlatformEngine static runtime
    ├── EpochPlatformEngine internal entrypoint archive
    ├── EpochParticleEngine::Core
    ├── EpochParticleEngine::Vulkan
    └── EpochGui layout adapter (optional)
```

The core has no Vulkan, platform, GUI, audio, or media dependency. Headless tools and servers therefore build without the graphics stack.

## Ownership and threading

EpochPlatformEngine owns the process entrypoint and native event loop. `EpochParticleLab` implements `epochengine::platform::IApplication`; it does not define `main()` or `WinMain()`.

A platform context owns one simulation, one renderer, and its UI command generation. Context initialization, frame, event, and shutdown callbacks execute on the context owner thread. Vulkan creation, rendering, title changes, and destruction therefore preserve thread affinity.

The simulation's internal `TaskArena` may use background workers, but scene phases must write disjoint output ranges and complete before stable reductions, compaction, or event publication.

## Frame flow

1. EpochPlatformEngine pumps ordered native events.
2. The context callback translates input to `PointerEvent` or simulation commands.
3. `Simulation::advance` accumulates elapsed time and executes zero or more fixed ticks.
4. The active scene performs read-only neighbor queries and disjoint parallel writes.
5. Stable serial phases compact particles, perform reductions, and publish events.
6. The scene appends renderer-neutral primitives and packed glyph commands to `RenderFrame`.
7. The optional UI appends layout-driven shapes and one command per visible glyph to the same stream.
8. `RenderFrame::finalize` performs a stable layer sort.
9. The Vulkan backend uploads the batch, issues one instanced draw, and rasterizes glyph bitmaps in the fragment shader.
The catch-up limit bounds simulation work performed for one rendered frame; it never deletes accumulated fixed-step time. Backlog drains over subsequent frames, so equal elapsed time produces equal authoritative state regardless of frame chunking.


## Compute simulation boundary

`Simulation` accepts an optional `IComputeBackend` and forwards it through `SceneUpdateContext`. The core library does not depend on Vulkan; CPU-only, replay, sanitizer, and server builds continue to execute the deterministic scene kernels.

The optional Vulkan backend owns a compute-only device/queue, one reusable mapped storage buffer, a command pool/fence, and cached pipelines keyed by stable program IDs. The five compute-enabled systems use double-buffered regions:

1. CPU scene state is packed into the current-state region.
2. A compute dispatch reads current state without mutation.
3. Each invocation writes only its particle/cell slot in a next-state region.
4. SPH runs density first, then pressure/force/integration against the completed density region.
5. Fence completion makes results visible for state mirroring, metrics, hashing, and renderer-neutral command generation.

Fixed ticks, immutable inputs, and disjoint outputs make results independent of rendered-frame chunking and workgroup order on the same device. The deterministic CPU backend remains authoritative for portable replay because Vulkan floating-point arithmetic is not guaranteed to be bit-identical between GPU architectures.
## Particle storage

`ParticlePool` uses one contiguous vector per property:

```text
positions       Vec2[]
velocities      Vec2[]
colors          Color[]
radii           float[]
lifetimes       float[]
inverse_masses  float[]
species         uint32[]
flags           uint32[]
ids             uint64[]
```

Integration and neighbor-force phases touch only the streams they need. Stable compaction preserves relative order and persistent IDs.

## Spatial index

`UniformGridIndex` builds a compressed sparse row representation:

```text
offsets[cell] ... offsets[cell + 1] -> indices[]
```

Build phases:

1. count positions per cell;
2. prefix-sum the counts;
3. insert indices in ascending particle order.

Neighbor traversal has stable cell and particle order. Periodic scenes use wrapped lookup and minimum-image displacement.

## Deterministic parallel rules

- each parallel phase writes only its assigned particle or cell range;
- shared floating-point reductions happen later in ascending index order;
- spawning, compaction, and event-vector writes remain serial unless explicitly partitioned;
- one phase completes before buffers are swapped;
- fast-math and floating-point contraction are disabled by the build.

`Fixed32` is signed Q16.16 with saturating arithmetic. The strict deterministic scene stores authoritative kinematics in `Fixed32`, uses an explicitly defined PCG32 stream, and hashes fields in a fixed byte order.

## Material grid and hybrid coupling

`MaterialGrid` stores compact cells containing material, variant, and age. Granular and fluid motion use deterministic scan direction and a moved bitmap so a cell cannot update twice in one tick.

The hybrid scene demonstrates both representations in one world:

- dynamic particles query liquid and solid cells;
- solid collisions reflect particle velocity;
- sufficiently slow particles deposit into empty grid cells;
- grid tools can emit new ballistic particles;
- particles and cells share bounds, seed, tick, input, and state hashing.

## Native Vulkan boundary

The public Vulkan header exposes `NativeSurface`, not Vulkan or OS types. The descriptor stores only a surface kind and opaque native handles. Platform-specific translation units contain the actual Vulkan WSI calls:

```text
native_surface_win32.cpp  -> vkCreateWin32SurfaceKHR
native_surface_xlib.cpp   -> vkCreateXlibSurfaceKHR
native_surface_macos.mm   -> CAMetalLayer + vkCreateMetalSurfaceEXT
```

Each frame-in-flight owns a command pool, command buffer, image-available semaphore, render-finished semaphore, in-flight fence, mapped storage buffer, and descriptor set. The vertex shader expands one six-vertex quad per item; the fragment shader evaluates shape coverage.

## Extension points

Add a scene by implementing `IScene`, or construct `Simulation` with a custom vector of scene instances. A scene may use `ParticlePool`, `UniformGridIndex`, `MaterialGrid`, fixed-point buffers, or custom data-oriented storage.

Alternate renderers consume `std::span<const RenderItem>`. Audio consumes `std::span<const SimulationEvent>`. Media/capture systems consume the finalized frame or a renderer-owned image. None of these adapters are part of authoritative simulation state.
