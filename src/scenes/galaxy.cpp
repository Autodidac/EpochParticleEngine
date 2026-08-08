#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/particle_pool.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <iostream>

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
                compute_disabled_ = false;
                last_update_used_compute_ = false;
                compute_storage_.clear();
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

                last_update_used_compute_ = false;
                if (context.compute != nullptr
                    && !compute_disabled_
                    && update_compute(*context.compute, context.delta_seconds))
                {
                    return;
                }
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
                result.metrics[3] = { "GPU COMPUTE", last_update_used_compute_ ? 1.0 : 0.0 };
                result.metric_count = 4;
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
            [[nodiscard]] bool update_compute(
                IComputeBackend& backend,
                float delta_seconds)
            {
                static constexpr std::string_view shader = R"glsl(
#version 450
layout(local_size_x = 64) in;
layout(std430, set = 0, binding = 0) buffer GalaxyStorage
{
    uint words[];
} data;
float f(uint index) { return uintBitsToFloat(data.words[index]); }
void putf(uint index, float value) { data.words[index] = floatBitsToUint(value); }
vec2 gravity_from(vec2 position, vec2 source, float strength)
{
    vec2 delta = source - position;
    float distance_squared = dot(delta, delta) + f(11u);
    float inverse_distance = inversesqrt(distance_squared);
    return delta * (strength * inverse_distance / distance_squared);
}

void main()
{
    uint particle = gl_GlobalInvocationID.x;
    uint count = data.words[0];
    if (particle >= count)
        return;
    uint input_base = 32u;
    uint output_base = input_base + count * 4u;
    uint source = input_base + particle * 4u;
    vec2 position = vec2(f(source), f(source + 1u));
    vec2 velocity = vec2(f(source + 2u), f(source + 3u));
    vec2 acceleration = gravity_from(
        position, vec2(f(4u), f(5u)), f(8u));
    acceleration += gravity_from(
        position, vec2(f(6u), f(7u)), f(8u) * 0.82);

    if (data.words[16] != 0u)
    {
        vec2 delta = vec2(f(14u), f(15u)) - position;
        float distance_squared = dot(delta, delta) + f(12u);
        float inverse_distance = inversesqrt(distance_squared);
        float sign_value = data.words[17] != 0u ? -1.0 : 1.0;
        acceleration += delta * (
            sign_value * f(13u) * inverse_distance / distance_squared);
    }

    velocity = (velocity + acceleration * f(3u)) * 0.9997;
    position += velocity * f(3u);
    vec2 bounds = vec2(f(1u), f(2u));
    if (position.x < -80.0 || position.x > bounds.x + 80.0
        || position.y < -80.0 || position.y > bounds.y + 80.0)
    {
        position = mod(mod(position, bounds) + bounds, bounds);
        velocity *= 0.75;
    }

    uint output_word = output_base + particle * 4u;
    putf(output_word, position.x);
    putf(output_word + 1u, position.y);
    putf(output_word + 2u, velocity.x);
    putf(output_word + 3u, velocity.y);
}
)glsl";

                constexpr std::size_t header_words = 32U;
                constexpr std::size_t stride = 4U;
                const std::size_t count = pool_.size();
                const std::size_t output_base = header_words + count * stride;
                compute_storage_.assign(output_base + count * stride, 0U);
                const auto put_float = [this](std::size_t word, float value)
                {
                    compute_storage_[word] = std::bit_cast<std::uint32_t>(value);
                };
                compute_storage_[0] = static_cast<std::uint32_t>(count);
                put_float(1, bounds_.width);
                put_float(2, bounds_.height);
                put_float(3, delta_seconds);
                put_float(4, attractor_a_.x);
                put_float(5, attractor_a_.y);
                put_float(6, attractor_b_.x);
                put_float(7, attractor_b_.y);
                put_float(8, gravity_strength_);
                put_float(11, softening_);
                put_float(12, pointer_softening_);
                put_float(13, pointer_strength_);
                put_float(14, pointer_position_.x);
                put_float(15, pointer_position_.y);
                compute_storage_[16] = pointer_active_ ? 1U : 0U;
                compute_storage_[17] = pointer_repels_ ? 1U : 0U;

                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    const std::size_t word = header_words + particle * stride;
                    put_float(word, positions[particle].x);
                    put_float(word + 1U, positions[particle].y);
                    put_float(word + 2U, velocities[particle].x);
                    put_float(word + 3U, velocities[particle].y);
                }

                std::span<std::uint32_t> words{ compute_storage_ };
                const auto result = backend.dispatch({
                    .program_id = "epoch.galaxy.v1",
                    .shader_source = shader,
                    .storage = std::as_writable_bytes(words),
                    .push_constants = {},
                    .workgroup_count_x = static_cast<std::uint32_t>(
                        (count + 63U) / 64U)
                });
                if (!result)
                {
                    std::clog << "Galaxy compute fallback: "
                              << result.error() << '\n';
                    compute_disabled_ = true;
                    return false;
                }

                auto mutable_positions = pool_.positions();
                auto mutable_velocities = pool_.velocities();
                average_speed_ = 0.0;
                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    const std::size_t word = output_base + particle * stride;
                    mutable_positions[particle] = {
                        std::bit_cast<float>(compute_storage_[word]),
                        std::bit_cast<float>(compute_storage_[word + 1U])
                    };
                    mutable_velocities[particle] = {
                        std::bit_cast<float>(compute_storage_[word + 2U]),
                        std::bit_cast<float>(compute_storage_[word + 3U])
                    };
                    average_speed_ += static_cast<double>(
                        length(mutable_velocities[particle]));
                }
                if (count != 0)
                    average_speed_ /= static_cast<double>(count);
                last_update_used_compute_ = true;
                return true;
            }

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
            std::vector<std::uint32_t> compute_storage_;
            bool compute_disabled_{};
            bool last_update_used_compute_{};
        };
    }

    std::unique_ptr<IScene> make_galaxy_scene()
    {
        return std::make_unique<GalaxyScene>();
    }
}
