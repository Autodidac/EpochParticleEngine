#pragma once

#include <epochengine/particle/scene.hpp>

#include <memory>

namespace epochengine::particle::scenes
{
    [[nodiscard]] std::unique_ptr<IScene> make_particle_studio_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_deterministic_fountain_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_flow_field_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_cellular_automata_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_reaction_diffusion_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_particle_life_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_hybrid_sand_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_fire_smoke_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_fireworks_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_galaxy_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_boids_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_sph_fluid_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_spring_cloth_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_physarum_scene();
    [[nodiscard]] std::unique_ptr<IScene> make_weather_scene();
}
