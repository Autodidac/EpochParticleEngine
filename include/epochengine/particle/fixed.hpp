#pragma once

#include "export.hpp"

#include <algorithm>
#include <bit>
#include <compare>
#include <cstdint>
#include <limits>

namespace epochengine::particle
{
    class EPOCH_PARTICLE_API Fixed32
    {
    public:
        using storage_type = std::int32_t;
        static constexpr int fractional_bits = 16;
        static constexpr storage_type one_raw = storage_type{ 1 } << fractional_bits;

        constexpr Fixed32() noexcept = default;

        [[nodiscard]] static constexpr Fixed32 from_raw(storage_type raw) noexcept
        {
            Fixed32 value;
            value.raw_ = raw;
            return value;
        }

        [[nodiscard]] static constexpr Fixed32 from_integer(std::int32_t value) noexcept
        {
            return from_raw(saturate(static_cast<std::int64_t>(value) * one_raw));
        }

        [[nodiscard]] static constexpr Fixed32 from_ratio(
            std::int32_t numerator,
            std::int32_t denominator) noexcept
        {
            if (denominator == 0)
                return {};
            return from_raw(saturate(
                (static_cast<std::int64_t>(numerator) * one_raw) / denominator));
        }

        [[nodiscard]] static Fixed32 from_float(float value) noexcept;

        [[nodiscard]] constexpr storage_type raw() const noexcept
        {
            return raw_;
        }

        [[nodiscard]] constexpr std::int32_t floor_to_integer() const noexcept
        {
            const std::int32_t quotient = raw_ / one_raw;
            const std::int32_t remainder = raw_ % one_raw;
            return raw_ < 0 && remainder != 0 ? quotient - 1 : quotient;
        }

        [[nodiscard]] float to_float() const noexcept;

        constexpr Fixed32& operator+=(Fixed32 rhs) noexcept
        {
            raw_ = saturate(static_cast<std::int64_t>(raw_) + rhs.raw_);
            return *this;
        }

        constexpr Fixed32& operator-=(Fixed32 rhs) noexcept
        {
            raw_ = saturate(static_cast<std::int64_t>(raw_) - rhs.raw_);
            return *this;
        }

        constexpr Fixed32& operator*=(Fixed32 rhs) noexcept
        {
            const std::int64_t product = static_cast<std::int64_t>(raw_) * rhs.raw_;
            raw_ = saturate(floor_divide(product, one_raw));
            return *this;
        }

        constexpr Fixed32& operator/=(Fixed32 rhs) noexcept
        {
            if (rhs.raw_ == 0)
            {
                raw_ = 0;
                return *this;
            }
            const std::int64_t numerator = static_cast<std::int64_t>(raw_) * one_raw;
            raw_ = saturate(numerator / rhs.raw_);
            return *this;
        }

        [[nodiscard]] friend constexpr Fixed32 operator+(Fixed32 lhs, Fixed32 rhs) noexcept
        {
            lhs += rhs;
            return lhs;
        }

        [[nodiscard]] friend constexpr Fixed32 operator-(Fixed32 lhs, Fixed32 rhs) noexcept
        {
            lhs -= rhs;
            return lhs;
        }

        [[nodiscard]] friend constexpr Fixed32 operator-(Fixed32 value) noexcept
        {
            return from_raw(value.raw_ == std::numeric_limits<storage_type>::min()
                ? std::numeric_limits<storage_type>::max()
                : -value.raw_);
        }

        [[nodiscard]] friend constexpr Fixed32 operator*(Fixed32 lhs, Fixed32 rhs) noexcept
        {
            lhs *= rhs;
            return lhs;
        }

        [[nodiscard]] friend constexpr Fixed32 operator/(Fixed32 lhs, Fixed32 rhs) noexcept
        {
            lhs /= rhs;
            return lhs;
        }

        [[nodiscard]] friend constexpr auto operator<=>(Fixed32, Fixed32) noexcept = default;

    private:
        [[nodiscard]] static constexpr std::int64_t floor_divide(
            std::int64_t numerator,
            std::int64_t denominator) noexcept
        {
            const std::int64_t quotient = numerator / denominator;
            const std::int64_t remainder = numerator % denominator;
            return numerator < 0 && remainder != 0 ? quotient - 1 : quotient;
        }

        [[nodiscard]] static constexpr storage_type saturate(std::int64_t value) noexcept
        {
            return static_cast<storage_type>(std::clamp(
                value,
                static_cast<std::int64_t>(std::numeric_limits<storage_type>::min()),
                static_cast<std::int64_t>(std::numeric_limits<storage_type>::max())));
        }

        storage_type raw_{};
    };

    struct FixedVec2
    {
        Fixed32 x{};
        Fixed32 y{};

        constexpr FixedVec2& operator+=(FixedVec2 rhs) noexcept
        {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        constexpr FixedVec2& operator-=(FixedVec2 rhs) noexcept
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }
    };

    [[nodiscard]] constexpr FixedVec2 operator+(FixedVec2 lhs, FixedVec2 rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    [[nodiscard]] constexpr FixedVec2 operator-(FixedVec2 lhs, FixedVec2 rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    [[nodiscard]] constexpr FixedVec2 operator*(FixedVec2 value, Fixed32 scalar) noexcept
    {
        return { value.x * scalar, value.y * scalar };
    }
}
