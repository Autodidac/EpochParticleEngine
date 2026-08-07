#include <epochengine/particle/particle_pool.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace epochengine::particle
{
    ParticlePool::ParticlePool(std::size_t reserve_count)
    {
        reserve(reserve_count);
    }

    void ParticlePool::clear() noexcept
    {
        positions_.clear();
        velocities_.clear();
        colors_.clear();
        radii_.clear();
        lifetimes_.clear();
        inverse_masses_.clear();
        species_.clear();
        flags_.clear();
        ids_.clear();
        next_id_ = 1;
    }

    void ParticlePool::reserve(std::size_t count)
    {
        positions_.reserve(count);
        velocities_.reserve(count);
        colors_.reserve(count);
        radii_.reserve(count);
        lifetimes_.reserve(count);
        inverse_masses_.reserve(count);
        species_.reserve(count);
        flags_.reserve(count);
        ids_.reserve(count);
    }

    std::size_t ParticlePool::spawn(const ParticleSpawn& particle)
    {
        if (next_id_ == 0)
            throw std::overflow_error("ParticlePool exhausted its 64-bit particle IDs");

        const std::size_t index = positions_.size();
        const std::array capacities{
            positions_.capacity(), velocities_.capacity(), colors_.capacity(),
            radii_.capacity(), lifetimes_.capacity(), inverse_masses_.capacity(),
            species_.capacity(), flags_.capacity(), ids_.capacity()
        };
        const bool needs_capacity = std::ranges::any_of(
            capacities,
            [index](std::size_t capacity) { return capacity <= index; });
        if (needs_capacity)
        {
            const std::size_t current_capacity = *std::ranges::max_element(capacities);
            const std::size_t maximum = std::numeric_limits<std::size_t>::max();
            const std::size_t growth = current_capacity > (maximum - 1U) / 2U
                ? maximum
                : current_capacity + current_capacity / 2U + 1U;
            const std::size_t requested = std::max({
                index + 1U,
                growth,
                std::size_t{ 8 }
            });
            reserve(requested);
        }

        // Capacity is guaranteed before any size changes. These trivial insertions cannot
        // allocate, so the structure-of-arrays invariant survives allocation failure.
        positions_.push_back(particle.position);
        velocities_.push_back(particle.velocity);
        colors_.push_back(particle.color);
        radii_.push_back(std::max(particle.radius, 0.0F));
        lifetimes_.push_back(particle.lifetime);
        inverse_masses_.push_back(std::max(particle.inverse_mass, 0.0F));
        species_.push_back(particle.species);
        flags_.push_back(particle.flags);
        ids_.push_back(next_id_);
        ++next_id_;
        return index;
    }

    void ParticlePool::mark_dead(std::size_t index) noexcept
    {
        if (index < flags_.size())
            flags_[index] |= particle_dead;
    }

    void ParticlePool::age(float delta_seconds) noexcept
    {
        if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0F)
            return;

        for (std::size_t index = 0; index < lifetimes_.size(); ++index)
        {
            float& lifetime = lifetimes_[index];
            if (lifetime < 0.0F)
                continue;

            lifetime -= delta_seconds;
            if (lifetime <= 0.0F)
                flags_[index] |= particle_dead;
        }
    }

    void ParticlePool::compact_stable()
    {
        std::size_t write = 0;
        for (std::size_t read = 0; read < size(); ++read)
        {
            if ((flags_[read] & particle_dead) != 0U)
                continue;

            if (write != read)
            {
                positions_[write] = positions_[read];
                velocities_[write] = velocities_[read];
                colors_[write] = colors_[read];
                radii_[write] = radii_[read];
                lifetimes_[write] = lifetimes_[read];
                inverse_masses_[write] = inverse_masses_[read];
                species_[write] = species_[read];
                flags_[write] = flags_[read];
                ids_[write] = ids_[read];
            }
            ++write;
        }

        positions_.resize(write);
        velocities_.resize(write);
        colors_.resize(write);
        radii_.resize(write);
        lifetimes_.resize(write);
        inverse_masses_.resize(write);
        species_.resize(write);
        flags_.resize(write);
        ids_.resize(write);
    }

    std::size_t ParticlePool::size() const noexcept { return positions_.size(); }
    bool ParticlePool::empty() const noexcept { return positions_.empty(); }

    std::span<Vec2> ParticlePool::positions() noexcept { return positions_; }
    std::span<const Vec2> ParticlePool::positions() const noexcept { return positions_; }
    std::span<Vec2> ParticlePool::velocities() noexcept { return velocities_; }
    std::span<const Vec2> ParticlePool::velocities() const noexcept { return velocities_; }
    std::span<Color> ParticlePool::colors() noexcept { return colors_; }
    std::span<const Color> ParticlePool::colors() const noexcept { return colors_; }
    std::span<float> ParticlePool::radii() noexcept { return radii_; }
    std::span<const float> ParticlePool::radii() const noexcept { return radii_; }
    std::span<float> ParticlePool::lifetimes() noexcept { return lifetimes_; }
    std::span<const float> ParticlePool::lifetimes() const noexcept { return lifetimes_; }
    std::span<float> ParticlePool::inverse_masses() noexcept { return inverse_masses_; }
    std::span<const float> ParticlePool::inverse_masses() const noexcept { return inverse_masses_; }
    std::span<std::uint32_t> ParticlePool::species() noexcept { return species_; }
    std::span<const std::uint32_t> ParticlePool::species() const noexcept { return species_; }
    std::span<std::uint32_t> ParticlePool::flags() noexcept { return flags_; }
    std::span<const std::uint32_t> ParticlePool::flags() const noexcept { return flags_; }
    std::span<const std::uint64_t> ParticlePool::ids() const noexcept { return ids_; }

    std::uint64_t ParticlePool::state_hash() const noexcept
    {
        StableHasher hasher;
        hasher.append_u64(size());
        hasher.append_u64(next_id_);

        for (std::size_t index = 0; index < size(); ++index)
        {
            hasher.append_float(positions_[index].x);
            hasher.append_float(positions_[index].y);
            hasher.append_float(velocities_[index].x);
            hasher.append_float(velocities_[index].y);
            hasher.append_float(colors_[index].r);
            hasher.append_float(colors_[index].g);
            hasher.append_float(colors_[index].b);
            hasher.append_float(colors_[index].a);
            hasher.append_float(radii_[index]);
            hasher.append_float(lifetimes_[index]);
            hasher.append_float(inverse_masses_[index]);
            hasher.append_u32(species_[index]);
            hasher.append_u32(flags_[index]);
            hasher.append_u64(ids_[index]);
        }
        return hasher.value();
    }
}
