#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/particle_pool.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>

namespace epochengine::particle::scenes
{
    namespace
    {
        class GalaxyScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "galaxy",
                    .name = "Orbital Galaxy",
                    .description = "Thousands of orbiting particles around a moving binary gravity field."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                pointer_repels_ = false;
                average_speed_ = 0.0;
                pool_.clear();
                pool_.reserve(particle_count_);

                random_.reseed(seed_);
                const Vec2 center = bounds_.center();
                const float maximum_radius =
                    std::max(24.0F, std::min(bounds_.width, bounds_.height) * 0.46F);

                for (std::size_t index = 0; index < particle_count_; ++index)
                {
                    const float normalized_radius = std::sqrt(random_.unit_float());
                    const float radius = 18.0F + normalized_radius * maximum_radius;
                    const float arm = static_cast<float>(index % 4U)
                        * std::numbers::pi_v<float> * 0.5F;
                    const float angle = arm
                        + normalized_radius * std::numbers::pi_v<float> * 5.5F
                        + random_.range(-0.32F, 0.32F);
                    const Vec2 radial{ std::cos(angle), std::sin(angle) };
                    const Vec2 tangent = perpendicular(radial);
                    const float orbital_speed =
                        65.0F + 95.0F * (1.0F - normalized_radius * 0.55F);
                    const std::uint32_t species =
                        static_cast<std::uint32_t>((index / 173U) % 6U);
                    Color color = lerp(
                        detail::species_color(species),
                        Color{ 0.82F, 0.90F, 1.0F, 1.0F },
                        normalized_radius * 0.35F);

                    pool_.spawn({
                        .position = center + radial * radius
                            + perpendicular(radial) * random_.range(-6.0F, 6.0F),
                        .velocity = tangent * orbital_speed
                            + radial * random_.range(-8.0F, 8.0F),
                        .color = color,
                        .radius = random_.range(1.0F, 2.2F),
                        .lifetime = -1.0F,
                        .inverse_mass = 1.0F,
                        .species = species,
                        .flags = particle_additive
                    });
                }
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                const float phase = static_cast<float>(context.tick) * 0.0028F;
                const Vec2 center = bounds_.center();
                const float binary_radius =
                    std::max(12.0F, std::min(bounds_.width, bounds_.height) * 0.035F);
                attractor_a_ = center + Vec2{ std::cos(phase), std::sin(phase) } * binary_radius;
                attractor_b_ = center - Vec2{ std::cos(phase), std::sin(phase) } * binary_radius;

                auto positions = pool_.positions();
                auto velocities = pool_.velocities();
                const float delta_seconds = context.delta_seconds;

                context.tasks.parallel_for(
                    pool_.size(),
                    128U,
                    [this, positions, velocities, delta_seconds](
                        std::size_t begin,
                        std::size_t end)
                    {
                        for (std::size_t index = begin; index < end; ++index)
                        {
                            Vec2 acceleration = gravity_from(
                                positions[index],
                                attractor_a_,
                                gravity_strength_);
                            acceleration += gravity_from(
                                positions[index],
                                attractor_b_,
                                gravity_strength_ * 0.82F);

                            if (pointer_active_)
                            {
                                Vec2 delta = pointer_position_ - positions[index];
                                const float distance_squared =
                                    length_squared(delta) + pointer_softening_;
                                const float inverse_distance = 1.0F / std::sqrt(distance_squared);
                                const float sign = pointer_repels_ ? -1.0F : 1.0F;
                                acceleration += delta
                                    * (sign * pointer_strength_
                                        * inverse_distance
                                        / distance_squared);
                            }

                            velocities[index] += acceleration * delta_seconds;
                            velocities[index] *= 0.9997F;
                            positions[index] += velocities[index] * delta_seconds;

                            if (positions[index].x < -80.0F
                                || positions[index].x > bounds_.width + 80.0F
                                || positions[index].y < -80.0F
                                || positions[index].y > bounds_.height + 80.0F)
                            {
                                positions[index] = detail::wrapped(
                                    positions[index],
                                    bounds_);
                                velocities[index] *= 0.75F;
                            }
                        }
                    });

                average_speed_ = 0.0;
                for (const Vec2 velocity : velocities)
                    average_speed_ += static_cast<double>(length(velocity));
                if (!velocities.empty())
                    average_speed_ /= static_cast<double>(velocities.size());
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                const auto colors = pool_.colors();
                const auto radii = pool_.radii();

                for (std::size_t index = 0; index < pool_.size(); ++index)
                {
                    frame.line(
                        positions[index],
                        positions[index] - velocities[index] * 0.022F,
                        0.75F,
                        with_alpha(colors[index], 0.28F),
                        1);
                    frame.circle(
                        positions[index],
                        radii[index],
                        with_alpha(colors[index], 0.86F),
                        2);
                }

                frame.circle(
                    attractor_a_,
                    7.0F,
                    { 1.0F, 0.82F, 0.34F, 1.0F },
                    4);
                frame.circle(
                    attractor_b_,
                    5.5F,
                    { 0.52F, 0.70F, 1.0F, 1.0F },
                    4);
                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        18.0F,
                        pointer_repels_
                            ? Color{ 1.0F, 0.18F, 0.22F, 0.42F }
                            : Color{ 0.34F, 0.78F, 1.0F, 0.42F },
                        3);
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
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{
                    .particle_count = pool_.size(),
                    .active_cell_count = 0
                };
                result.metrics[0] = { "AVERAGE SPEED", average_speed_ };
                result.metrics[1] = {
                    "GRAVITY",
                    static_cast<double>(gravity_strength_)
                };
                result.metrics[2] = { "ATTRACTORS", 2.0 };
                result.metric_count = 3;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(random_.state());
                hasher.append_u64(pool_.state_hash());
                hasher.append_float(attractor_a_.x);
                hasher.append_float(attractor_a_.y);
                hasher.append_float(attractor_b_.x);
                hasher.append_float(attractor_b_.y);
                return hasher.value();
            }

        private:
            [[nodiscard]] static Vec2 gravity_from(
                Vec2 position,
                Vec2 source,
                float strength) noexcept
            {
                const Vec2 delta = source - position;
                const float distance_squared = length_squared(delta) + softening_;
                const float inverse_distance = 1.0F / std::sqrt(distance_squared);
                return delta * (strength * inverse_distance / distance_squared);
            }

            static constexpr std::size_t particle_count_ = 5'800;
            static constexpr float gravity_strength_ = 1'420'000.0F;
            static constexpr float pointer_strength_ = 2'800'000.0F;
            static constexpr float softening_ = 900.0F;
            static constexpr float pointer_softening_ = 400.0F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            Pcg32 random_{};
            ParticlePool pool_{ particle_count_ };
            Vec2 attractor_a_{};
            Vec2 attractor_b_{};
            Vec2 pointer_position_{};
            double average_speed_{};
            bool pointer_active_{};
            bool pointer_repels_{};
        };
    }

    std::unique_ptr<IScene> make_galaxy_scene()
    {
        return std::make_unique<GalaxyScene>();
    }
}
