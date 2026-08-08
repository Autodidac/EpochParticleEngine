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
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        struct ReactionCell
        {
            float a{ 1.0F };
            float b{};
        };

        class ReactionDiffusionScene final : public IScene
        {
        public:
            ReactionDiffusionScene()
                : current_(grid_width_ * grid_height_)
                , next_(grid_width_ * grid_height_)
            {
            }

            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "reaction-diffusion",
                    .name = "Reaction Diffusion",
                    .description = "Gray-Scott grid chemistry producing deterministic coral, maze, and spot patterns."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                pointer_erases_ = false;
                active_cells_ = 0;
                average_b_ = 0.0;
                std::fill(current_.begin(), current_.end(), ReactionCell{});
                std::fill(next_.begin(), next_.end(), ReactionCell{});

                Pcg32 random(seed_ ^ 0x5245414354494f4eULL);
                for (int cluster = 0; cluster < 18; ++cluster)
                {
                    const int center_x = random.range(8, static_cast<std::int32_t>(grid_width_ - 8U));
                    const int center_y = random.range(8, static_cast<std::int32_t>(grid_height_ - 8U));
                    const int radius = random.range(2, 6);
                    inject_grid(center_x, center_y, radius, 1.0F);
                }
            }

            void update(const SceneUpdateContext&) override
            {
                if (pointer_active_)
                {
                    const int x = static_cast<int>(std::clamp(
                        pointer_position_.x / std::max(bounds_.width, 1.0F)
                            * static_cast<float>(grid_width_),
                        0.0F,
                        static_cast<float>(grid_width_ - 1U)));
                    const int y = static_cast<int>(std::clamp(
                        pointer_position_.y / std::max(bounds_.height, 1.0F)
                            * static_cast<float>(grid_height_),
                        0.0F,
                        static_cast<float>(grid_height_ - 1U)));
                    inject_grid(x, y, 4, pointer_erases_ ? 0.0F : 1.0F);
                }

                std::uint64_t active = 0;
                double b_sum = 0.0;
                for (std::size_t y = 0; y < grid_height_; ++y)
                {
                    for (std::size_t x = 0; x < grid_width_; ++x)
                    {
                        const ReactionCell center = current_[index(x, y)];
                        const float laplace_a = laplacian(x, y, true);
                        const float laplace_b = laplacian(x, y, false);
                        const float reaction = center.a * center.b * center.b;
                        ReactionCell value{
                            .a = center.a
                                + diffusion_a_ * laplace_a
                                - reaction
                                + feed_ * (1.0F - center.a),
                            .b = center.b
                                + diffusion_b_ * laplace_b
                                + reaction
                                - (kill_ + feed_) * center.b
                        };
                        value.a = std::clamp(value.a, 0.0F, 1.0F);
                        value.b = std::clamp(value.b, 0.0F, 1.0F);
                        next_[index(x, y)] = value;
                        if (value.b > 0.12F)
                            ++active;
                        b_sum += static_cast<double>(value.b);
                    }
                }
                current_.swap(next_);
                active_cells_ = active;
                average_b_ = b_sum / static_cast<double>(current_.size());
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                const float cell_width = bounds.width / static_cast<float>(grid_width_);
                const float cell_height = bounds.height / static_cast<float>(grid_height_);
                const Vec2 half_extent{
                    std::max(0.55F, cell_width * 0.52F),
                    std::max(0.55F, cell_height * 0.52F)
                };

                for (std::size_t y = 0; y < grid_height_; ++y)
                {
                    for (std::size_t x = 0; x < grid_width_; ++x)
                    {
                        const ReactionCell cell = current_[index(x, y)];
                        const float intensity = std::clamp(cell.b * 1.55F, 0.0F, 1.0F);
                        const float inverse = std::clamp(1.0F - cell.a, 0.0F, 1.0F);
                        const Color color{
                            0.025F + intensity * 0.14F,
                            0.04F + intensity * 0.62F,
                            0.08F + inverse * 0.36F + intensity * 0.48F,
                            1.0F
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

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        18.0F,
                        pointer_erases_
                            ? Color{ 1.0F, 0.28F, 0.24F, 0.55F }
                            : Color{ 0.30F, 0.92F, 1.0F, 0.55F },
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

            void resize(Bounds, Bounds new_bounds) override
            {
                if (new_bounds.valid())
                    bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{};
                result.active_cell_count = active_cells_;
                result.metrics[0] = { "Grid cells", static_cast<double>(current_.size()) };
                result.metrics[1] = { "Active B", static_cast<double>(active_cells_) };
                result.metrics[2] = { "Mean B", average_b_ };
                result.metrics[3] = { "Feed", static_cast<double>(feed_) };
                result.metrics[4] = { "Kill", static_cast<double>(kill_) };
                result.metric_count = 5;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                for (const ReactionCell& cell : current_)
                {
                    hasher.append_float(cell.a);
                    hasher.append_float(cell.b);
                }
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

            [[nodiscard]] static constexpr std::size_t index(
                std::size_t x,
                std::size_t y) noexcept
            {
                return y * grid_width_ + x;
            }

            [[nodiscard]] float component(
                std::ptrdiff_t x,
                std::ptrdiff_t y,
                bool a_component) const noexcept
            {
                const ReactionCell cell = current_[index(wrap_x(x), wrap_y(y))];
                return a_component ? cell.a : cell.b;
            }

            [[nodiscard]] float laplacian(
                std::size_t x,
                std::size_t y,
                bool a_component) const noexcept
            {
                const std::ptrdiff_t px = static_cast<std::ptrdiff_t>(x);
                const std::ptrdiff_t py = static_cast<std::ptrdiff_t>(y);
                const float center = component(px, py, a_component);
                const float cardinal =
                    component(px - 1, py, a_component)
                    + component(px + 1, py, a_component)
                    + component(px, py - 1, a_component)
                    + component(px, py + 1, a_component);
                const float diagonal =
                    component(px - 1, py - 1, a_component)
                    + component(px + 1, py - 1, a_component)
                    + component(px - 1, py + 1, a_component)
                    + component(px + 1, py + 1, a_component);
                return cardinal * 0.20F + diagonal * 0.05F - center;
            }

            void inject_grid(int center_x, int center_y, int radius, float amount) noexcept
            {
                for (int offset_y = -radius; offset_y <= radius; ++offset_y)
                {
                    for (int offset_x = -radius; offset_x <= radius; ++offset_x)
                    {
                        if (offset_x * offset_x + offset_y * offset_y > radius * radius)
                            continue;
                        ReactionCell& cell = current_[index(
                            wrap_x(static_cast<std::ptrdiff_t>(center_x + offset_x)),
                            wrap_y(static_cast<std::ptrdiff_t>(center_y + offset_y)))];
                        if (amount > 0.5F)
                        {
                            cell.a = 0.15F;
                            cell.b = amount;
                        }
                        else
                        {
                            cell.a = 1.0F;
                            cell.b = 0.0F;
                        }
                    }
                }
            }

            static constexpr std::size_t grid_width_ = 144;
            static constexpr std::size_t grid_height_ = 81;
            static constexpr float diffusion_a_ = 1.0F;
            static constexpr float diffusion_b_ = 0.50F;
            static constexpr float feed_ = 0.055F;
            static constexpr float kill_ = 0.062F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            std::vector<ReactionCell> current_;
            std::vector<ReactionCell> next_;
            Vec2 pointer_position_{};
            std::uint64_t active_cells_{};
            double average_b_{};
            bool pointer_active_{};
            bool pointer_erases_{};
        };
    }

    std::unique_ptr<IScene> make_reaction_diffusion_scene()
    {
        return std::make_unique<ReactionDiffusionScene>();
    }
}
