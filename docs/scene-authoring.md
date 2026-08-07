# Scene authoring

Implement `epochengine::particle::IScene` when adding a simulation.

## Required methods

- `info`: stable ID, display name, and concise description.
- `reset`: discard old state and initialize entirely from bounds and seed.
- `update`: advance exactly one fixed tick.
- `render`: append primitives without mutating authoritative state.
- `pointer`: consume already ordered host input.
- `resize`: remap or clamp state explicitly.
- `stats`: expose low-cost diagnostics.
- `state_hash`: hash every authoritative field in stable order.

## Deterministic parallel pattern

Use separate read and write arrays:

```cpp
context.tasks.parallel_for(count, 128, [&](std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
        next_velocity[i] = evaluate_velocity(i, positions, velocities);
    }
});

context.tasks.parallel_for(count, 128, [&](std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
        positions[i] += next_velocity[i] * context.delta_seconds;
    }
});
```

Never push into a shared vector or accumulate into one floating-point value from worker threads. Perform reductions afterward in ascending index order.

## Hashing rules

Include:

- seed and RNG state;
- particle/grid counts;
- every authoritative scalar and enum;
- persistent IDs;
- data in deterministic logical order.

Exclude:

- render-only colors derived from authoritative state;
- temporary acceleration/neighbor buffers when they are fully recomputed;
- frame rate and timing measurements;
- audio/render handles;
- allocation capacity.

## Input rules

Pointer events carry position, button action, held-button state, Shift, and Control. Treat input as a tick-ordered command. Do not read platform state directly from a scene.

## Rendering rules

Use layers deliberately. Stable sorting preserves insertion order within one layer. Keep `RenderItem` counts below the frame cap and use one item per visible cell only when necessary. Large grids should add future tile/texture render paths rather than millions of tiny rectangles.
