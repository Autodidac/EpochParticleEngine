module;

#include <epochengine/particle/epoch_particle_engine.hpp>

export module epoch.particle;

// ABI-preserving named-module facade over the public header API. The original
// declarations remain attached to the global module; exported using-declarations
// make the same entities visible to importers without creating a second ABI.
export namespace epochengine::particle
{
    using ::epochengine::particle::BitmapTextMetrics;
    using ::epochengine::particle::Bounds;
    using ::epochengine::particle::Color;
    using ::epochengine::particle::ComputeDispatch;
    using ::epochengine::particle::ComputeStatus;
    using ::epochengine::particle::IComputeBackend;
    using ::epochengine::particle::Fixed32;
    using ::epochengine::particle::FixedVec2;
    using ::epochengine::particle::GridCell;
    using ::epochengine::particle::IScene;
    using ::epochengine::particle::Material;
    using ::epochengine::particle::MaterialGrid;
    using ::epochengine::particle::ParticleFlags;
    using ::epochengine::particle::ParticlePool;
    using ::epochengine::particle::ParticleSpawn;
    using ::epochengine::particle::Pcg32;
    using ::epochengine::particle::PointerAction;
    using ::epochengine::particle::PointerButton;
    using ::epochengine::particle::PointerEvent;
    using ::epochengine::particle::PrimitiveShape;
    using ::epochengine::particle::RenderFrame;
    using ::epochengine::particle::RenderItem;
    using ::epochengine::particle::SceneInfo;
    using ::epochengine::particle::SceneMetric;
    using ::epochengine::particle::SceneResetContext;
    using ::epochengine::particle::SceneStats;
    using ::epochengine::particle::SceneUpdateContext;
    using ::epochengine::particle::ScreenOrigin;
    using ::epochengine::particle::ScreenSpaceConvention;
    using ::epochengine::particle::Simulation;
    using ::epochengine::particle::SimulationConfig;
    using ::epochengine::particle::SimulationEvent;
    using ::epochengine::particle::SimulationEventType;
    using ::epochengine::particle::StableHasher;
    using ::epochengine::particle::TaskArena;
    using ::epochengine::particle::TextAlign;
    using ::epochengine::particle::TextSize;
    using ::epochengine::particle::UniformGridIndex;
    using ::epochengine::particle::Vec2;

    using ::epochengine::particle::clamp;
    using ::epochengine::particle::coordinate_hash;
    using ::epochengine::particle::dot;
    using ::epochengine::particle::hash_combine;
    using ::epochengine::particle::length;
    using ::epochengine::particle::length_squared;
    using ::epochengine::particle::lerp;
    using ::epochengine::particle::make_bitmap_text_metrics;
    using ::epochengine::particle::make_default_scenes;
    using ::epochengine::particle::mix_u32;
    using ::epochengine::particle::normalized_or;
    using ::epochengine::particle::operator+;
    using ::epochengine::particle::operator-;
    using ::epochengine::particle::operator*;
    using ::epochengine::particle::operator/;
    using ::epochengine::particle::perpendicular;
    using ::epochengine::particle::radians;
    using ::epochengine::particle::resolved_text_pixel_height;
    using ::epochengine::particle::rotate;
    using ::epochengine::particle::screen_to_vulkan_ndc;
    using ::epochengine::particle::with_alpha;

    using ::epochengine::particle::library_name;
    using ::epochengine::particle::version_major;
    using ::epochengine::particle::version_minor;
    using ::epochengine::particle::version_patch;
    using ::epochengine::particle::version_string;
}
