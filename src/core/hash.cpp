#include <epochengine/particle/hash.hpp>

#include <bit>
#include <cstdint>

namespace epochengine::particle
{
    void StableHasher::append_byte(std::uint8_t value) noexcept
    {
        hash_ ^= value;
        hash_ *= prime;
    }

    void StableHasher::append_u32(std::uint32_t value) noexcept
    {
        for (unsigned shift = 0; shift < 32U; shift += 8U)
            append_byte(static_cast<std::uint8_t>(value >> shift));
    }

    void StableHasher::append_i32(std::int32_t value) noexcept
    {
        append_u32(std::bit_cast<std::uint32_t>(value));
    }

    void StableHasher::append_u64(std::uint64_t value) noexcept
    {
        for (unsigned shift = 0; shift < 64U; shift += 8U)
            append_byte(static_cast<std::uint8_t>(value >> shift));
    }

    void StableHasher::append_float(float value) noexcept
    {
        append_u32(std::bit_cast<std::uint32_t>(value));
    }

    void StableHasher::append_string(std::string_view value) noexcept
    {
        append_u64(value.size());
        for (const char character : value)
            append_byte(static_cast<std::uint8_t>(character));
    }

    std::uint64_t hash_combine(std::uint64_t lhs, std::uint64_t rhs) noexcept
    {
        rhs += 0x9e3779b97f4a7c15ULL + (lhs << 6U) + (lhs >> 2U);
        rhs ^= rhs >> 30U;
        rhs *= 0xbf58476d1ce4e5b9ULL;
        rhs ^= rhs >> 27U;
        rhs *= 0x94d049bb133111ebULL;
        rhs ^= rhs >> 31U;
        return lhs ^ rhs;
    }
}
