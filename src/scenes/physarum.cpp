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
        struct PhysarumAgent
        {
            Vec2 position{};
            float angle{};
        };

        class PhysarumScene final : public IScene
        {
        public:
            PhysarumScene()
                : trail_(grid_width_ * grid_height_)
                , next_trail_(grid_width_ * grid_height_)
            {
            }

            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "physarum",
                    .name = "Physarum Trails",
                    .description = "Agent/grid hybrid inspired by slime mold transport networks and chemotaxis."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                pointer_erases_ = false;
                average_trail_ = 0.0;
                std::fill(trail_.begin(), trail_.end(), 0.0F);
                std::fill(next_trail_.begin(), next_trail_.end(), 0.0F);

                agents_.clear();
                agents_.reserve(agent_count_);
                Pcg32 random(seed_ ^ 0x504859534152554dULL);
                const Vec2 center = bounds_.center();
                const float radius = std::min(bounds_.width, bounds_.height) * 0.24F;
                for (std::size_t index = 0; index < agent_count_; ++index)
                {
                    const float angle = random.range(
                        0.0F,
                        std::numbers::pi_v<float> * 2.0F);
                    const float distance = radius * std::sqrt(random.unit_float());
                    agents_.push_back({
                        .position = center + Vec2{
                            std::cos(angle) * distance,
                            std::sin(angle) * distance
                        },
                        .angle = random.range(
                            0.0F,
                            std::numbers::pi_v<float> * 2.0F)
                    });
                }
            }

            void update(const SceneUpdateContext& context) override
            {
                if (pointer_active_)
                    paint_pointer(pointer_erases_ ? 0.0F : 1.0F);

                const float move_distance = move_speed_ * context.delta_seconds;
                for (std::size_t index = 0; index < agents_.size(); ++index)
                {
                    PhysarumAgent& agent = agents_[index];
                    const float forward = sense(agent, 0.0F);
                    const float left = sense(agent, -sensor_angle_);
                    const float right = sense(agent, sensor_angle_);

                    if (forward < left || forward < right)
                    {
                        if (left > right)
                        {
                            agent.angle -= turn_angle_;
                        }
                        else if (right > left)
                        {
                            agent.angle += turn_angle_;
                        }
                        else
                        {
                            const std::uint32_t choice = coordinate_hash(
                                static_cast<std::int32_t>(index),
                                static_cast<std::int32_t>(index >> 4U),
                                context.tick,
                                seed_);
                            agent.angle += (choice & 1U) == 0U
                                ? -turn_angle_
                                : turn_angle_;
                        }
                    }

                    agent.position += Vec2{
                        std::cos(agent.angle) * move_distance,
                        std::sin(agent.angle) * move_distance
                    };
                    agent.position = detail::wrapped(agent.position, bounds_);
                    trail_[trail_index(agent.position)] = 1.0F;
                }

                double sum = 0.0;
                for (std::size_t y = 0; y < grid_height_; ++y)
                {
                    for (std::size_t x = 0; x < grid_width_; ++x)
                    {
                        float total = 0.0F;
                        for (int offset_y = -1; offset_y <= 1; ++offset_y)
                        {
                            for (int offset_x = -1; offset_x <= 1; ++offset_x)
                            {
                                total += trail_[grid_index(
                                    wrap_x(static_cast<std::ptrdiff_t>(x) + offset_x),
                                    wrap_y(static_cast<std::ptrdiff_t>(y) + offset_y))];
                            }
                        }
                        const float value = std::clamp(
                            total / 9.0F * trail_decay_,
                            0.0F,
                            1.0F);
                        next_trail_[grid_index(x, y)] = value;
                        sum += static_cast<double>(value);
                    }
                }
                trail_.swap(next_trail_);
                average_trail_ = sum / static_cast<double>(trail_.size());
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                const float cell_width = bounds.width / static_cast<float>(grid_width_);
                const float cell_height = bounds.height / static_cast<float>(grid_height_);
                const Vec2 half_extent{
                    std::max(0.65F, cell_width * 0.53F),
                    std::max(0.65F, cell_height * 0.53F)
                };

                for (std::size_t y = 0; y < grid_height_; ++y)
                {
                    for (std::size_t x = 0; x < grid_width_; ++x)
                    {
                        const float intensity = trail_[grid_index(x, y)];
                        if (intensity < 0.018F)
                            continue;
                        const Color color{
                            0.06F + intensity * 0.16F,
                            0.07F + intensity * 0.76F,
                            0.12F + intensity * 0.54F,
                            std::clamp(0.10F + intensity * 0.82F, 0.0F, 0.92F)
                        };
                        frame.rectangle(
                            {
                                (static_cast<float>(x) + 0.5F) * cell_width,
                                (static_cast<float>(y) + 0.5F) * cell_height
                            },
                            half_extent,
                            color,
                            0);
                    }
                }

                for (std::size_t index = 0; index < agents_.size(); index += 2U)
                {
                    const PhysarumAgent& agent = agents_[index];
                    frame.circle(
                        agent.position,
                        1.45F,
                        detail::species_color(static_cast<std::uint32_t>(index % 8U)),
                        4);
                }

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        16.0F,
                        pointer_erases_
                            ? Color{ 1.0F, 0.28F, 0.24F, 0.65F }
                            : Color{ 0.30F, 1.0F, 0.68F, 0.65F },
                        20);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                pointer_position_ = event.position;
                if (event.action == PointerAction::press)
                {
                    pointer_active_ = true;
                    pointer_erases_ = event.button == PointerButton::secondary;
                }
                else if (event.action == PointerAction::release)
                {
                    pointer_active_ = event.primary_down || event.secondary_down;
                    pointer_erases_ = event.secondary_down;
                }
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                if (!new_bounds.valid())
                    return;
                for (PhysarumAgent& agent : agents_)
                    detail::rescale_position(agent.position, old_bounds, new_bounds);
                detail::rescale_position(pointer_position_, old_bounds, new_bounds);
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                std::uint64_t active_cells = 0;
                for (const float value : trail_)
                    active_cells += value > 0.05F ? 1U : 0U;

                SceneStats result{};
                result.particle_count = static_cast<std::uint64_t>(agents_.size());
                result.active_cell_count = active_cells;
                result.metrics[0] = { "Agents", static_cast<double>(agents_.size()) };
                result.metrics[1] = { "Trail cells", static_cast<double>(active_cells) };
                result.metrics[2] = { "Mean trail", average_trail_ };
                result.metrics[3] = { "Sensor angle", static_cast<double>(sensor_angle_) };
                result.metric_count = 4;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                for (const PhysarumAgent& agent : agents_)
                {
                    hasher.append_float(agent.position.x);
                    hasher.append_float(agent.position.y);
                    hasher.append_float(agent.angle);
                }
                for (const float value : trail_)
                    hasher.append_float(value);
                return hasher.value();
            }

        private:
            [[nodiscard]] static constexpr std::size_t wrap_x(std::ptrdiff_t x) noexcept
            {
                const std::ptrdiff_t width = static_cast<std::ptrdiff_t>(grid_width_);
                x %= width;
                if (x < 0)
                    x += width;
                return static_cast<std::size_t>(x);
            }

            [[nodiscard]] static constexpr std::size_t wrap_y(std::ptrdiff_t y) noexcept
            {
                const std::ptrdiff_t height = static_cast<std::ptrdiff_t>(grid_height_);
                y %= height;
                if (y < 0)
                    y += height;
                return static_cast<std::size_t>(y);
            }

            [[nodiscard]] static constexpr std::size_t grid_index(
                std::size_t x,
                std::size_t y) noexcept
            {
                return y * grid_width_ + x;
            }

            [[nodiscard]] std::size_t trail_index(Vec2 position) const noexcept
            {
                const float x = std::clamp(
                    position.x / std::max(bounds_.width, 1.0F),
                    0.0F,
                    0.999999F);
                const float y = std::clamp(
                    position.y / std::max(bounds_.height, 1.0F),
                    0.0F,
                    0.999999F);
                return grid_index(
                    static_cast<std::size_t>(x * static_cast<float>(grid_width_)),
                    static_cast<std::size_t>(y * static_cast<float>(grid_height_)));
            }

            [[nodiscard]] float sense(
                const PhysarumAgent& agent,
                float angle_offset) const noexcept
            {
                const float angle = agent.angle + angle_offset;
                const Vec2 sensor = detail::wrapped(
                    agent.position + Vec2{
                        std::cos(angle) * sensor_distance_,
                        std::sin(angle) * sensor_distance_
                    },
                    bounds_);
                return trail_[trail_index(sensor)];
            }

            void paint_pointer(float value) noexcept
            {
                const std::size_t center = trail_index(pointer_position_);
                const std::size_t center_x = center % grid_width_;
                const std::size_t center_y = center / grid_width_;
                constexpr int radius = 4;
                for (int y = -radius; y <= radius; ++y)
                {
                    for (int x = -radius; x <= radius; ++x)
                    {
                        if (x * x + y * y > radius * radius)
                            continue;
                        trail_[grid_index(
                            wrap_x(static_cast<std::ptrdiff_t>(center_x) + x),
                            wrap_y(static_cast<std::ptrdiff_t>(center_y) + y))] = value;
                    }
                }
            }

            static constexpr std::size_t grid_width_ = 128;
            static constexpr std::size_t grid_height_ = 72;
            static constexpr std::size_t agent_count_ = 1'600;
            static constexpr float sensor_distance_ = 12.0F;
            static constexpr float sensor_angle_ = 0.62F;
            static constexpr float turn_angle_ = 0.48F;
            static constexpr float move_speed_ = 88.0F;
            static constexpr float trail_decay_ = 0.935F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            std::vector<PhysarumAgent> agents_;
            std::vector<float> trail_;
            std::vector<float> next_trail_;
            Vec2 pointer_position_{};
            double average_trail_{};
            bool pointer_active_{};
            bool pointer_erases_{};
        };
    }

    std::unique_ptr<IScene> make_physarum_scene()
    {
        return std::make_unique<PhysarumScene>();
    }
}
