#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>

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
        struct FireworkParticle
        {
            Vec2 position{};
            Vec2 velocity{};
            Color color{};
            float age{};
            float lifetime{ 1.0F };
            float radius{ 2.0F };
            std::uint64_t id{};
            bool rocket{};
            bool exploded{};
        };

        class FireworksScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "fireworks",
                    .name = "Fireworks",
                    .description = "Ballistic rockets, radial explosions, drag, gravity and additive trails."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                random_.reseed(seed_);
                particles_.clear();
                particles_.reserve(maximum_particles_);
                next_id_ = 1;
                explosions_last_step_ = 0;
                launch(bounds_.width * 0.28F);
                launch(bounds_.width * 0.62F);
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                explosions_last_step_ = 0;
                if ((context.tick % 54U) == 0U && particles_.size() < 4'000U)
                    launch(random_.range(bounds_.width * 0.12F, bounds_.width * 0.88F));

                struct Explosion
                {
                    Vec2 position;
                    Color color;
                    std::uint64_t source_id;
                };
                std::vector<Explosion> explosions;
                explosions.reserve(8);

                for (FireworkParticle& particle : particles_)
                {
                    particle.age += context.delta_seconds;

                    if (particle.rocket)
                    {
                        particle.velocity.y += 105.0F * context.delta_seconds;
                        particle.position += particle.velocity * context.delta_seconds;
                        if (particle.age >= particle.lifetime
                            || particle.velocity.y > -18.0F)
                        {
                            particle.exploded = true;
                            explosions.push_back({
                                .position = particle.position,
                                .color = particle.color,
                                .source_id = particle.id
                            });
                        }
                    }
                    else
                    {
                        particle.velocity.y += 92.0F * context.delta_seconds;
                        particle.velocity *= std::pow(0.985F, context.delta_seconds * 60.0F);
                        particle.position += particle.velocity * context.delta_seconds;
                        const float remaining = std::clamp(
                            1.0F - particle.age / particle.lifetime,
                            0.0F,
                            1.0F);
                        particle.color.a = remaining;
                        particle.radius = 0.7F + remaining * 1.7F;
                    }
                }

                std::erase_if(
                    particles_,
                    [this](const FireworkParticle& particle)
                    {
                        return particle.exploded || particle.age >= particle.lifetime
                            || particle.position.y > bounds_.height + 20.0F
                            || particle.position.x < -40.0F
                            || particle.position.x > bounds_.width + 40.0F;
                    });

                for (const Explosion& explosion : explosions)
                {
                    explode(explosion.position, explosion.color);
                    ++explosions_last_step_;
                    if (context.events.size() < 256U)
                    {
                        context.events.push_back({
                            .type = SimulationEventType::explosion,
                            .position = explosion.position,
                            .intensity = 1.0F,
                            .source_id = explosion.source_id
                        });
                    }
                }
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                for (const FireworkParticle& particle : particles_)
                {
                    if (particle.rocket)
                    {
                        frame.line(
                            particle.position,
                            particle.position - particle.velocity * 0.06F,
                            2.0F,
                            with_alpha(particle.color, 0.55F),
                            2);
                        frame.circle(particle.position, 3.1F, particle.color, 3);
                    }
                    else
                    {
                        frame.line(
                            particle.position,
                            particle.position - particle.velocity * 0.024F,
                            std::max(0.7F, particle.radius * 0.65F),
                            with_alpha(particle.color, particle.color.a * 0.45F),
                            2);
                        frame.circle(
                            particle.position,
                            particle.radius,
                            particle.color,
                            3);
                    }
                }
            }

            void pointer(const PointerEvent& event) override
            {
                if (event.action == PointerAction::press
                    && event.button == PointerButton::primary)
                {
                    launch(event.position.x);
                }
                else if (event.action == PointerAction::press
                    && event.button == PointerButton::secondary)
                {
                    const Color color = detail::species_color(random_.bounded(8U));
                    explode(event.position, color);
                }
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                for (FireworkParticle& particle : particles_)
                    detail::rescale_position(particle.position, old_bounds, new_bounds);
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                const std::size_t rockets = static_cast<std::size_t>(std::count_if(
                    particles_.begin(),
                    particles_.end(),
                    [](const FireworkParticle& particle)
                    {
                        return particle.rocket;
                    }));
                SceneStats result{
                    .particle_count = particles_.size(),
                    .active_cell_count = 0
                };
                result.metrics[0] = { "ROCKETS", static_cast<double>(rockets) };
                result.metrics[1] = {
                    "EXPLOSIONS",
                    static_cast<double>(explosions_last_step_)
                };
                result.metrics[2] = { "NEXT ID", static_cast<double>(next_id_) };
                result.metric_count = 3;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(random_.state());
                hasher.append_u64(next_id_);
                hasher.append_u64(particles_.size());
                for (const FireworkParticle& particle : particles_)
                {
                    hasher.append_float(particle.position.x);
                    hasher.append_float(particle.position.y);
                    hasher.append_float(particle.velocity.x);
                    hasher.append_float(particle.velocity.y);
                    hasher.append_float(particle.color.r);
                    hasher.append_float(particle.color.g);
                    hasher.append_float(particle.color.b);
                    hasher.append_float(particle.color.a);
                    hasher.append_float(particle.age);
                    hasher.append_float(particle.lifetime);
                    hasher.append_float(particle.radius);
                    hasher.append_u64(particle.id);
                    hasher.append_byte(static_cast<std::uint8_t>(particle.rocket));
                }
                return hasher.value();
            }

        private:
            void launch(float x)
            {
                if (particles_.size() >= maximum_particles_)
                    return;

                const Color color = detail::species_color(random_.bounded(8U));
                particles_.push_back({
                    .position = {
                        std::clamp(x, 12.0F, std::max(12.0F, bounds_.width - 12.0F)),
                        bounds_.height - 12.0F
                    },
                    .velocity = {
                        random_.range(-28.0F, 28.0F),
                        random_.range(-410.0F, -300.0F)
                    },
                    .color = color,
                    .age = 0.0F,
                    .lifetime = random_.range(0.8F, 1.45F),
                    .radius = 3.0F,
                    .id = next_id_++,
                    .rocket = true,
                    .exploded = false
                });
            }

            void explode(Vec2 position, Color base_color)
            {
                const std::size_t available =
                    maximum_particles_ - std::min(maximum_particles_, particles_.size());
                const std::size_t count = std::min<std::size_t>(
                    96U + random_.bounded(72U),
                    available);
                const float phase = random_.range(0.0F, std::numbers::pi_v<float> * 2.0F);

                for (std::size_t index = 0; index < count; ++index)
                {
                    const float angle = phase
                        + static_cast<float>(index) / static_cast<float>(count)
                            * std::numbers::pi_v<float> * 2.0F
                        + random_.range(-0.035F, 0.035F);
                    const float speed = random_.range(75.0F, 250.0F)
                        * (0.65F + 0.35F * std::sin(
                            static_cast<float>(index) * 2.399963F)
                            * std::sin(static_cast<float>(index) * 2.399963F));
                    Color color = lerp(
                        base_color,
                        Color{ 1.0F, 1.0F, 0.95F, 1.0F },
                        random_.range(0.0F, 0.28F));
                    particles_.push_back({
                        .position = position,
                        .velocity = {
                            std::cos(angle) * speed,
                            std::sin(angle) * speed
                        },
                        .color = color,
                        .age = 0.0F,
                        .lifetime = random_.range(1.3F, 2.8F),
                        .radius = random_.range(1.2F, 2.7F),
                        .id = next_id_++,
                        .rocket = false,
                        .exploded = false
                    });
                }
            }

            static constexpr std::size_t maximum_particles_ = 18'000;

            Bounds bounds_{};
            std::uint64_t seed_{};
            Pcg32 random_{};
            std::vector<FireworkParticle> particles_;
            std::uint64_t next_id_{ 1 };
            std::size_t explosions_last_step_{};
        };
    }

    std::unique_ptr<IScene> make_fireworks_scene()
    {
        return std::make_unique<FireworksScene>();
    }
}
