#pragma once

#include "export.hpp"
#include "hash.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace epochengine::particle
{
    enum ParticleFlags : std::uint32_t
    {
        particle_none = 0,
        particle_dead = 1U << 0U,
        particle_collides_with_grid = 1U << 1U,
        particle_can_deposit = 1U << 2U,
        particle_additive = 1U << 3U
    };

    struct ParticleSpawn
    {
        Vec2 position{};
        Vec2 velocity{};
        Color color{};
        float radius{ 2.0F };
        float lifetime{ -1.0F };
        float inverse_mass{ 1.0F };
        std::uint32_t species{};
        std::uint32_t flags{ particle_none };
    };

    class EPOCH_PARTICLE_API ParticlePool
    {
    public:
        explicit ParticlePool(std::size_t reserve_count = 0);

        void clear() noexcept;
        void reserve(std::size_t count);
        std::size_t spawn(const ParticleSpawn& particle);
        void mark_dead(std::size_t index) noexcept;
        void age(float delta_seconds) noexcept;
        void compact_stable();

        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] std::span<Vec2> positions() noexcept;
        [[nodiscard]] std::span<const Vec2> positions() const noexcept;
        [[nodiscard]] std::span<Vec2> velocities() noexcept;
        [[nodiscard]] std::span<const Vec2> velocities() const noexcept;
        [[nodiscard]] std::span<Color> colors() noexcept;
        [[nodiscard]] std::span<const Color> colors() const noexcept;
        [[nodiscard]] std::span<float> radii() noexcept;
        [[nodiscard]] std::span<const float> radii() const noexcept;
        [[nodiscard]] std::span<float> lifetimes() noexcept;
        [[nodiscard]] std::span<const float> lifetimes() const noexcept;
        [[nodiscard]] std::span<float> inverse_masses() noexcept;
        [[nodiscard]] std::span<const float> inverse_masses() const noexcept;
        [[nodiscard]] std::span<std::uint32_t> species() noexcept;
        [[nodiscard]] std::span<const std::uint32_t> species() const noexcept;
        [[nodiscard]] std::span<std::uint32_t> flags() noexcept;
        [[nodiscard]] std::span<const std::uint32_t> flags() const noexcept;
        [[nodiscard]] std::span<const std::uint64_t> ids() const noexcept;

        [[nodiscard]] std::uint64_t state_hash() const noexcept;

    private:
        std::vector<Vec2> positions_;
        std::vector<Vec2> velocities_;
        std::vector<Color> colors_;
        std::vector<float> radii_;
        std::vector<float> lifetimes_;
        std::vector<float> inverse_masses_;
        std::vector<std::uint32_t> species_;
        std::vector<std::uint32_t> flags_;
        std::vector<std::uint64_t> ids_;
        std::uint64_t next_id_{ 1 };
    };
}
