#pragma once

#include "export.hpp"

#include <cstdint>
#include <limits>

namespace epochengine::particle
{
    class EPOCH_PARTICLE_API Pcg32
    {
    public:
        explicit Pcg32(
            std::uint64_t seed = 0x853c49e6748fea9bULL,
            std::uint64_t sequence = 0xda3e39cb94b95bdbULL) noexcept
        {
            reseed(seed, sequence);
        }

        void reseed(std::uint64_t seed, std::uint64_t sequence = 0xda3e39cb94b95bdbULL) noexcept;

        [[nodiscard]] std::uint32_t next_u32() noexcept;
        [[nodiscard]] std::uint32_t bounded(std::uint32_t exclusive_upper_bound) noexcept;
        [[nodiscard]] float unit_float() noexcept;
        [[nodiscard]] float range(float minimum, float maximum) noexcept;
        [[nodiscard]] std::int32_t range(std::int32_t minimum, std::int32_t maximum_exclusive) noexcept;

        [[nodiscard]] constexpr std::uint64_t state() const noexcept
        {
            return state_;
        }

        [[nodiscard]] constexpr std::uint64_t increment() const noexcept
        {
            return increment_;
        }

    private:
        std::uint64_t state_{};
        std::uint64_t increment_{};
    };

    [[nodiscard]] constexpr std::uint32_t mix_u32(std::uint32_t value) noexcept
    {
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        value ^= value >> 16U;
        return value;
    }

    [[nodiscard]] constexpr std::uint32_t coordinate_hash(
        std::int32_t x,
        std::int32_t y,
        std::uint64_t tick,
        std::uint64_t seed) noexcept
    {
        std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9e3779b9U;
        value ^= static_cast<std::uint32_t>(y) * 0x85ebca6bU;
        value ^= static_cast<std::uint32_t>(tick);
        value ^= static_cast<std::uint32_t>(tick >> 32U) * 0xc2b2ae35U;
        value ^= static_cast<std::uint32_t>(seed);
        value ^= static_cast<std::uint32_t>(seed >> 32U);
        return mix_u32(value);
    }
}
