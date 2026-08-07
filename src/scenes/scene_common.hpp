#pragma once

#include <epochengine/particle/types.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace epochengine::particle::scenes::detail
{
    inline constexpr std::array<Color, 8> palette{
        Color{ 0.20F, 0.78F, 1.00F, 1.0F },
        Color{ 1.00F, 0.34F, 0.30F, 1.0F },
        Color{ 0.42F, 0.92F, 0.42F, 1.0F },
        Color{ 0.96F, 0.78F, 0.20F, 1.0F },
        Color{ 0.78F, 0.35F, 1.00F, 1.0F },
        Color{ 1.00F, 0.48F, 0.82F, 1.0F },
        Color{ 0.28F, 0.92F, 0.80F, 1.0F },
        Color{ 1.00F, 0.62F, 0.24F, 1.0F }
    };

    [[nodiscard]] constexpr Color species_color(std::uint32_t species) noexcept
    {
        return palette[species % palette.size()];
    }

    [[nodiscard]] inline Vec2 wrapped(Vec2 position, Bounds bounds) noexcept
    {
        if (bounds.width > 0.0F)
        {
            position.x = std::fmod(position.x, bounds.width);
            if (position.x < 0.0F)
                position.x += bounds.width;
        }
        if (bounds.height > 0.0F)
        {
            position.y = std::fmod(position.y, bounds.height);
            if (position.y < 0.0F)
                position.y += bounds.height;
        }
        return position;
    }

    [[nodiscard]] constexpr Vec2 minimum_image(Vec2 delta, Bounds bounds) noexcept
    {
        if (delta.x > bounds.width * 0.5F)
            delta.x -= bounds.width;
        else if (delta.x < -bounds.width * 0.5F)
            delta.x += bounds.width;

        if (delta.y > bounds.height * 0.5F)
            delta.y -= bounds.height;
        else if (delta.y < -bounds.height * 0.5F)
            delta.y += bounds.height;
        return delta;
    }

    inline void rescale_position(Vec2& position, Bounds old_bounds, Bounds new_bounds) noexcept
    {
        if (old_bounds.width > 0.0F)
            position.x *= new_bounds.width / old_bounds.width;
        if (old_bounds.height > 0.0F)
            position.y *= new_bounds.height / old_bounds.height;
        position = new_bounds.clamp_point(position);
    }

    [[nodiscard]] inline float smooth_pulse(float value) noexcept
    {
        value = std::clamp(value, 0.0F, 1.0F);
        return value * value * (3.0F - 2.0F * value);
    }
}
