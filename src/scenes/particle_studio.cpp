#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/particle_pool.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>
#include <epochengine/particle/text.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        enum class StudioTool : std::uint8_t
        {
            emitter,
            attractor,
            repulsor,
            vortex,
            obstacle,
            erase
        };

        enum class StudioFieldKind : std::uint8_t
        {
            attractor,
            repulsor,
            vortex
        };

        struct StudioEmitter
        {
            Vec2 position{};
            float rate{ 90.0F };
            float speed{ 190.0F };
            float spread{ 0.55F };
            float angle{ -std::numbers::pi_v<float> * 0.5F };
            float accumulator{};
            std::uint32_t species{};
        };

        struct StudioField
        {
            Vec2 position{};
            float radius{ 150.0F };
            float strength{ 300.0F };
            StudioFieldKind kind{ StudioFieldKind::attractor };
        };

        struct StudioObstacle
        {
            Vec2 position{};
            float radius{ 36.0F };
        };

        struct StudioButton
        {
            std::string_view label;
            StudioTool tool{};
        };

        constexpr std::array studio_buttons{
            StudioButton{ "EMITTER", StudioTool::emitter },
            StudioButton{ "ATTRACT", StudioTool::attractor },
            StudioButton{ "REPEL", StudioTool::repulsor },
            StudioButton{ "VORTEX", StudioTool::vortex },
            StudioButton{ "OBSTACLE", StudioTool::obstacle },
            StudioButton{ "ERASE", StudioTool::erase }
        };

        class ParticleStudioScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "particle-studio",
                    .name = "Particle Studio",
                    .description = "Build particle scenes live: emitters, force fields, vortices, obstacles, bursts, save/load."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                rng_.reseed(seed_ ^ 0x53545544494f3032ULL);
                pool_.clear();
                emitters_.clear();
                fields_.clear();
                obstacles_.clear();
                tool_ = StudioTool::emitter;
                cursor_ = bounds_.center();
                cursor_visible_ = false;
                spawned_total_ = 0;
                collision_count_ = 0;
                build_default_document();
            }

            void update(const SceneUpdateContext& context) override
            {
                emit_from_sources(context.delta_seconds);
                pool_.age(context.delta_seconds);

                auto positions = pool_.positions();
                auto velocities = pool_.velocities();
                auto radii = pool_.radii();
                collision_count_ = 0;

                for (std::size_t index = 0; index < positions.size(); ++index)
                {
                    Vec2 position = positions[index];
                    Vec2 velocity = velocities[index];
                    velocity.y += gravity_ * context.delta_seconds;

                    for (const StudioField& field : fields_)
                    {
                        const Vec2 delta = field.position - position;
                        const float distance_squared = length_squared(delta);
                        if (distance_squared <= 1.0F
                            || distance_squared >= field.radius * field.radius)
                        {
                            continue;
                        }
                        const float distance = std::sqrt(distance_squared);
                        const float falloff = 1.0F - distance / field.radius;
                        const Vec2 direction = delta / distance;
                        if (field.kind == StudioFieldKind::attractor)
                        {
                            velocity += direction
                                * (field.strength * falloff * context.delta_seconds);
                        }
                        else if (field.kind == StudioFieldKind::repulsor)
                        {
                            velocity -= direction
                                * (field.strength * falloff * context.delta_seconds);
                        }
                        else
                        {
                            velocity += perpendicular(direction)
                                * (field.strength * falloff * context.delta_seconds);
                        }
                    }

                    position += velocity * context.delta_seconds;
                    collide_world(position, velocity, radii[index]);
                    for (const StudioObstacle& obstacle : obstacles_)
                        collide_obstacle(position, velocity, radii[index], obstacle);

                    positions[index] = position;
                    velocities[index] = velocity;
                }
                pool_.compact_stable();
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                frame.rectangle(
                    bounds.center(),
                    { bounds.width * 0.5F, bounds.height * 0.5F },
                    { 0.010F, 0.017F, 0.030F, 1.0F },
                    -20);

                render_editor_objects(frame);

                const auto positions = pool_.positions();
                const auto colors = pool_.colors();
                const auto radii = pool_.radii();
                for (std::size_t index = 0; index < positions.size(); ++index)
                {
                    frame.circle(
                        positions[index],
                        radii[index],
                        colors[index],
                        4);
                }

                render_toolbar(frame);
                if (cursor_visible_ && cursor_.y > toolbar_height_)
                {
                    frame.circle(
                        cursor_,
                        cursor_radius(),
                        tool_color(tool_, 0.55F),
                        60);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                cursor_ = event.position;
                cursor_visible_ = true;

                if (event.action == PointerAction::move)
                {
                    if (event.primary_down && tool_ == StudioTool::erase
                        && event.position.y > toolbar_height_)
                    {
                        erase_nearest(event.position, event.shift ? 72.0F : 42.0F);
                    }
                    return;
                }
                if (event.action != PointerAction::press)
                    return;

                if (event.position.y <= toolbar_height_)
                {
                    handle_toolbar_press(event.position);
                    return;
                }
                if (event.button == PointerButton::secondary)
                {
                    erase_nearest(event.position, event.shift ? 84.0F : 48.0F);
                    return;
                }
                if (event.button == PointerButton::middle)
                {
                    burst(event.position, event.shift ? 420U : 180U);
                    return;
                }
                place_tool(event);
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                if (!new_bounds.valid())
                    return;
                for (Vec2& position : pool_.positions())
                    detail::rescale_position(position, old_bounds, new_bounds);
                for (StudioEmitter& emitter : emitters_)
                    detail::rescale_position(emitter.position, old_bounds, new_bounds);
                for (StudioField& field : fields_)
                    detail::rescale_position(field.position, old_bounds, new_bounds);
                for (StudioObstacle& obstacle : obstacles_)
                    detail::rescale_position(obstacle.position, old_bounds, new_bounds);
                detail::rescale_position(cursor_, old_bounds, new_bounds);
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{};
                result.particle_count = static_cast<std::uint64_t>(pool_.size());
                result.metrics[0] = { "Emitters", static_cast<double>(emitters_.size()) };
                result.metrics[1] = { "Fields", static_cast<double>(fields_.size()) };
                result.metrics[2] = { "Obstacles", static_cast<double>(obstacles_.size()) };
                result.metrics[3] = { "Spawned", static_cast<double>(spawned_total_) };
                result.metrics[4] = { "Collisions", static_cast<double>(collision_count_) };
                result.metrics[5] = { "Tool", static_cast<double>(std::to_underlying(tool_)) };
                result.metric_count = 6;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(pool_.state_hash());
                hasher.append_u64(rng_.state());
                hasher.append_u64(rng_.increment());
                hasher.append_byte(static_cast<std::uint8_t>(tool_));
                for (const StudioEmitter& emitter : emitters_)
                {
                    hasher.append_float(emitter.position.x);
                    hasher.append_float(emitter.position.y);
                    hasher.append_float(emitter.rate);
                    hasher.append_float(emitter.speed);
                    hasher.append_float(emitter.spread);
                    hasher.append_float(emitter.angle);
                    hasher.append_float(emitter.accumulator);
                    hasher.append_u32(emitter.species);
                }
                for (const StudioField& field : fields_)
                {
                    hasher.append_float(field.position.x);
                    hasher.append_float(field.position.y);
                    hasher.append_float(field.radius);
                    hasher.append_float(field.strength);
                    hasher.append_byte(static_cast<std::uint8_t>(field.kind));
                }
                for (const StudioObstacle& obstacle : obstacles_)
                {
                    hasher.append_float(obstacle.position.x);
                    hasher.append_float(obstacle.position.y);
                    hasher.append_float(obstacle.radius);
                }
                return hasher.value();
            }

            [[nodiscard]] std::string scene_document() const override
            {
                std::ostringstream output;
                output << std::setprecision(9);
                output << "EPOCH_PARTICLE_SCENE 1\n";
                output << "TOOL " << static_cast<unsigned>(tool_) << '\n';
                output << "EMITTERS " << emitters_.size() << '\n';
                for (const StudioEmitter& emitter : emitters_)
                {
                    output
                        << emitter.position.x << ' '
                        << emitter.position.y << ' '
                        << emitter.rate << ' '
                        << emitter.speed << ' '
                        << emitter.spread << ' '
                        << emitter.angle << ' '
                        << emitter.species << '\n';
                }
                output << "FIELDS " << fields_.size() << '\n';
                for (const StudioField& field : fields_)
                {
                    output
                        << static_cast<unsigned>(field.kind) << ' '
                        << field.position.x << ' '
                        << field.position.y << ' '
                        << field.radius << ' '
                        << field.strength << '\n';
                }
                output << "OBSTACLES " << obstacles_.size() << '\n';
                for (const StudioObstacle& obstacle : obstacles_)
                {
                    output
                        << obstacle.position.x << ' '
                        << obstacle.position.y << ' '
                        << obstacle.radius << '\n';
                }
                return output.str();
            }

            bool apply_scene_document(std::string_view document) override
            {
                std::istringstream input{ std::string{ document } };
                std::string token;
                unsigned version = 0;
                if (!(input >> token >> version)
                    || token != "EPOCH_PARTICLE_SCENE"
                    || version != 1U)
                {
                    return false;
                }

                unsigned tool_value = 0;
                if (!(input >> token >> tool_value)
                    || token != "TOOL"
                    || tool_value > static_cast<unsigned>(StudioTool::erase))
                {
                    return false;
                }

                std::size_t emitter_count = 0;
                if (!(input >> token >> emitter_count)
                    || token != "EMITTERS"
                    || emitter_count > maximum_emitters_)
                {
                    return false;
                }
                std::vector<StudioEmitter> emitters;
                emitters.reserve(emitter_count);
                for (std::size_t index = 0; index < emitter_count; ++index)
                {
                    StudioEmitter emitter{};
                    if (!(input
                        >> emitter.position.x
                        >> emitter.position.y
                        >> emitter.rate
                        >> emitter.speed
                        >> emitter.spread
                        >> emitter.angle
                        >> emitter.species)
                        || !valid_emitter(emitter))
                    {
                        return false;
                    }
                    emitter.position = bounds_.clamp_point(emitter.position, 4.0F);
                    emitter.species %= 8U;
                    emitters.push_back(emitter);
                }

                std::size_t field_count = 0;
                if (!(input >> token >> field_count)
                    || token != "FIELDS"
                    || field_count > maximum_fields_)
                {
                    return false;
                }
                std::vector<StudioField> fields;
                fields.reserve(field_count);
                for (std::size_t index = 0; index < field_count; ++index)
                {
                    unsigned kind = 0;
                    StudioField field{};
                    if (!(input
                        >> kind
                        >> field.position.x
                        >> field.position.y
                        >> field.radius
                        >> field.strength)
                        || kind > static_cast<unsigned>(StudioFieldKind::vortex)
                        || !valid_field(field))
                    {
                        return false;
                    }
                    field.kind = static_cast<StudioFieldKind>(kind);
                    field.position = bounds_.clamp_point(field.position, 4.0F);
                    fields.push_back(field);
                }

                std::size_t obstacle_count = 0;
                if (!(input >> token >> obstacle_count)
                    || token != "OBSTACLES"
                    || obstacle_count > maximum_obstacles_)
                {
                    return false;
                }
                std::vector<StudioObstacle> obstacles;
                obstacles.reserve(obstacle_count);
                for (std::size_t index = 0; index < obstacle_count; ++index)
                {
                    StudioObstacle obstacle{};
                    if (!(input
                        >> obstacle.position.x
                        >> obstacle.position.y
                        >> obstacle.radius)
                        || !valid_obstacle(obstacle))
                    {
                        return false;
                    }
                    obstacle.position = bounds_.clamp_point(obstacle.position, 4.0F);
                    obstacles.push_back(obstacle);
                }

                tool_ = static_cast<StudioTool>(tool_value);
                emitters_ = std::move(emitters);
                fields_ = std::move(fields);
                obstacles_ = std::move(obstacles);
                pool_.clear();
                rng_.reseed(seed_ ^ 0x53545544494f3032ULL);
                spawned_total_ = 0;
                collision_count_ = 0;
                return true;
            }

        private:
            [[nodiscard]] static bool finite(Vec2 value) noexcept
            {
                return std::isfinite(value.x) && std::isfinite(value.y);
            }

            [[nodiscard]] static bool valid_emitter(const StudioEmitter& emitter) noexcept
            {
                return finite(emitter.position)
                    && std::isfinite(emitter.rate)
                    && std::isfinite(emitter.speed)
                    && std::isfinite(emitter.spread)
                    && std::isfinite(emitter.angle)
                    && emitter.rate >= 1.0F && emitter.rate <= 2'000.0F
                    && emitter.speed >= 0.0F && emitter.speed <= 2'000.0F
                    && emitter.spread >= 0.0F && emitter.spread <= 6.4F;
            }

            [[nodiscard]] static bool valid_field(const StudioField& field) noexcept
            {
                return finite(field.position)
                    && std::isfinite(field.radius)
                    && std::isfinite(field.strength)
                    && field.radius >= 4.0F && field.radius <= 2'000.0F
                    && field.strength >= 0.0F && field.strength <= 5'000.0F;
            }

            [[nodiscard]] static bool valid_obstacle(const StudioObstacle& obstacle) noexcept
            {
                return finite(obstacle.position)
                    && std::isfinite(obstacle.radius)
                    && obstacle.radius >= 2.0F && obstacle.radius <= 1'000.0F;
            }

            [[nodiscard]] static Color tool_color(StudioTool tool, float alpha) noexcept
            {
                Color color{};
                switch (tool)
                {
                case StudioTool::emitter: color = { 0.24F, 0.82F, 1.0F, alpha }; break;
                case StudioTool::attractor: color = { 0.34F, 1.0F, 0.56F, alpha }; break;
                case StudioTool::repulsor: color = { 1.0F, 0.28F, 0.24F, alpha }; break;
                case StudioTool::vortex: color = { 0.78F, 0.42F, 1.0F, alpha }; break;
                case StudioTool::obstacle: color = { 0.72F, 0.78F, 0.86F, alpha }; break;
                case StudioTool::erase: color = { 1.0F, 0.68F, 0.22F, alpha }; break;
                }
                return color;
            }

            [[nodiscard]] float cursor_radius() const noexcept
            {
                if (tool_ == StudioTool::obstacle)
                    return 36.0F;
                if (tool_ == StudioTool::attractor
                    || tool_ == StudioTool::repulsor
                    || tool_ == StudioTool::vortex)
                {
                    return 22.0F;
                }
                return 10.0F;
            }

            void build_default_document()
            {
                emitters_.push_back({
                    .position = { bounds_.width * 0.22F, bounds_.height * 0.72F },
                    .rate = 105.0F,
                    .speed = 235.0F,
                    .spread = 0.62F,
                    .angle = -1.12F,
                    .accumulator = 0.0F,
                    .species = 0U
                });
                emitters_.push_back({
                    .position = { bounds_.width * 0.78F, bounds_.height * 0.72F },
                    .rate = 105.0F,
                    .speed = 235.0F,
                    .spread = 0.62F,
                    .angle = -2.02F,
                    .accumulator = 0.0F,
                    .species = 4U
                });
                fields_.push_back({
                    .position = bounds_.center(),
                    .radius = std::min(bounds_.width, bounds_.height) * 0.29F,
                    .strength = 330.0F,
                    .kind = StudioFieldKind::vortex
                });
                obstacles_.push_back({
                    .position = { bounds_.width * 0.50F, bounds_.height * 0.73F },
                    .radius = 48.0F
                });
            }

            void emit_from_sources(float delta_seconds)
            {
                for (StudioEmitter& emitter : emitters_)
                {
                    emitter.accumulator += emitter.rate * delta_seconds;
                    const std::size_t available = maximum_particles_ > pool_.size()
                        ? maximum_particles_ - pool_.size()
                        : 0U;
                    const std::size_t requested = static_cast<std::size_t>(
                        std::max(0.0F, std::floor(emitter.accumulator)));
                    const std::size_t count = std::min(requested, available);
                    for (std::size_t index = 0; index < count; ++index)
                        emit_particle(emitter);
                    emitter.accumulator -= static_cast<float>(count);
                    if (available == 0U)
                        emitter.accumulator = std::min(emitter.accumulator, 1.0F);
                }
            }

            void emit_particle(const StudioEmitter& emitter)
            {
                const float angle = emitter.angle
                    + rng_.range(-emitter.spread * 0.5F, emitter.spread * 0.5F);
                const float speed = emitter.speed * rng_.range(0.78F, 1.22F);
                pool_.spawn({
                    .position = emitter.position,
                    .velocity = {
                        std::cos(angle) * speed,
                        std::sin(angle) * speed
                    },
                    .color = detail::species_color(emitter.species),
                    .radius = rng_.range(1.8F, 3.4F),
                    .lifetime = rng_.range(6.0F, 11.0F),
                    .inverse_mass = 1.0F,
                    .species = emitter.species,
                    .flags = particle_none
                });
                ++spawned_total_;
            }

            void burst(Vec2 position, std::size_t requested)
            {
                const std::size_t available = maximum_particles_ > pool_.size()
                    ? maximum_particles_ - pool_.size()
                    : 0U;
                const std::size_t count = std::min(requested, available);
                for (std::size_t index = 0; index < count; ++index)
                {
                    const float angle = rng_.range(
                        0.0F,
                        std::numbers::pi_v<float> * 2.0F);
                    const float speed = rng_.range(80.0F, 330.0F);
                    const std::uint32_t species = rng_.bounded(8U);
                    pool_.spawn({
                        .position = position,
                        .velocity = {
                            std::cos(angle) * speed,
                            std::sin(angle) * speed
                        },
                        .color = detail::species_color(species),
                        .radius = rng_.range(1.6F, 3.8F),
                        .lifetime = rng_.range(4.0F, 8.0F),
                        .inverse_mass = 1.0F,
                        .species = species,
                        .flags = particle_additive
                    });
                    ++spawned_total_;
                }
            }

            void place_tool(const PointerEvent& event)
            {
                const Vec2 position = bounds_.clamp_point(event.position, 6.0F);
                const float scale = event.shift ? 1.55F : 1.0F;
                switch (tool_)
                {
                case StudioTool::emitter:
                    if (emitters_.size() < maximum_emitters_)
                    {
                        emitters_.push_back({
                            .position = position,
                            .rate = event.shift ? 180.0F : 95.0F,
                            .speed = event.control ? 330.0F : 205.0F,
                            .spread = event.control ? 0.20F : 0.70F,
                            .angle = -std::numbers::pi_v<float> * 0.5F,
                            .accumulator = 0.0F,
                            .species = static_cast<std::uint32_t>(emitters_.size() % 8U)
                        });
                    }
                    break;
                case StudioTool::attractor:
                    add_field(position, StudioFieldKind::attractor, scale);
                    break;
                case StudioTool::repulsor:
                    add_field(position, StudioFieldKind::repulsor, scale);
                    break;
                case StudioTool::vortex:
                    add_field(position, StudioFieldKind::vortex, scale);
                    break;
                case StudioTool::obstacle:
                    if (obstacles_.size() < maximum_obstacles_)
                    {
                        obstacles_.push_back({
                            .position = position,
                            .radius = 36.0F * scale
                        });
                    }
                    break;
                case StudioTool::erase:
                    erase_nearest(position, event.shift ? 84.0F : 48.0F);
                    break;
                }
            }

            void add_field(Vec2 position, StudioFieldKind kind, float scale)
            {
                if (fields_.size() >= maximum_fields_)
                    return;
                fields_.push_back({
                    .position = position,
                    .radius = 150.0F * scale,
                    .strength = (kind == StudioFieldKind::vortex ? 390.0F : 330.0F) * scale,
                    .kind = kind
                });
            }

            void erase_nearest(Vec2 position, float radius)
            {
                const float radius_squared = radius * radius;
                auto erase_nearest_vector = [position, radius_squared](auto& values)
                {
                    if (values.empty())
                        return false;
                    auto nearest = values.end();
                    float nearest_distance = radius_squared;
                    for (auto iterator = values.begin(); iterator != values.end(); ++iterator)
                    {
                        const float distance = length_squared(iterator->position - position);
                        if (distance <= nearest_distance)
                        {
                            nearest = iterator;
                            nearest_distance = distance;
                        }
                    }
                    if (nearest == values.end())
                        return false;
                    values.erase(nearest);
                    return true;
                };

                if (erase_nearest_vector(emitters_))
                    return;
                if (erase_nearest_vector(fields_))
                    return;
                static_cast<void>(erase_nearest_vector(obstacles_));
            }

            void collide_world(Vec2& position, Vec2& velocity, float radius) noexcept
            {
                if (position.x < radius)
                {
                    position.x = radius;
                    if (velocity.x < 0.0F)
                        velocity.x = -velocity.x * restitution_;
                    ++collision_count_;
                }
                else if (position.x > bounds_.width - radius)
                {
                    position.x = bounds_.width - radius;
                    if (velocity.x > 0.0F)
                        velocity.x = -velocity.x * restitution_;
                    ++collision_count_;
                }
                if (position.y < toolbar_height_ + radius)
                {
                    position.y = toolbar_height_ + radius;
                    if (velocity.y < 0.0F)
                        velocity.y = -velocity.y * restitution_;
                    ++collision_count_;
                }
                else if (position.y > bounds_.height - radius)
                {
                    position.y = bounds_.height - radius;
                    if (velocity.y > 0.0F)
                        velocity.y = -velocity.y * restitution_;
                    velocity.x *= 0.985F;
                    ++collision_count_;
                }
            }

            void collide_obstacle(
                Vec2& position,
                Vec2& velocity,
                float radius,
                const StudioObstacle& obstacle) noexcept
            {
                const Vec2 delta = position - obstacle.position;
                const float minimum_distance = radius + obstacle.radius;
                const float distance_squared = length_squared(delta);
                if (distance_squared >= minimum_distance * minimum_distance)
                    return;
                const Vec2 normal = normalized_or(delta, { 0.0F, -1.0F });
                position = obstacle.position + normal * minimum_distance;
                const float normal_velocity = dot(velocity, normal);
                if (normal_velocity < 0.0F)
                    velocity -= normal * ((1.0F + restitution_) * normal_velocity);
                ++collision_count_;
            }

            void render_editor_objects(RenderFrame& frame) const
            {
                for (const StudioField& field : fields_)
                {
                    StudioTool tool = StudioTool::attractor;
                    if (field.kind == StudioFieldKind::repulsor)
                        tool = StudioTool::repulsor;
                    else if (field.kind == StudioFieldKind::vortex)
                        tool = StudioTool::vortex;
                    frame.circle(field.position, field.radius, tool_color(tool, 0.045F), -3);
                    frame.circle(field.position, 7.0F, tool_color(tool, 0.88F), 8);
                    if (field.kind == StudioFieldKind::vortex)
                    {
                        frame.line(
                            field.position + Vec2{ -16.0F, 0.0F },
                            field.position + Vec2{ 16.0F, 0.0F },
                            2.0F,
                            tool_color(tool, 0.82F),
                            8);
                    }
                }

                for (const StudioObstacle& obstacle : obstacles_)
                {
                    frame.circle(
                        obstacle.position,
                        obstacle.radius,
                        { 0.20F, 0.25F, 0.32F, 1.0F },
                        6);
                    frame.circle(
                        obstacle.position,
                        std::max(2.0F, obstacle.radius - 4.0F),
                        { 0.08F, 0.10F, 0.14F, 1.0F },
                        7);
                }

                for (const StudioEmitter& emitter : emitters_)
                {
                    const Vec2 direction{
                        std::cos(emitter.angle),
                        std::sin(emitter.angle)
                    };
                    frame.circle(
                        emitter.position,
                        9.0F,
                        detail::species_color(emitter.species),
                        12);
                    frame.line(
                        emitter.position,
                        emitter.position + direction * 25.0F,
                        3.0F,
                        detail::species_color(emitter.species),
                        13);
                }
            }

            void render_toolbar(RenderFrame& frame) const
            {
                frame.rectangle(
                    { bounds_.width * 0.5F, toolbar_height_ * 0.5F },
                    { bounds_.width * 0.5F, toolbar_height_ * 0.5F },
                    { 0.022F, 0.031F, 0.048F, 0.97F },
                    80);

                const float button_width = std::clamp(
                    (bounds_.width - 24.0F) / 9.0F,
                    74.0F,
                    112.0F);
                const float start_x = 12.0F + button_width * 0.5F;
                const float center_y = toolbar_height_ * 0.5F;
                const Vec2 half_extent{ button_width * 0.46F, 16.0F };
                for (std::size_t index = 0; index < studio_buttons.size(); ++index)
                {
                    const StudioButton button = studio_buttons[index];
                    const Vec2 center{
                        start_x + static_cast<float>(index) * button_width,
                        center_y
                    };
                    frame.rounded_rectangle(
                        center,
                        half_extent,
                        6.0F,
                        button.tool == tool_
                            ? tool_color(button.tool, 0.42F)
                            : Color{ 0.055F, 0.075F, 0.105F, 1.0F },
                        90);
                    frame.text(
                        { center.x, center.y - 6.0F },
                        button.label,
                        TextSize{ 11.0F, 1.0F },
                        button.tool == tool_
                            ? Color{ 0.95F, 0.98F, 1.0F, 1.0F }
                            : Color{ 0.68F, 0.76F, 0.86F, 1.0F },
                        100,
                        TextAlign::center);
                }

                constexpr std::array<std::string_view, 3> action_labels{
                    "BURST", "RANDOM", "CLEAR"
                };
                for (std::size_t action = 0; action < action_labels.size(); ++action)
                {
                    const std::size_t index = studio_buttons.size() + action;
                    const Vec2 center{
                        start_x + static_cast<float>(index) * button_width,
                        center_y
                    };
                    frame.rounded_rectangle(
                        center,
                        half_extent,
                        6.0F,
                        { 0.075F, 0.100F, 0.145F, 1.0F },
                        90);
                    frame.text(
                        { center.x, center.y - 6.0F },
                        action_labels[action],
                        TextSize{ 11.0F, 1.0F },
                        { 0.75F, 0.84F, 0.96F, 1.0F },
                        100,
                        TextAlign::center);
                }

                frame.text(
                    { 10.0F, toolbar_height_ + 8.0F },
                    "LMB PLACE  RMB ERASE  MMB BURST  SHIFT = LARGE  CTRL+S SAVE  CTRL+L LOAD",
                    TextSize{ 11.0F, 1.0F },
                    { 0.56F, 0.67F, 0.79F, 0.92F },
                    70);
            }

            void handle_toolbar_press(Vec2 position)
            {
                const float button_width = std::clamp(
                    (bounds_.width - 24.0F) / 9.0F,
                    74.0F,
                    112.0F);
                const float start = 12.0F;
                if (position.x < start)
                    return;
                const std::size_t index = static_cast<std::size_t>(
                    (position.x - start) / button_width);
                if (index < studio_buttons.size())
                {
                    tool_ = studio_buttons[index].tool;
                    return;
                }
                if (index == studio_buttons.size())
                {
                    burst(cursor_.y > toolbar_height_ ? cursor_ : bounds_.center(), 240U);
                }
                else if (index == studio_buttons.size() + 1U)
                {
                    randomize_document();
                }
                else if (index == studio_buttons.size() + 2U)
                {
                    emitters_.clear();
                    fields_.clear();
                    obstacles_.clear();
                    pool_.clear();
                }
            }

            void randomize_document()
            {
                emitters_.clear();
                fields_.clear();
                obstacles_.clear();
                pool_.clear();

                for (std::size_t index = 0; index < 4U; ++index)
                {
                    emitters_.push_back({
                        .position = {
                            rng_.range(bounds_.width * 0.12F, bounds_.width * 0.88F),
                            rng_.range(bounds_.height * 0.28F, bounds_.height * 0.82F)
                        },
                        .rate = rng_.range(55.0F, 150.0F),
                        .speed = rng_.range(140.0F, 310.0F),
                        .spread = rng_.range(0.18F, 1.20F),
                        .angle = rng_.range(-2.8F, -0.35F),
                        .accumulator = 0.0F,
                        .species = rng_.bounded(8U)
                    });
                }
                for (std::size_t index = 0; index < 5U; ++index)
                {
                    const auto kind = static_cast<StudioFieldKind>(rng_.bounded(3U));
                    fields_.push_back({
                        .position = {
                            rng_.range(bounds_.width * 0.15F, bounds_.width * 0.85F),
                            rng_.range(bounds_.height * 0.25F, bounds_.height * 0.78F)
                        },
                        .radius = rng_.range(90.0F, 210.0F),
                        .strength = rng_.range(180.0F, 430.0F),
                        .kind = kind
                    });
                }
                for (std::size_t index = 0; index < 6U; ++index)
                {
                    obstacles_.push_back({
                        .position = {
                            rng_.range(bounds_.width * 0.12F, bounds_.width * 0.88F),
                            rng_.range(bounds_.height * 0.45F, bounds_.height * 0.88F)
                        },
                        .radius = rng_.range(22.0F, 54.0F)
                    });
                }
            }

            static constexpr std::size_t maximum_particles_ = 15'000;
            static constexpr std::size_t maximum_emitters_ = 32;
            static constexpr std::size_t maximum_fields_ = 48;
            static constexpr std::size_t maximum_obstacles_ = 64;
            static constexpr float toolbar_height_ = 44.0F;
            static constexpr float gravity_ = 54.0F;
            static constexpr float restitution_ = 0.72F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            Pcg32 rng_{};
            ParticlePool pool_{ maximum_particles_ };
            std::vector<StudioEmitter> emitters_;
            std::vector<StudioField> fields_;
            std::vector<StudioObstacle> obstacles_;
            StudioTool tool_{ StudioTool::emitter };
            Vec2 cursor_{};
            std::uint64_t spawned_total_{};
            std::uint64_t collision_count_{};
            bool cursor_visible_{};
        };
    }

    std::unique_ptr<IScene> make_particle_studio_scene()
    {
        return std::make_unique<ParticleStudioScene>();
    }
}
