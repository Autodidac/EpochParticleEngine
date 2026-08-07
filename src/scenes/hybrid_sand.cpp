#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/material_grid.hpp>
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
        class HybridSandScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "hybrid-sand",
                    .name = "Hybrid Sand Lab",
                    .description = "Falling-material cells coupled to ballistic particles that collide and deposit."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                random_.reseed(seed_);
                grid_.resize(grid_width_, grid_height_);
                dynamic_particles_.clear();
                dynamic_particles_.reserve(maximum_dynamic_particles_);
                deposited_last_step_ = 0;

                grid_.fill_rectangle(
                    0,
                    static_cast<std::int32_t>(grid_height_) - 6,
                    static_cast<std::int32_t>(grid_width_),
                    6,
                    Material::stone);
                grid_.fill_rectangle(0, 0, 3, static_cast<std::int32_t>(grid_height_), Material::stone);
                grid_.fill_rectangle(
                    static_cast<std::int32_t>(grid_width_) - 3,
                    0,
                    3,
                    static_cast<std::int32_t>(grid_height_),
                    Material::stone);

                grid_.fill_rectangle(28, 88, 58, 4, Material::stone);
                grid_.fill_rectangle(154, 74, 52, 4, Material::stone);
                grid_.fill_rectangle(95, 111, 50, 3, Material::wood);
                grid_.fill_circle(56, 75, 18, Material::sand);
                grid_.fill_rectangle(158, 95, 42, 15, Material::water);
                grid_.fill_rectangle(102, 34, 28, 4, Material::stone);
                grid_.fill_rectangle(114, 0, 4, 24, Material::stone);
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                deposited_last_step_ = 0;
                grid_.step_granular(context.tick, seed_);

                if ((context.tick % 5U) == 0U
                    && dynamic_particles_.size() < maximum_dynamic_particles_)
                {
                    const Vec2 origin{
                        bounds_.width * 0.49F,
                        bounds_.height * 0.10F
                    };
                    spawn_dynamic_sand(origin, 4U, { 0.0F, 20.0F });
                }

                dynamic_particles_.age(context.delta_seconds);
                auto positions = dynamic_particles_.positions();
                auto velocities = dynamic_particles_.velocities();
                auto flags = dynamic_particles_.flags();
                const auto ids = dynamic_particles_.ids();

                for (std::size_t index = 0; index < dynamic_particles_.size(); ++index)
                {
                    Vec2 position = positions[index];
                    Vec2 velocity = velocities[index];
                    const Vec2 old_position = position;

                    const auto [cell_x, cell_y] = grid_.world_to_cell(position, bounds_);
                    const Material current_material = grid_.at(
                        static_cast<std::uint32_t>(cell_x),
                        static_cast<std::uint32_t>(cell_y)).material;
                    if (MaterialGrid::is_liquid(current_material))
                    {
                        velocity *= 0.92F;
                        velocity.y -= 65.0F * context.delta_seconds;
                    }

                    velocity.y += gravity_ * context.delta_seconds;
                    velocity.x *= 0.999F;
                    position += velocity * context.delta_seconds;

                    if (position.x < 3.0F)
                    {
                        position.x = 3.0F;
                        velocity.x = std::abs(velocity.x) * restitution_;
                    }
                    else if (position.x > bounds_.width - 3.0F)
                    {
                        position.x = bounds_.width - 3.0F;
                        velocity.x = -std::abs(velocity.x) * restitution_;
                    }

                    if (position.y < 3.0F)
                    {
                        position.y = 3.0F;
                        velocity.y = std::abs(velocity.y) * restitution_;
                    }
                    else if (position.y > bounds_.height - 3.0F)
                    {
                        position.y = bounds_.height - 3.0F;
                        velocity.y = -std::abs(velocity.y) * restitution_;
                    }

                    if ((flags[index] & particle_collides_with_grid) != 0U
                        && grid_.is_solid_at(position, bounds_))
                    {
                        const bool vertical_collision = grid_.is_solid_at(
                            { old_position.x, position.y },
                            bounds_);
                        const bool horizontal_collision = grid_.is_solid_at(
                            { position.x, old_position.y },
                            bounds_);

                        position = old_position;
                        if (vertical_collision)
                            velocity.y = -velocity.y * restitution_;
                        if (horizontal_collision)
                            velocity.x = -velocity.x * restitution_;
                        if (!vertical_collision && !horizontal_collision)
                            velocity = -velocity * restitution_;

                        const float speed_squared = length_squared(velocity);
                        if ((flags[index] & particle_can_deposit) != 0U
                            && speed_squared < deposit_speed_ * deposit_speed_
                            && grid_.deposit(old_position, bounds_, Material::sand))
                        {
                            dynamic_particles_.mark_dead(index);
                            ++deposited_last_step_;
                            if (context.events.size() < 256U)
                            {
                                context.events.push_back({
                                    .type = SimulationEventType::material_deposited,
                                    .position = old_position,
                                    .intensity = 1.0F,
                                    .source_id = ids[index]
                                });
                            }
                        }
                    }

                    positions[index] = position;
                    velocities[index] = velocity;
                }

                dynamic_particles_.compact_stable();
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                grid_.render(frame, bounds, 1);
                const auto positions = dynamic_particles_.positions();
                const auto velocities = dynamic_particles_.velocities();
                for (std::size_t index = 0; index < dynamic_particles_.size(); ++index)
                {
                    const float speed = length(velocities[index]);
                    const Color color = lerp(
                        Color{ 0.90F, 0.70F, 0.23F, 1.0F },
                        Color{ 1.0F, 0.96F, 0.65F, 1.0F },
                        std::clamp(speed / 280.0F, 0.0F, 1.0F));
                    frame.circle(positions[index], 2.4F, color, 3);
                    frame.line(
                        positions[index],
                        positions[index] - velocities[index] * 0.018F,
                        1.0F,
                        with_alpha(color, 0.38F),
                        2);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                const auto [cell_x, cell_y] = grid_.world_to_cell(event.position, bounds_);
                const int radius = event.shift ? 8 : 4;

                if (event.primary_down)
                {
                    Material material = Material::sand;
                    if (event.shift)
                        material = Material::stone;
                    else if (event.control)
                        material = Material::fire;
                    grid_.fill_circle(cell_x, cell_y, radius, material);

                    if (material == Material::sand
                        && dynamic_particles_.size() < maximum_dynamic_particles_)
                    {
                        spawn_dynamic_sand(event.position, 6U, { 0.0F, -35.0F });
                    }
                }

                if (event.secondary_down)
                {
                    const Material material = event.control
                        ? Material::acid
                        : event.shift ? Material::oil : Material::water;
                    grid_.fill_circle(cell_x, cell_y, radius, material);
                }
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                for (Vec2& position : dynamic_particles_.positions())
                    detail::rescale_position(position, old_bounds, new_bounds);
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                const std::size_t active_cells =
                    grid_.size() - grid_.count(Material::empty);
                SceneStats result{
                    .particle_count = dynamic_particles_.size(),
                    .active_cell_count = active_cells
                };
                result.metrics[0] = {
                    "SAND CELLS",
                    static_cast<double>(grid_.count(Material::sand))
                };
                result.metrics[1] = {
                    "WATER CELLS",
                    static_cast<double>(grid_.count(Material::water))
                };
                result.metrics[2] = {
                    "DEPOSITED",
                    static_cast<double>(deposited_last_step_)
                };
                result.metric_count = 3;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(random_.state());
                hasher.append_u64(grid_.state_hash());
                hasher.append_u64(dynamic_particles_.state_hash());
                return hasher.value();
            }

        private:
            void spawn_dynamic_sand(Vec2 origin, std::size_t count, Vec2 base_velocity)
            {
                const std::size_t available = maximum_dynamic_particles_
                    - std::min(maximum_dynamic_particles_, dynamic_particles_.size());
                count = std::min(count, available);
                for (std::size_t index = 0; index < count; ++index)
                {
                    dynamic_particles_.spawn({
                        .position = {
                            origin.x + random_.range(-7.0F, 7.0F),
                            origin.y + random_.range(-3.0F, 3.0F)
                        },
                        .velocity = {
                            base_velocity.x + random_.range(-45.0F, 45.0F),
                            base_velocity.y + random_.range(-18.0F, 18.0F)
                        },
                        .color = { 0.92F, 0.73F, 0.26F, 1.0F },
                        .radius = 2.4F,
                        .lifetime = 18.0F,
                        .inverse_mass = 1.0F,
                        .species = 0,
                        .flags = particle_collides_with_grid | particle_can_deposit
                    });
                }
            }

            static constexpr std::uint32_t grid_width_ = 240;
            static constexpr std::uint32_t grid_height_ = 135;
            static constexpr std::size_t maximum_dynamic_particles_ = 4'096;
            static constexpr float gravity_ = 520.0F;
            static constexpr float restitution_ = 0.43F;
            static constexpr float deposit_speed_ = 38.0F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            Pcg32 random_{};
            MaterialGrid grid_{ grid_width_, grid_height_ };
            ParticlePool dynamic_particles_{ maximum_dynamic_particles_ };
            std::size_t deposited_last_step_{};
        };
    }

    std::unique_ptr<IScene> make_hybrid_sand_scene()
    {
        return std::make_unique<HybridSandScene>();
    }
}
