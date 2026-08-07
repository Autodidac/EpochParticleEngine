#pragma once

#include "export.hpp"
#include "scene.hpp"

#include <memory>
#include <vector>

namespace epochengine::particle
{
    [[nodiscard]] EPOCH_PARTICLE_API std::vector<std::unique_ptr<IScene>> make_default_scenes();
}
