#include "scene_factories.hpp"

#include <epochengine/particle/scenes.hpp>

#include <memory>
#include <vector>

namespace epochengine::particle
{
    std::vector<std::unique_ptr<IScene>> make_default_scenes()
    {
        std::vector<std::unique_ptr<IScene>> result;
        result.reserve(9);
        result.push_back(scenes::make_deterministic_fountain_scene());
        result.push_back(scenes::make_cellular_automata_scene());
        result.push_back(scenes::make_particle_life_scene());
        result.push_back(scenes::make_hybrid_sand_scene());
        result.push_back(scenes::make_fire_smoke_scene());
        result.push_back(scenes::make_fireworks_scene());
        result.push_back(scenes::make_galaxy_scene());
        result.push_back(scenes::make_boids_scene());
        result.push_back(scenes::make_sph_fluid_scene());
        return result;
    }
}
