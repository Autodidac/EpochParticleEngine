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

namespace epochengine::particle::scenes
{
    namespace
    {
        enum WeatherSpecies : std::uint32_t
        {
            rain_species = 0,
            snow_species = 1,
            hail_species = 2
        };

        class WeatherScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "weather",
                    .name = "Weather Lab",
                    .description = "Rain, snow, and hail share one particle pool with gusts, drag, and pointer wind."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                wind_impulse_ = 0.0F;
                pool_.clear();
                pool_.reserve(particle_count_);

                Pcg32 random(seed_ ^ 0x574541544845524cULL);
                for (std::size_t index = 0; index < particle_count_; ++index)
                {
                    const std::uint32_t species = static_cast<std::uint32_t>(index % 3U);
                    const float base_speed = species == rain_species
                        ? random.range(260.0F, 420.0F)
                        : species == snow_species
                            ? random.range(34.0F, 78.0F)
                            : random.range(155.0F, 245.0F);
                    pool_.spawn({
                        .position = {
                            random.range(0.0F, bounds_.width),
                            random.range(0.0F, bounds_.height)
                        },
                        .velocity = {
                            random.range(-24.0F, 24.0F),
                            base_speed
                        },
                        .color = weather_color(species),
                        .radius = species == snow_species ? 2.8F : species == hail_species ? 2.1F : 1.2F,
                        .lifetime = -1.0F,
                        .inverse_mass = 1.0F,
                        .species = species,
                        .flags = particle_none
                    });
                }
            }

            void update(const SceneUpdateContext& context) override
            {
                auto positions = pool_.positions();
                auto velocities = pool_.velocities();
                const auto species = pool_.species();
                const float tick_phase = static_cast<float>(context.tick % 50'000U) * 0.013F;
                const float gust = std::sin(tick_phase) * 32.0F
                    + std::sin(tick_phase * 0.37F + 1.4F) * 18.0F;
                wind_impulse_ = gust;

                for (std::size_t index = 0; index < positions.size(); ++index)
                {
                    Vec2 position = positions[index];
                    Vec2 velocity = velocities[index];
                    const std::uint32_t type = species[index];
                    const float local_gust = gust
                        + std::sin(position.y * 0.018F + tick_phase * 0.65F) * 22.0F;

                    if (type == rain_species)
                    {
                        velocity.x += (local_gust - velocity.x) * context.delta_seconds * 1.8F;
                        velocity.y += 28.0F * context.delta_seconds;
                    }
                    else if (type == snow_species)
                    {
                        velocity.x += (local_gust * 1.45F - velocity.x)
                            * context.delta_seconds * 1.15F;
                        velocity.y += std::sin(tick_phase + position.x * 0.02F)
                            * 8.0F * context.delta_seconds;
                    }
                    else
                    {
                        velocity.x += (local_gust * 0.72F - velocity.x)
                            * context.delta_seconds * 0.9F;
                        velocity.y += 155.0F * context.delta_seconds;
                    }

                    if (pointer_active_)
                    {
                        const Vec2 delta = position - pointer_position_;
                        const float distance_squared = length_squared(delta);
                        if (distance_squared > 1.0F
                            && distance_squared < pointer_radius_ * pointer_radius_)
                        {
                            const float distance = std::sqrt(distance_squared);
                            const float falloff = 1.0F - distance / pointer_radius_;
                            const Vec2 tangent = perpendicular(delta / distance);
                            velocity += tangent * (falloff * 260.0F * context.delta_seconds);
                        }
                    }

                    position += velocity * context.delta_seconds;
                    if (type == hail_species && position.y >= bounds_.height - 5.0F
                        && velocity.y > 0.0F)
                    {
                        position.y = bounds_.height - 5.0F;
                        velocity.y = -velocity.y * 0.52F;
                        velocity.x *= 0.86F;
                    }

                    if (position.y > bounds_.height + 18.0F
                        || position.x < -80.0F
                        || position.x > bounds_.width + 80.0F)
                    {
                        respawn(index, context.tick, position, velocity, type);
                    }
                    positions[index] = position;
                    velocities[index] = velocity;
                }
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                const auto species = pool_.species();
                for (std::size_t index = 0; index < positions.size(); ++index)
                {
                    const std::uint32_t type = species[index];
                    if (type == rain_species)
                    {
                        const Vec2 direction = normalized_or(velocities[index], { 0.0F, 1.0F });
                        frame.line(
                            positions[index] - direction * 7.0F,
                            positions[index] + direction * 2.0F,
                            1.15F,
                            weather_color(type),
                            2);
                    }
                    else
                    {
                        frame.circle(
                            positions[index],
                            type == snow_species ? 2.7F : 2.2F,
                            weather_color(type),
                            2);
                    }
                }

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        12.0F,
                        Color{ 0.30F, 0.84F, 1.0F, 0.70F },
                        20);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                pointer_position_ = event.position;
                if (event.action == PointerAction::press)
                    pointer_active_ = event.button == PointerButton::primary;
                else if (event.action == PointerAction::release)
                    pointer_active_ = event.primary_down;
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
                result.metrics[0] = { "Rain", static_cast<double>(pool_.size() / 3U) };
                result.metrics[1] = { "Snow", static_cast<double>(pool_.size() / 3U) };
                result.metrics[2] = { "Hail", static_cast<double>(pool_.size() / 3U) };
                result.metrics[3] = { "Wind", static_cast<double>(wind_impulse_) };
                result.metric_count = 4;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(pool_.state_hash());
                hasher.append_float(wind_impulse_);
                return hasher.value();
            }

        private:
            [[nodiscard]] static constexpr Color weather_color(std::uint32_t species) noexcept
            {
                if (species == rain_species)
                    return { 0.28F, 0.68F, 1.0F, 0.78F };
                if (species == snow_species)
                    return { 0.90F, 0.96F, 1.0F, 0.94F };
                return { 0.62F, 0.86F, 1.0F, 0.92F };
            }

            void respawn(
                std::size_t index,
                std::uint64_t tick,
                Vec2& position,
                Vec2& velocity,
                std::uint32_t type) const noexcept
            {
                const std::uint32_t first = coordinate_hash(
                    static_cast<std::int32_t>(index),
                    static_cast<std::int32_t>(type),
                    tick,
                    seed_);
                const std::uint32_t second = coordinate_hash(
                    static_cast<std::int32_t>(type),
                    static_cast<std::int32_t>(index),
                    tick + 17U,
                    seed_ ^ 0x91e10da5ULL);
                const float unit_x = static_cast<float>(first & 0xffffU) / 65535.0F;
                const float unit_v = static_cast<float>(second & 0xffffU) / 65535.0F;
                position = {
                    unit_x * bounds_.width,
                    -6.0F - static_cast<float>((second >> 16U) & 0x3fU)
                };
                velocity.x = (unit_v - 0.5F) * 44.0F;
                velocity.y = type == rain_species
                    ? 280.0F + unit_v * 130.0F
                    : type == snow_species
                        ? 38.0F + unit_v * 42.0F
                        : 160.0F + unit_v * 90.0F;
            }

            static constexpr std::size_t particle_count_ = 3'600;
            static constexpr float pointer_radius_ = 150.0F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            ParticlePool pool_{ particle_count_ };
            Vec2 pointer_position_{};
            float wind_impulse_{};
            bool pointer_active_{};
        };
    }

    std::unique_ptr<IScene> make_weather_scene()
    {
        return std::make_unique<WeatherScene>();
    }
}
