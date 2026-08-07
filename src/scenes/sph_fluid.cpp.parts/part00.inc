#include "../scene_common.hpp"
#include "../scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/particle_pool.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>
#include <epochengine/particle/uniform_grid.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        class SphFluidScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "sph-fluid",
                    .name = "SPH Fluid",
                    .description = "Uniform-grid smoothed particle hydrodynamics with pressure and viscosity."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                pointer_repels_ = false;
                pool_.clear();
                pool_.reserve(maximum_particles_);

                Pcg32 random(seed_);
                const float spacing = 8.5F;
                const std::size_t columns = 36;
                const std::size_t rows = 28;
                const Vec2 origin{
                    std::max(28.0F, bounds_.width * 0.07F),
                    std::max(28.0F, bounds_.height * 0.18F)
                };

                for (std::size_t row = 0; row < rows; ++row)
                {
                    for (std::size_t column = 0; column < columns; ++column)
                    {
                        const float stagger = (row & 1U) != 0U ? spacing * 0.5F : 0.0F;
                        const Vec2 position{
                            origin.x + static_cast<float>(column) * spacing + stagger
                                + random.range(-0.35F, 0.35F),
                            origin.y + static_cast<float>(row) * spacing
                                + random.range(-0.35F, 0.35F)
                        };
                        pool_.spawn({
                            .position = position,
                            .velocity = {},
                            .color = { 0.12F, 0.52F, 1.0F, 0.92F },
                            .radius = particle_radius_,
                            .lifetime = -1.0F,
                            .inverse_mass = 1.0F,
                            .species = 0,
                            .flags = particle_none
                        });
                    }
                }

                resize_working_sets();
                grid_.configure(bounds_, smoothing_radius_);
                grid_.build(pool_.positions());
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                grid_.configure(bounds_, smoothing_radius_);
                grid_.build(pool_.positions());
                resize_working_sets();

                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                const std::size_t count = pool_.size();

                context.tasks.parallel_for(
                    count,
                    64U,
                    [this, positions](std::size_t begin, std::size_t end)
                    {
                        for (std::size_t particle = begin; particle < end; ++particle)
                        {
                            float density = self_density_;
                            std::uint32_t neighbors = 0;
                            grid_.for_each_neighbor(
                                positions[particle],
                                smoothing_radius_,
                                false,
                                [&](std::uint32_t neighbor)
                                {
                                    if (neighbor == particle)
                                        return;
                                    const Vec2 delta =
                                        positions[neighbor] - positions[particle];
                                    const float distance_squared = length_squared(delta);
                                    if (distance_squared >= smoothing_radius_
                                        * smoothing_radius_)
                                    {
                                        return;
                                    }
                                    const float distance = std::sqrt(
                                        std::max(distance_squared, 0.0F));
                                    const float q = 1.0F - distance / smoothing_radius_;
                                    density += q * q;
                                    ++neighbors;
                                });
                            densities_[particle] = std::max(density, 0.1F);
                            neighbor_counts_[particle] = neighbors;
                        }
                    });

                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    pressures_[particle] = std::clamp(
                        (densities_[particle] - rest_density_) * pressure_stiffness_,
                        -minimum_pressure_,
                        maximum_pressure_);
                }

