#include <epochengine/particle/fixed.hpp>

#include <cmath>
#include <cstdint>
#include <limits>

namespace epochengine::particle
{
    Fixed32 Fixed32::from_float(float value) noexcept
    {
        if (std::isnan(value))
            return {};
        if (value == std::numeric_limits<float>::infinity())
            return from_raw(std::numeric_limits<storage_type>::max());
        if (value == -std::numeric_limits<float>::infinity())
            return from_raw(std::numeric_limits<storage_type>::min());

        const double scaled = static_cast<double>(value) * static_cast<double>(one_raw);
        const double minimum = static_cast<double>(std::numeric_limits<storage_type>::min());
        const double maximum = static_cast<double>(std::numeric_limits<storage_type>::max());
        const double clamped = std::clamp(scaled, minimum, maximum);
        return from_raw(static_cast<storage_type>(std::llround(clamped)));
    }

    float Fixed32::to_float() const noexcept
    {
        return static_cast<float>(raw_) / static_cast<float>(one_raw);
    }
}
