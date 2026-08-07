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
        class FireSmokeScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "fire-smoke",
                    .name = "Fire And Smoke",
                    .description = "Reactive fire, smoke, water, oil and wood cells with ballistic embers."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                random_.reseed(seed_);
                grid_.resize(grid_width_, grid_height_);
                embers_.clear();
                embers_.reserve(maximum_embers_);
                ignitions_last_step_ = 0;

                grid_.fill_rectangle(
                    0,
                    static_cast<std::int32_t>(grid_height_) - 5,
                    static_cast<std::int32_t>(grid_width_),
                    5,
                    Material::stone);

                build_structure(24, 68, 40, 35);
                build_structure(78, 60, 45, 43);
                build_structure(139, 73, 32, 30);
                grid_.fill_rectangle(52, 93, 22, 10, Material::oil);
                grid_.fill_rectangle(126, 93, 10, 10, Material::water);
                grid_.fill_circle(98, 98, 4, Material::fire);
            }

            void update(const SceneUpdateContext& context) override
            {
                bounds_ = context.bounds;
                ignitions_last_step_ = 0;
                grid_.step_granular(context.tick, seed_);

                if ((context.tick & 1U) == 0U)
                    spawn_embers_from_fire(context.tick);

                embers_.age(context.delta_seconds);
                auto positions = embers_.positions();
                auto velocities = embers_.velocities();
                auto colors = embers_.colors();
                const auto lifetimes = embers_.lifetimes();
                const auto ids = embers_.ids();

                const float wind = std::sin(static_cast<float>(context.tick) * 0.018F) * 36.0F;
                for (std::size_t index = 0; index < embers_.size(); ++index)
                {
                    Vec2 velocity = velocities[index];
                    velocity.x += wind * context.delta_seconds;
                    velocity.y -= 24.0F * context.delta_seconds;
                    velocity *= 0.987F;

                    Vec2 position = positions[index] + velocity * context.delta_seconds;
                    if (position.x < 0.0F || position.x >= bounds_.width
                        || position.y < 0.0F || position.y >= bounds_.height)
                    {
                        embers_.mark_dead(index);
                        continue;
                    }

                    const auto [cell_x, cell_y] = grid_.world_to_cell(position, bounds_);
                    GridCell& cell = grid_.at(
                        static_cast<std::uint32_t>(cell_x),
                        static_cast<std::uint32_t>(cell_y));
                    if (cell.material == Material::wood
                        && coordinate_hash(cell_x, cell_y, context.tick, seed_) % 101U == 0U)
                    {
                        cell.material = Material::fire;
                        cell.age = 0;
                        ++ignitions_last_step_;
                        embers_.mark_dead(index);
                        if (context.events.size() < 256U)
                        {
                            context.events.push_back({
                                .type = SimulationEventType::ignition,
                                .position = position,
                                .intensity = 0.6F,
                                .source_id = ids[index]
                            });
                        }
                    }

                    positions[index] = position;
                    velocities[index] = velocity;
                    const float alpha = std::clamp(lifetimes[index] / 2.0F, 0.0F, 1.0F);
                    colors[index] = {
                        1.0F,
                        0.25F + alpha * 0.55F,
                        0.03F,
                        alpha
                    };
                }

                embers_.compact_stable();
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                grid_.render(frame, bounds, 1);
                const auto positions = embers_.positions();
                const auto colors = embers_.colors();
                const auto velocities = embers_.velocities();
                for (std::size_t index = 0; index < embers_.size(); ++index)
                {
                    frame.line(
                        positions[index],
                        positions[index] - velocities[index] * 0.025F,
                        1.3F,
                        with_alpha(colors[index], colors[index].a * 0.65F),
                        2);
                    frame.circle(positions[index], 1.5F, colors[index], 3);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                const auto [cell_x, cell_y] = grid_.world_to_cell(event.position, bounds_);
                const int radius = event.shift ? 7 : 4;
                if (event.primary_down)
                {
                    const Material material = event.control
                        ? Material::smoke
                        : event.shift ? Material::oil : Material::fire;
                    grid_.fill_circle(cell_x, cell_y, radius, material);
                }
                if (event.secondary_down)
                {
                    const Material material = event.shift
                        ? Material::water
                        : Material::wood;
                    grid_.fill_circle(cell_x, cell_y, radius, material);
                }
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                for (Vec2& position : embers_.positions())
                    detail::rescale_position(position, old_bounds, new_bounds);
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                const std::size_t active_cells =
                    grid_.size() - grid_.count(Material::empty);
                SceneStats result{
                    .particle_count = embers_.size(),
                    .active_cell_count = active_cells
                };
                result.metrics[0] = {
                    "FIRE CELLS",
                    static_cast<double>(grid_.count(Material::fire))
                };
                result.metrics[1] = {
                    "SMOKE CELLS",
                    static_cast<double>(grid_.count(Material::smoke))
                };
                result.metrics[2] = {
                    "IGNITIONS",
                    static_cast<double>(ignitions_last_step_)
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
                hasher.append_u64(embers_.state_hash());
                return hasher.value();
            }

        private:
            void build_structure(int x, int y, int width, int height)
            {
                grid_.fill_rectangle(x, y, width, 3, Material::wood);
                grid_.fill_rectangle(x, y, 3, height, Material::wood);
                grid_.fill_rectangle(x + width - 3, y, 3, height, Material::wood);
                grid_.fill_rectangle(x, y + height - 3, width, 3, Material::wood);
                grid_.fill_rectangle(x + width / 2 - 1, y + 4, 3, height - 8, Material::wood);
            }

            void spawn_embers_from_fire(std::uint64_t tick)
            {
                if (embers_.size() >= maximum_embers_)
                    return;

                const auto cells = grid_.cells();
                for (int attempt = 0; attempt < 24; ++attempt)
                {
                    const std::size_t candidate = random_.bounded(
                        static_cast<std::uint32_t>(cells.size()));
                    if (cells[candidate].material != Material::fire)
                        continue;

                    const int x = static_cast<int>(candidate % grid_width_);
                    const int y = static_cast<int>(candidate / grid_width_);
                    const Vec2 origin = grid_.cell_center(x, y, bounds_);
                    const std::size_t count = 1U + random_.bounded(2U);
                    for (std::size_t index = 0;
                        index < count && embers_.size() < maximum_embers_;
                        ++index)
                    {
                        embers_.spawn({
                            .position = origin,
                            .velocity = {
                                random_.range(-32.0F, 32.0F),
                                random_.range(-110.0F, -45.0F)
                            },
                            .color = { 1.0F, 0.72F, 0.10F, 1.0F },
                            .radius = 1.5F,
                            .lifetime = random_.range(0.8F, 2.4F),
                            .inverse_mass = 1.0F,
                            .species = 0,
                            .flags = particle_additive
                        });
                    }
                    break;
                }
                static_cast<void>(tick);
            }

            static constexpr std::uint32_t grid_width_ = 192;
            static constexpr std::uint32_t grid_height_ = 108;
            static constexpr std::size_t maximum_embers_ = 2'048;

            Bounds bounds_{};
            std::uint64_t seed_{};
            Pcg32 random_{};
            MaterialGrid grid_{ grid_width_, grid_height_ };
            ParticlePool embers_{ maximum_embers_ };
            std::size_t ignitions_last_step_{};
        };
    }

    std::unique_ptr<IScene> make_fire_smoke_scene()
    {
        return std::make_unique<FireSmokeScene>();
    }
}
