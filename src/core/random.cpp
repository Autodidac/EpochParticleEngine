#include <epochengine/particle/random.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace epochengine::particle
{
    void Pcg32::reseed(std::uint64_t seed, std::uint64_t sequence) noexcept
    {
        state_ = 0;
        increment_ = (sequence << 1U) | 1U;
        static_cast<void>(next_u32());
        state_ += seed;
        static_cast<void>(next_u32());
    }

    std::uint32_t Pcg32::next_u32() noexcept
    {
        const std::uint64_t old_state = state_;
        state_ = old_state * 6364136223846793005ULL + increment_;
        const std::uint32_t xor_shifted =
            static_cast<std::uint32_t>(((old_state >> 18U) ^ old_state) >> 27U);
        const std::uint32_t rotation = static_cast<std::uint32_t>(old_state >> 59U);
        return (xor_shifted >> rotation)
            | (xor_shifted << ((0U - rotation) & 31U));
    }

    std::uint32_t Pcg32::bounded(std::uint32_t exclusive_upper_bound) noexcept
    {
        if (exclusive_upper_bound == 0)
            return 0;

        const std::uint32_t threshold =
            (std::uint32_t{ 0 } - exclusive_upper_bound) % exclusive_upper_bound;
        for (;;)
        {
            const std::uint32_t value = next_u32();
            if (value >= threshold)
                return value % exclusive_upper_bound;
        }
    }

    float Pcg32::unit_float() noexcept
    {
        return static_cast<float>(next_u32() >> 8U) * (1.0F / 16'777'216.0F);
    }

    float Pcg32::range(float minimum, float maximum) noexcept
    {
        if (!std::isfinite(minimum))
            return 0.0F;
        if (!std::isfinite(maximum) || maximum <= minimum)
            return minimum;
        return minimum + (maximum - minimum) * unit_float();
    }

    std::int32_t Pcg32::range(
        std::int32_t minimum,
        std::int32_t maximum_exclusive) noexcept
    {
        if (maximum_exclusive <= minimum)
            return minimum;

        const auto span = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(static_cast<std::int64_t>(maximum_exclusive)
                - static_cast<std::int64_t>(minimum)));
        const std::int64_t result = static_cast<std::int64_t>(minimum)
            + static_cast<std::int64_t>(bounded(span));
        return static_cast<std::int32_t>(result);
    }
}
