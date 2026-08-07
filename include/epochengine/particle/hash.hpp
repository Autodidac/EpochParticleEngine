#pragma once

#include "export.hpp"

#include <bit>
#include <cstdint>
#include <string_view>

namespace epochengine::particle
{
    class EPOCH_PARTICLE_API StableHasher
    {
    public:
        void append_byte(std::uint8_t value) noexcept;
        void append_u32(std::uint32_t value) noexcept;
        void append_i32(std::int32_t value) noexcept;
        void append_u64(std::uint64_t value) noexcept;
        void append_float(float value) noexcept;
        void append_string(std::string_view value) noexcept;

        [[nodiscard]] constexpr std::uint64_t value() const noexcept
        {
            return hash_;
        }

    private:
        static constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
        static constexpr std::uint64_t prime = 1099511628211ULL;
        std::uint64_t hash_{ offset_basis };
    };

    [[nodiscard]] EPOCH_PARTICLE_API std::uint64_t hash_combine(
        std::uint64_t lhs,
        std::uint64_t rhs) noexcept;
}
