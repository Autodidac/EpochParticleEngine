#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/fixed.hpp>
#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        struct DeterministicParticle
        {
            FixedVec2 position{};
            FixedVec2 velocity{};
            std::uint32_t life{};
            std::uint32_t maximum_life{};
            std::uint64_t id{};
        };

        class DeterministicFountainScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "deterministic-fountain",
                    .name = "Deterministic Fountain",
                    .description = "Fixed-point particles, PCG32 spawning, stable compaction and repeatable hashes."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                random_.reseed(seed_);
                particles_.clear();
                particles_.reserve(8'192);
                next_id_ = 1;
                emitter_position_ = {
                    Fixed32::from_float(bounds_.width * 0.5F),
                    Fixed32::from_float(bounds_.height - 20.0F)
                };
                primary_down_ = false;
                last_spawn_count_ = 0;
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                last_spawn_count_ = 0;

                const std::size_t automatic_count = particles_.size() < 7'500U ? 4U : 0U;
                spawn(emitter_position_, automatic_count);
                if (primary_down_)
                    spawn(pointer_position_, 10U);

                const Fixed32 gravity = Fixed32::from_ratio(7, 25);
                const Fixed32 floor = Fixed32::from_float(std::max(bounds_.height - 4.0F, 4.0F));
                const Fixed32 right = Fixed32::from_float(std::max(bounds_.width - 4.0F, 4.0F));
                const Fixed32 left = Fixed32::from_integer(4);
                const Fixed32 ceiling = Fixed32::from_integer(2);
                const Fixed32 restitution = Fixed32::from_ratio(3, 5);
                const Fixed32 horizontal_damping = Fixed32::from_ratio(49, 50);

                for (DeterministicParticle& particle : particles_)
                {
                    particle.velocity.y += gravity;
                    particle.position += particle.velocity;

                    if (particle.position.x < left)
                    {
                        particle.position.x = left;
                        particle.velocity.x = -particle.velocity.x * restitution;
                    }
                    else if (particle.position.x > right)
                    {
                        particle.position.x = right;
                        particle.velocity.x = -particle.velocity.x * restitution;
                    }

                    if (particle.position.y < ceiling)
                    {
                        particle.position.y = ceiling;
                        particle.velocity.y = -particle.velocity.y * restitution;
                    }
                    else if (particle.position.y > floor)
                    {
                        particle.position.y = floor;
                        particle.velocity.y = -particle.velocity.y * restitution;
                        particle.velocity.x *= horizontal_damping;
                    }

                    if (particle.life > 0U)
                        --particle.life;
                }

                std::erase_if(
                    particles_,
                    [](const DeterministicParticle& particle)
                    {
                        return particle.life == 0U;
                    });

                if (last_spawn_count_ != 0U && context.events.size() < 256U)
                {
                    context.events.push_back({
                        .type = SimulationEventType::particle_emitted,
                        .position = {
                            emitter_position_.x.to_float(),
                            emitter_position_.y.to_float()
                        },
                        .intensity = static_cast<float>(last_spawn_count_),
                        .source_id = next_id_
                    });
                }
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                for (const DeterministicParticle& particle : particles_)
                {
                    const float life_fraction = particle.maximum_life == 0U
                        ? 0.0F
                        : static_cast<float>(particle.life)
                            / static_cast<float>(particle.maximum_life);
                    const std::uint32_t band =
                        static_cast<std::uint32_t>((particle.id * 7U) % detail::palette.size());
                    Color color = detail::species_color(band);
                    color.a = std::clamp(life_fraction * 1.3F, 0.1F, 1.0F);
                    const float radius = 1.6F + static_cast<float>(particle.id % 3U) * 0.45F;
                    frame.circle(
                        {
                            particle.position.x.to_float(),
                            particle.position.y.to_float()
                        },
                        radius,
                        color,
                        2);
                }

                frame.rounded_rectangle(
                    {
                        emitter_position_.x.to_float(),
                        emitter_position_.y.to_float() + 8.0F
                    },
                    { 28.0F, 8.0F },
                    5.0F,
                    { 0.17F, 0.20F, 0.25F, 1.0F },
                    1);
            }

            void pointer(const PointerEvent& event) override
            {
                pointer_position_ = {
                    Fixed32::from_float(event.position.x),
                    Fixed32::from_float(event.position.y)
                };
                primary_down_ = event.primary_down;
                if (event.action == PointerAction::press
                    && event.button == PointerButton::secondary)
                {
                    emitter_position_ = pointer_position_;
                }
            }

            void resize(Bounds, Bounds new_bounds) override
            {
                bounds_ = new_bounds;
                const Fixed32 right = Fixed32::from_float(new_bounds.width);
                const Fixed32 bottom = Fixed32::from_float(new_bounds.height);
                for (DeterministicParticle& particle : particles_)
                {
                    particle.position.x = std::clamp(
                        particle.position.x,
                        Fixed32{},
                        right);
                    particle.position.y = std::clamp(
                        particle.position.y,
                        Fixed32{},
                        bottom);
                }
                emitter_position_ = {
                    Fixed32::from_float(new_bounds.width * 0.5F),
                    Fixed32::from_float(new_bounds.height - 20.0F)
                };
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{
                    .particle_count = particles_.size(),
                    .active_cell_count = 0
                };
                result.metrics[0] = { "LAST SPAWN", static_cast<double>(last_spawn_count_) };
                result.metrics[1] = { "NEXT ID", static_cast<double>(next_id_) };
                result.metric_count = 2;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(random_.state());
                hasher.append_u64(random_.increment());
                hasher.append_u64(next_id_);
                hasher.append_i32(emitter_position_.x.raw());
                hasher.append_i32(emitter_position_.y.raw());
                hasher.append_u64(particles_.size());
                for (const DeterministicParticle& particle : particles_)
                {
                    hasher.append_i32(particle.position.x.raw());
                    hasher.append_i32(particle.position.y.raw());
                    hasher.append_i32(particle.velocity.x.raw());
                    hasher.append_i32(particle.velocity.y.raw());
                    hasher.append_u32(particle.life);
                    hasher.append_u32(particle.maximum_life);
                    hasher.append_u64(particle.id);
                }
                return hasher.value();
            }

        private:
            void spawn(FixedVec2 origin, std::size_t count)
            {
                constexpr std::size_t maximum_particles = 8'192;
                count = std::min(count, maximum_particles - std::min(maximum_particles, particles_.size()));
                for (std::size_t index = 0; index < count; ++index)
                {
                    const std::int32_t horizontal = random_.range(-75, 76);
                    const std::int32_t vertical = random_.range(85, 151);
                    const std::uint32_t life = 300U + random_.bounded(220U);
                    const FixedVec2 jitter{
                        Fixed32::from_ratio(random_.range(-20, 21), 10),
                        Fixed32::from_ratio(random_.range(-10, 11), 10)
                    };
                    particles_.push_back({
                        .position = origin + jitter,
                        .velocity = {
                            Fixed32::from_ratio(horizontal, 10),
                            Fixed32::from_ratio(-vertical, 10)
                        },
                        .life = life,
                        .maximum_life = life,
                        .id = next_id_++
                    });
                    ++last_spawn_count_;
                }
            }

            Bounds bounds_{};
            std::uint64_t seed_{};
            Pcg32 random_{};
            std::vector<DeterministicParticle> particles_;
            FixedVec2 emitter_position_{};
            FixedVec2 pointer_position_{};
            std::uint64_t next_id_{ 1 };
            std::size_t last_spawn_count_{};
            bool primary_down_{};
        };
    }

    std::unique_ptr<IScene> make_deterministic_fountain_scene()
    {
        return std::make_unique<DeterministicFountainScene>();
    }
}
