#pragma once

#include "types.hpp"

#include <cstdint>

namespace epochengine::particle
{
    enum class SimulationEventType : std::uint8_t
    {
        particle_emitted,
        collision,
        explosion,
        ignition,
        material_deposited
    };

    struct SimulationEvent
    {
        SimulationEventType type{};
        Vec2 position{};
        float intensity{ 1.0F };
        std::uint64_t source_id{};
    };
}
