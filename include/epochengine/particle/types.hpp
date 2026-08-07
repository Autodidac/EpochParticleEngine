#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace epochengine::particle
{
    struct Vec2
    {
        float x{};
        float y{};

        constexpr Vec2& operator+=(Vec2 rhs) noexcept
        {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        constexpr Vec2& operator-=(Vec2 rhs) noexcept
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }

        constexpr Vec2& operator*=(float scalar) noexcept
        {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        constexpr Vec2& operator/=(float scalar) noexcept
        {
            x /= scalar;
            y /= scalar;
            return *this;
        }
    };

    [[nodiscard]] constexpr Vec2 operator+(Vec2 lhs, Vec2 rhs) noexcept
    {
        return { lhs.x + rhs.x, lhs.y + rhs.y };
    }

    [[nodiscard]] constexpr Vec2 operator-(Vec2 lhs, Vec2 rhs) noexcept
    {
        return { lhs.x - rhs.x, lhs.y - rhs.y };
    }

    [[nodiscard]] constexpr Vec2 operator-(Vec2 value) noexcept
    {
        return { -value.x, -value.y };
    }

    [[nodiscard]] constexpr Vec2 operator*(Vec2 value, float scalar) noexcept
    {
        return { value.x * scalar, value.y * scalar };
    }

    [[nodiscard]] constexpr Vec2 operator*(float scalar, Vec2 value) noexcept
    {
        return value * scalar;
    }

    [[nodiscard]] constexpr Vec2 operator/(Vec2 value, float scalar) noexcept
    {
        return { value.x / scalar, value.y / scalar };
    }

    [[nodiscard]] constexpr float dot(Vec2 lhs, Vec2 rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y;
    }

    [[nodiscard]] constexpr float length_squared(Vec2 value) noexcept
    {
        return dot(value, value);
    }

    [[nodiscard]] inline float length(Vec2 value) noexcept
    {
        return std::sqrt(length_squared(value));
    }

    [[nodiscard]] inline Vec2 normalized_or(Vec2 value, Vec2 fallback = { 1.0F, 0.0F }) noexcept
    {
        const float magnitude_squared = length_squared(value);
        if (!std::isfinite(magnitude_squared) || magnitude_squared <= 1.0e-12F)
            return fallback;
        return value / std::sqrt(magnitude_squared);
    }

    [[nodiscard]] constexpr Vec2 perpendicular(Vec2 value) noexcept
    {
        return { -value.y, value.x };
    }

    [[nodiscard]] constexpr Vec2 lerp(Vec2 from, Vec2 to, float fraction) noexcept
    {
        return from + (to - from) * fraction;
    }

    [[nodiscard]] constexpr float clamp(float value, float minimum, float maximum) noexcept
    {
        return value < minimum ? minimum : value > maximum ? maximum : value;
    }

    [[nodiscard]] constexpr Vec2 clamp(Vec2 value, Vec2 minimum, Vec2 maximum) noexcept
    {
        return {
            clamp(value.x, minimum.x, maximum.x),
            clamp(value.y, minimum.y, maximum.y)
        };
    }

    struct Color
    {
        float r{ 1.0F };
        float g{ 1.0F };
        float b{ 1.0F };
        float a{ 1.0F };
    };

    [[nodiscard]] constexpr Color lerp(Color from, Color to, float fraction) noexcept
    {
        return {
            from.r + (to.r - from.r) * fraction,
            from.g + (to.g - from.g) * fraction,
            from.b + (to.b - from.b) * fraction,
            from.a + (to.a - from.a) * fraction
        };
    }

    [[nodiscard]] constexpr Color with_alpha(Color color, float alpha) noexcept
    {
        color.a = alpha;
        return color;
    }

    struct Bounds
    {
        float width{ 1.0F };
        float height{ 1.0F };

        [[nodiscard]] bool valid() const noexcept
        {
            return std::isfinite(width) && std::isfinite(height)
                && width > 0.0F && height > 0.0F;
        }

        [[nodiscard]] constexpr Vec2 center() const noexcept
        {
            return { width * 0.5F, height * 0.5F };
        }

        [[nodiscard]] bool contains(Vec2 point) const noexcept
        {
            return valid() && std::isfinite(point.x) && std::isfinite(point.y)
                && point.x >= 0.0F && point.y >= 0.0F
                && point.x < width && point.y < height;
        }

        [[nodiscard]] constexpr Vec2 clamp_point(Vec2 point, float margin = 0.0F) const noexcept
        {
            return {
                clamp(point.x, margin, std::max(margin, width - margin)),
                clamp(point.y, margin, std::max(margin, height - margin))
            };
        }
    };

    [[nodiscard]] constexpr float radians(float degrees) noexcept
    {
        return degrees * std::numbers::pi_v<float> / 180.0F;
    }

    [[nodiscard]] inline Vec2 rotate(Vec2 value, float angle_radians) noexcept
    {
        const float cosine = std::cos(angle_radians);
        const float sine = std::sin(angle_radians);
        return {
            value.x * cosine - value.y * sine,
            value.x * sine + value.y * cosine
        };
    }
}
