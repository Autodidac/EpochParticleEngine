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
        class FlowFieldScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "flow-field",
                    .name = "Flow Field",
                    .description = "Thousands of particles advected through a time-varying analytic vector field."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                phase_ = 0.0F;
                pointer_active_ = false;
                pointer_repels_ = false;
                average_speed_ = 0.0;

                Pcg32 random(seed_ ^ 0x464c4f574649454cULL);
                pool_.clear();
                pool_.reserve(particle_count_);
                for (std::size_t index = 0; index < particle_count_; ++index)
                {
                    const std::uint32_t species = static_cast<std::uint32_t>(index % 8U);
                    pool_.spawn({
                        .position = {
                            random.range(0.0F, bounds_.width),
                            random.range(0.0F, bounds_.height)
                        },
                        .velocity = {
                            random.range(-18.0F, 18.0F),
                            random.range(-18.0F, 18.0F)
                        },
                        .color = detail::species_color(species),
                        .radius = 1.55F,
                        .lifetime = -1.0F,
                        .inverse_mass = 1.0F,
                        .species = species,
                        .flags = particle_none
                    });
                }
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                phase_ = std::fmod(
                    phase_ + context.delta_seconds * 0.21F,
                    std::numbers::pi_v<float> * 2.0F);

                auto positions = pool_.positions();
                auto velocities = pool_.velocities();
                const float blend = std::clamp(context.delta_seconds * 3.4F, 0.0F, 1.0F);
                const float safe_width = std::max(bounds_.width, 1.0F);
                const float safe_height = std::max(bounds_.height, 1.0F);
                double speed_sum = 0.0;

                for (std::size_t index = 0; index < positions.size(); ++index)
                {
                    Vec2 position = positions[index];
                    Vec2 velocity = velocities[index];
                    const float normalized_x = position.x / safe_width;
                    const float normalized_y = position.y / safe_height;
                    const float angle =
                        std::sin(normalized_y * 13.0F + phase_ * 2.3F)
                            * 1.65F
                        + std::cos(normalized_x * 9.0F - phase_ * 1.7F)
                            * 1.15F
                        + std::sin((normalized_x + normalized_y) * 7.0F + phase_)
                            * 0.65F;
                    const Vec2 desired{
                        std::cos(angle) * 92.0F,
                        std::sin(angle) * 92.0F
                    };
                    velocity += (desired - velocity) * blend;

                    if (pointer_active_)
                    {
                        const Vec2 delta = pointer_position_ - position;
                        const float distance_squared = length_squared(delta);
                        if (distance_squared > 1.0F
                            && distance_squared < pointer_radius_ * pointer_radius_)
                        {
                            const float distance = std::sqrt(distance_squared);
                            const float falloff = 1.0F - distance / pointer_radius_;
                            const Vec2 direction = delta / distance;
                            velocity += direction
                                * (pointer_repels_ ? -1.0F : 1.0F)
                                * (240.0F * falloff * context.delta_seconds);
                        }
                    }

                    position += velocity * context.delta_seconds;
                    positions[index] = detail::wrapped(position, bounds_);
                    velocities[index] = velocity;
                    speed_sum += static_cast<double>(length(velocity));
                }

                average_speed_ = positions.empty()
                    ? 0.0
                    : speed_sum / static_cast<double>(positions.size());
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                const Color field_color{ 0.16F, 0.28F, 0.42F, 0.55F };
                constexpr int columns = 22;
                constexpr int rows = 13;
                for (int row = 0; row < rows; ++row)
                {
                    for (int column = 0; column < columns; ++column)
                    {
                        const Vec2 position{
                            (static_cast<float>(column) + 0.5F)
                                * bounds.width / static_cast<float>(columns),
                            (static_cast<float>(row) + 0.5F)
                                * bounds.height / static_cast<float>(rows)
                        };
                        const float normalized_x = position.x / std::max(bounds.width, 1.0F);
                        const float normalized_y = position.y / std::max(bounds.height, 1.0F);
                        const float angle =
                            std::sin(normalized_y * 13.0F + phase_ * 2.3F) * 1.65F
                            + std::cos(normalized_x * 9.0F - phase_ * 1.7F) * 1.15F
                            + std::sin((normalized_x + normalized_y) * 7.0F + phase_) * 0.65F;
                        const Vec2 direction{ std::cos(angle), std::sin(angle) };
                        frame.line(
                            position - direction * 5.0F,
                            position + direction * 5.0F,
                            1.0F,
                            field_color,
                            -5);
                    }
                }

                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                const auto colors = pool_.colors();
                for (std::size_t index = 0; index < positions.size(); ++index)
                {
                    const Vec2 direction = normalized_or(velocities[index], {});
                    frame.line(
                        positions[index] - direction * 4.5F,
                        positions[index] + direction * 1.5F,
                        1.5F,
                        colors[index],
                        2);
                }

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        10.0F,
                        pointer_repels_
                            ? Color{ 1.0F, 0.26F, 0.22F, 0.72F }
                            : Color{ 0.25F, 0.82F, 1.0F, 0.72F },
                        20);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                pointer_position_ = event.position;
                if (event.action == PointerAction::press)
                {
                    pointer_active_ = true;
                    pointer_repels_ = event.button == PointerButton::secondary;
                }
                else if (event.action == PointerAction::release)
                {
                    pointer_active_ = event.primary_down || event.secondary_down;
                    pointer_repels_ = event.secondary_down;
                }
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                if (!new_bounds.valid())
                    return;
                for (Vec2& position : pool_.positions())
                    detail::rescale_position(position, old_bounds, new_bounds);
                detail::rescale_position(pointer_position_, old_bounds, new_bounds);
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{};
                result.particle_count = static_cast<std::uint64_t>(pool_.size());
                result.metrics[0] = { "Avg speed", average_speed_ };
                result.metrics[1] = { "Field phase", static_cast<double>(phase_) };
                result.metrics[2] = { "Pointer radius", static_cast<double>(pointer_radius_) };
                result.metric_count = 3;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_float(phase_);
                hasher.append_u64(pool_.state_hash());
                return hasher.value();
            }

        private:
            static constexpr std::size_t particle_count_ = 5'200;
            static constexpr float pointer_radius_ = 150.0F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            ParticlePool pool_{ particle_count_ };
            Vec2 pointer_position_{};
            float phase_{};
            double average_speed_{};
            bool pointer_active_{};
            bool pointer_repels_{};
        };
    }

    std::unique_ptr<IScene> make_flow_field_scene()
    {
        return std::make_unique<FlowFieldScene>();
    }
}
