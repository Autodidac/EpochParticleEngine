#include "../scene_common.hpp"
#include "../scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/particle_pool.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>
#include <epochengine/particle/uniform_grid.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        class ParticleLifeScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "particle-life",
                    .name = "Particle Life",
                    .description = "Species attraction matrix with a deterministic CSR spatial hash."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                pointer_repels_ = false;
                interaction_count_ = 0;

                Pcg32 random(seed_);
                for (std::size_t row = 0; row < species_count_; ++row)
                {
                    for (std::size_t column = 0; column < species_count_; ++column)
                    {
                        float value = random.range(-1.0F, 1.0F);
                        if (row == column)
                            value = random.range(-0.35F, 0.65F);
                        attraction_[row * species_count_ + column] = value;
                    }
                }

                pool_.clear();
                pool_.reserve(particle_count_);
                for (std::size_t index = 0; index < particle_count_; ++index)
                {
                    const std::uint32_t species =
                        static_cast<std::uint32_t>(index % species_count_);
                    pool_.spawn({
                        .position = {
                            random.range(0.0F, bounds_.width),
                            random.range(0.0F, bounds_.height)
                        },
                        .velocity = {
                            random.range(-8.0F, 8.0F),
                            random.range(-8.0F, 8.0F)
                        },
                        .color = detail::species_color(species),
                        .radius = 2.2F,
                        .lifetime = -1.0F,
                        .inverse_mass = 1.0F,
                        .species = species,
                        .flags = particle_none
                    });
                }

                accelerations_.assign(pool_.size(), {});
                neighbor_counts_.assign(pool_.size(), 0U);
                grid_.configure(bounds_, interaction_radius_);
                grid_.build(pool_.positions());
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                grid_.configure(bounds_, interaction_radius_);
                grid_.build(pool_.positions());

                const std::size_t count = pool_.size();
                accelerations_.resize(count);
                neighbor_counts_.resize(count);

                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                const auto species = pool_.species();
                const float delta_seconds = context.delta_seconds;

                context.tasks.parallel_for(
                    count,
                    64U,
                    [this, positions, species](std::size_t begin, std::size_t end)
                    {
                        for (std::size_t particle = begin; particle < end; ++particle)
                        {
                            Vec2 force{};
                            std::uint32_t neighbor_count = 0;
                            const Vec2 position = positions[particle];
                            const std::uint32_t own_species = species[particle];

                            grid_.for_each_neighbor(
                                position,
                                interaction_radius_,
                                true,
                                [&](std::uint32_t neighbor)
                                {
                                    if (neighbor == particle)
                                        return;

                                    Vec2 delta = detail::minimum_image(
                                        positions[neighbor] - position,
                                        bounds_);
                                    const float distance_squared = length_squared(delta);
                                    if (distance_squared <= 1.0e-8F
                                        || distance_squared >= interaction_radius_
                                            * interaction_radius_)
                                    {
                                        return;
                                    }

                                    const float distance = std::sqrt(distance_squared);
                                    const Vec2 direction = delta / distance;
                                    float strength{};
                                    if (distance < repulsion_radius_)
                                    {
                                        strength = -2.4F
                                            * (1.0F - distance / repulsion_radius_);
                                    }
                                    else
                                    {
                                        const float phase =
                                            (distance - repulsion_radius_)
                                            / (interaction_radius_ - repulsion_radius_);
                                        const float envelope = std::sin(
                                            phase * std::numbers::pi_v<float>);
                                        strength = attraction_[
                                            own_species * species_count_
                                            + species[neighbor]] * envelope;
                                    }

