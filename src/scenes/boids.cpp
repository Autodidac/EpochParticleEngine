#include "scene_common.hpp"
#include "scene_factories.hpp"

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
#include <numbers>
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        class BoidsScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "boids",
                    .name = "Boids Flocking",
                    .description = "Spatially indexed alignment, cohesion and separation steering."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                pointer_repels_ = false;
                pool_.clear();
                pool_.reserve(boid_count_);

                Pcg32 random(seed_);
                for (std::size_t index = 0; index < boid_count_; ++index)
                {
                    const float angle = random.range(
                        0.0F,
                        std::numbers::pi_v<float> * 2.0F);
                    const float speed = random.range(minimum_speed_, maximum_speed_);
                    const std::uint32_t species =
                        static_cast<std::uint32_t>((index / 200U) % 6U);
                    pool_.spawn({
                        .position = {
                            random.range(0.0F, bounds_.width),
                            random.range(0.0F, bounds_.height)
                        },
                        .velocity = {
                            std::cos(angle) * speed,
                            std::sin(angle) * speed
                        },
                        .color = detail::species_color(species),
                        .radius = 2.0F,
                        .lifetime = -1.0F,
                        .inverse_mass = 1.0F,
                        .species = species,
                        .flags = particle_none
                    });
                }

                steering_.assign(pool_.size(), {});
                neighbor_counts_.assign(pool_.size(), 0U);
                grid_.configure(bounds_, perception_radius_);
                grid_.build(pool_.positions());
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                grid_.configure(bounds_, perception_radius_);
                grid_.build(pool_.positions());

                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                steering_.resize(pool_.size());
                neighbor_counts_.resize(pool_.size());

                context.tasks.parallel_for(
                    pool_.size(),
                    64U,
                    [this, positions, velocities](std::size_t begin, std::size_t end)
                    {
                        for (std::size_t index = begin; index < end; ++index)
                        {
                            Vec2 alignment{};
                            Vec2 cohesion{};
                            Vec2 separation{};
                            std::uint32_t neighbor_count = 0;

                            grid_.for_each_neighbor(
                                positions[index],
                                perception_radius_,
                                true,
                                [&](std::uint32_t neighbor)
                                {
                                    if (neighbor == index)
                                        return;

                                    const Vec2 delta = detail::minimum_image(
                                        positions[neighbor] - positions[index],
                                        bounds_);
                                    const float distance_squared = length_squared(delta);
                                    if (distance_squared <= 1.0e-5F
                                        || distance_squared > perception_radius_
                                            * perception_radius_)
                                    {
                                        return;
                                    }

                                    alignment += velocities[neighbor];
                                    cohesion += delta;
                                    if (distance_squared
                                        < separation_radius_ * separation_radius_)
                                    {
                                        separation -= delta / std::max(distance_squared, 4.0F);
                                    }
                                    ++neighbor_count;
                                });

                            Vec2 steering{};
                            if (neighbor_count != 0U)
                            {
                                const float inverse_count =
                                    1.0F / static_cast<float>(neighbor_count);
                                const Vec2 desired_alignment = normalized_or(
                                    alignment * inverse_count,
                                    normalized_or(velocities[index]))
                                    * maximum_speed_;
                                const Vec2 desired_cohesion = normalized_or(
                                    cohesion * inverse_count,
                                    normalized_or(velocities[index]))
                                    * maximum_speed_;
                                const Vec2 desired_separation = normalized_or(
                                    separation,
                                    normalized_or(velocities[index]))
                                    * maximum_speed_;

                                steering += (desired_alignment - velocities[index])
                                    * alignment_weight_;
                                steering += (desired_cohesion - velocities[index])
                                    * cohesion_weight_;
                                steering += (desired_separation - velocities[index])
                                    * separation_weight_;
                            }

                            if (pointer_active_)
                            {
                                Vec2 delta = detail::minimum_image(
                                    pointer_position_ - positions[index],
                                    bounds_);
                                const float distance_squared = length_squared(delta);
                                if (distance_squared > 1.0F
                                    && distance_squared < pointer_radius_ * pointer_radius_)
                                {
                                    const float distance = std::sqrt(distance_squared);
                                    const float sign = pointer_repels_ ? -1.0F : 1.0F;
                                    steering += delta / distance
                                        * sign * pointer_force_
                                        * (1.0F - distance / pointer_radius_);
                                }
                            }

                            const float magnitude_squared = length_squared(steering);
                            if (magnitude_squared > maximum_force_ * maximum_force_)
                                steering = normalized_or(steering) * maximum_force_;

                            steering_[index] = steering;
                            neighbor_counts_[index] = neighbor_count;
                        }
                    });

                auto mutable_positions = pool_.positions();
                auto mutable_velocities = pool_.velocities();
                const float delta_seconds = context.delta_seconds;
                context.tasks.parallel_for(
                    pool_.size(),
                    128U,
                    [this, mutable_positions, mutable_velocities, delta_seconds](
                        std::size_t begin,
                        std::size_t end)
                    {
                        for (std::size_t index = begin; index < end; ++index)
                        {
                            Vec2 velocity =
                                mutable_velocities[index] + steering_[index] * delta_seconds;
                            const float speed = length(velocity);
                            if (speed > maximum_speed_)
                                velocity = velocity / speed * maximum_speed_;
                            else if (speed < minimum_speed_)
                                velocity = normalized_or(velocity) * minimum_speed_;

                            mutable_velocities[index] = velocity;
                            mutable_positions[index] = detail::wrapped(
                                mutable_positions[index] + velocity * delta_seconds,
                                bounds_);
                        }
                    });

                average_neighbors_ = 0.0;
                average_speed_ = 0.0;
                for (std::size_t index = 0; index < pool_.size(); ++index)
                {
                    average_neighbors_ += neighbor_counts_[index];
                    average_speed_ += static_cast<double>(length(mutable_velocities[index]));
                }
                if (!pool_.empty())
                {
                    average_neighbors_ /= static_cast<double>(pool_.size());
                    average_speed_ /= static_cast<double>(pool_.size());
                }
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                const auto colors = pool_.colors();

                for (std::size_t index = 0; index < pool_.size(); ++index)
                {
                    const Vec2 forward = normalized_or(velocities[index]);
                    const Vec2 side = perpendicular(forward);
                    const Vec2 nose = positions[index] + forward * 6.0F;
                    const Vec2 tail = positions[index] - forward * 4.0F;
                    frame.line(
                        nose,
                        tail + side * 3.2F,
                        1.35F,
                        colors[index],
                        2);
                    frame.line(
                        nose,
                        tail - side * 3.2F,
                        1.35F,
                        colors[index],
                        2);
                }

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        pointer_radius_,
                        pointer_repels_
                            ? Color{ 1.0F, 0.24F, 0.24F, 0.10F }
                            : Color{ 0.24F, 0.72F, 1.0F, 0.10F },
                        1);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                pointer_position_ = event.position;
                pointer_active_ = event.primary_down || event.secondary_down;
                pointer_repels_ = event.secondary_down;
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                for (Vec2& position : pool_.positions())
                    detail::rescale_position(position, old_bounds, new_bounds);
                bounds_ = new_bounds;
                grid_.configure(bounds_, perception_radius_);
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{
                    .particle_count = pool_.size(),
                    .active_cell_count = 0
                };
                result.metrics[0] = { "AVERAGE NEIGHBORS", average_neighbors_ };
                result.metrics[1] = { "AVERAGE SPEED", average_speed_ };
                result.metrics[2] = {
                    "PERCEPTION",
                    static_cast<double>(perception_radius_)
                };
                result.metric_count = 3;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(pool_.state_hash());
                return hasher.value();
            }

        private:
            static constexpr std::size_t boid_count_ = 1'600;
            static constexpr float perception_radius_ = 52.0F;
            static constexpr float separation_radius_ = 17.0F;
            static constexpr float minimum_speed_ = 48.0F;
            static constexpr float maximum_speed_ = 145.0F;
            static constexpr float maximum_force_ = 210.0F;
            static constexpr float alignment_weight_ = 0.62F;
            static constexpr float cohesion_weight_ = 0.32F;
            static constexpr float separation_weight_ = 1.18F;
            static constexpr float pointer_radius_ = 135.0F;
            static constexpr float pointer_force_ = 420.0F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            ParticlePool pool_{ boid_count_ };
            UniformGridIndex grid_{};
            std::vector<Vec2> steering_;
            std::vector<std::uint32_t> neighbor_counts_;
            Vec2 pointer_position_{};
            double average_neighbors_{};
            double average_speed_{};
            bool pointer_active_{};
            bool pointer_repels_{};
        };
    }

    std::unique_ptr<IScene> make_boids_scene()
    {
        return std::make_unique<BoidsScene>();
    }
}
