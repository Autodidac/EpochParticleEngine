#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <iostream>
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        class CellularAutomataScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "cellular-automata",
                    .name = "Cellular Automata",
                    .description = "Conway Life on a deterministic double-buffered grid with live editing."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                generation_ = 0;
                compute_disabled_ = false;
                last_update_used_compute_ = false;
                compute_storage_.clear();
                cells_.assign(static_cast<std::size_t>(width_) * height_, 0U);
                next_.assign(cells_.size(), 0U);

                Pcg32 random(seed_);
                for (std::uint32_t y = 0; y < height_; ++y)
                {
                    for (std::uint32_t x = 0; x < width_; ++x)
                    {
                        if (random.bounded(100U) < 14U)
                            cells_[index(x, y)] = 1U;
                    }
                }

                stamp_glider(8, 8);
                stamp_glider(static_cast<std::int32_t>(width_) - 14, 12);
                stamp_pulsar(
                    static_cast<std::int32_t>(width_ / 2U) - 6,
                    static_cast<std::int32_t>(height_ / 2U) - 6);
                recount();
            }

            void update(const SceneUpdateContext& context) override
            {
                if ((context.tick % 3U) != 0U)
                    return;

                last_update_used_compute_ = false;
                if (context.compute != nullptr
                    && !compute_disabled_
                    && update_compute(*context.compute))
                {
                    ++generation_;
                    recount();
                    return;
                }

                context.tasks.parallel_for(
                    height_,
                    8U,
                    [this](std::size_t begin, std::size_t end)
                    {
                        for (std::size_t row = begin; row < end; ++row)
                        {
                            const std::uint32_t y = static_cast<std::uint32_t>(row);
                            for (std::uint32_t x = 0; x < width_; ++x)
                            {
                                const int neighbors = neighbor_count(x, y);
                                const bool alive = cells_[index(x, y)] != 0U;
                                next_[index(x, y)] = static_cast<std::uint8_t>(
                                    neighbors == 3 || (alive && neighbors == 2));
                            }
                        }
                    });

                cells_.swap(next_);
                ++generation_;
                recount();
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                const float cell_width = bounds.width / static_cast<float>(width_);
                const float cell_height = bounds.height / static_cast<float>(height_);
                const Vec2 half_extent{ cell_width * 0.47F, cell_height * 0.47F };

                for (std::uint32_t y = 0; y < height_; ++y)
                {
                    for (std::uint32_t x = 0; x < width_; ++x)
                    {
                        if (cells_[index(x, y)] == 0U)
                            continue;

                        const int neighbors = neighbor_count(x, y);
                        const float heat = static_cast<float>(neighbors) / 8.0F;
                        frame.rectangle(
                            {
                                (static_cast<float>(x) + 0.5F) * cell_width,
                                (static_cast<float>(y) + 0.5F) * cell_height
                            },
                            half_extent,
                            lerp(
                                Color{ 0.10F, 0.42F, 0.68F, 1.0F },
                                Color{ 0.35F, 1.0F, 0.72F, 1.0F },
                                heat),
                            2);
                    }
                }
            }

            void pointer(const PointerEvent& event) override
            {
                bounds_ = bounds_.valid() ? bounds_ : Bounds{ 1.0F, 1.0F };
                const int center_x = static_cast<int>(
                    event.position.x / bounds_.width * static_cast<float>(width_));
                const int center_y = static_cast<int>(
                    event.position.y / bounds_.height * static_cast<float>(height_));
                if (event.primary_down)
                    paint(center_x, center_y, event.shift ? 6 : 3, true);
                if (event.secondary_down)
                    paint(center_x, center_y, event.shift ? 6 : 3, false);
            }

            void resize(Bounds, Bounds new_bounds) override
            {
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{
                    .particle_count = 0,
                    .active_cell_count = alive_count_
                };
                result.metrics[0] = { "GENERATION", static_cast<double>(generation_) };
                result.metrics[1] = { "GRID WIDTH", static_cast<double>(width_) };
                result.metrics[2] = { "GRID HEIGHT", static_cast<double>(height_) };
                result.metrics[3] = { "GPU COMPUTE", last_update_used_compute_ ? 1.0 : 0.0 };
                result.metric_count = 4;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(generation_);
                hasher.append_u32(width_);
                hasher.append_u32(height_);
                for (const std::uint8_t value : cells_)
                    hasher.append_byte(value);
                return hasher.value();
            }

        private:
            [[nodiscard]] bool update_compute(IComputeBackend& backend)
            {
                static constexpr std::string_view shader = R"glsl(
#version 450
layout(local_size_x = 64) in;
layout(std430, set = 0, binding = 0) buffer CellStorage
{
    uint words[];
} cells;

void main()
{
    uint cell = gl_GlobalInvocationID.x;
    uint count = cells.words[0];
    uint width = cells.words[1];
    uint height = cells.words[2];
    if (cell >= count)
        return;

    uint x = cell % width;
    uint y = cell / width;
    uint neighbors = 0u;
    for (int offset_y = -1; offset_y <= 1; ++offset_y)
    {
        for (int offset_x = -1; offset_x <= 1; ++offset_x)
        {
            if (offset_x == 0 && offset_y == 0)
                continue;
            uint neighbor_x = uint(
                (int(x) + offset_x + int(width)) % int(width));
            uint neighbor_y = uint(
                (int(y) + offset_y + int(height)) % int(height));
            neighbors += cells.words[
                4u + neighbor_y * width + neighbor_x] != 0u ? 1u : 0u;
        }
    }

    bool alive = cells.words[4u + cell] != 0u;
    cells.words[4u + count + cell] =
        neighbors == 3u || (alive && neighbors == 2u) ? 1u : 0u;
}
)glsl";

                const std::size_t cell_count = cells_.size();
                compute_storage_.assign(4U + cell_count * 2U, 0U);
                compute_storage_[0] = static_cast<std::uint32_t>(cell_count);
                compute_storage_[1] = width_;
                compute_storage_[2] = height_;
                for (std::size_t cell = 0; cell < cell_count; ++cell)
                    compute_storage_[4U + cell] = cells_[cell];

                std::span<std::uint32_t> words{ compute_storage_ };
                const auto result = backend.dispatch({
                    .program_id = "epoch.cellular.life.v1",
                    .shader_source = shader,
                    .storage = std::as_writable_bytes(words),
                    .push_constants = {},
                    .workgroup_count_x = static_cast<std::uint32_t>(
                        (cell_count + 63U) / 64U)
                });
                if (!result)
                {
                    compute_disabled_ = true;
                    std::clog << "Cellular compute fallback: " << result.error() << '\n';
                    return false;
                }

                for (std::size_t cell = 0; cell < cell_count; ++cell)
                {
                    cells_[cell] = static_cast<std::uint8_t>(
                        compute_storage_[4U + cell_count + cell] != 0U);
                }
                last_update_used_compute_ = true;
                return true;
            }

            [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y) const noexcept
            {
                return static_cast<std::size_t>(y) * width_ + x;
            }

            [[nodiscard]] int neighbor_count(std::uint32_t x, std::uint32_t y) const noexcept
            {
                int count = 0;
                for (int offset_y = -1; offset_y <= 1; ++offset_y)
                {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x)
                    {
                        if (offset_x == 0 && offset_y == 0)
                            continue;
                        const auto neighbor_x = static_cast<std::uint32_t>(
                            (static_cast<int>(x) + offset_x + static_cast<int>(width_))
                            % static_cast<int>(width_));
                        const auto neighbor_y = static_cast<std::uint32_t>(
                            (static_cast<int>(y) + offset_y + static_cast<int>(height_))
                            % static_cast<int>(height_));
                        count += cells_[index(neighbor_x, neighbor_y)] != 0U ? 1 : 0;
                    }
                }
                return count;
            }

            void paint(int center_x, int center_y, int radius, bool alive)
            {
                for (int y = -radius; y <= radius; ++y)
                {
                    for (int x = -radius; x <= radius; ++x)
                    {
                        if (x * x + y * y > radius * radius)
                            continue;
                        const int target_x = center_x + x;
                        const int target_y = center_y + y;
                        if (target_x < 0 || target_y < 0
                            || target_x >= static_cast<int>(width_)
                            || target_y >= static_cast<int>(height_))
                        {
                            continue;
                        }
                        cells_[index(
                            static_cast<std::uint32_t>(target_x),
                            static_cast<std::uint32_t>(target_y))] =
                            static_cast<std::uint8_t>(alive);
                    }
                }
                recount();
            }

            void stamp_glider(int x, int y)
            {
                constexpr std::array<std::pair<int, int>, 5> pattern{
                    std::pair{ 1, 0 }, std::pair{ 2, 1 }, std::pair{ 0, 2 },
                    std::pair{ 1, 2 }, std::pair{ 2, 2 }
                };
                for (const auto& [offset_x, offset_y] : pattern)
                {
                    const int target_x = x + offset_x;
                    const int target_y = y + offset_y;
                    if (target_x >= 0 && target_y >= 0
                        && target_x < static_cast<int>(width_)
                        && target_y < static_cast<int>(height_))
                    {
                        cells_[index(
                            static_cast<std::uint32_t>(target_x),
                            static_cast<std::uint32_t>(target_y))] = 1U;
                    }
                }
            }

            void stamp_pulsar(int x, int y)
            {
                constexpr std::array<int, 4> axes{ 0, 5, 7, 12 };
                constexpr std::array<int, 3> spans{ 2, 3, 4 };
                for (const int axis : axes)
                {
                    for (const int span : spans)
                    {
                        const std::array<std::pair<int, int>, 4> points{
                            std::pair{ x + axis, y + span },
                            std::pair{ x + axis, y + 12 - span },
                            std::pair{ x + span, y + axis },
                            std::pair{ x + 12 - span, y + axis }
                        };
                        for (const auto& [point_x, point_y] : points)
                        {
                            if (point_x >= 0 && point_y >= 0
                                && point_x < static_cast<int>(width_)
                                && point_y < static_cast<int>(height_))
                            {
                                cells_[index(
                                    static_cast<std::uint32_t>(point_x),
                                    static_cast<std::uint32_t>(point_y))] = 1U;
                            }
                        }
                    }
                }
            }

            void recount() noexcept
            {
                alive_count_ = static_cast<std::size_t>(
                    std::count(cells_.begin(), cells_.end(), std::uint8_t{ 1 }));
            }

            static constexpr std::uint32_t width_ = 192;
            static constexpr std::uint32_t height_ = 108;
            Bounds bounds_{};
            std::uint64_t seed_{};
            std::uint64_t generation_{};
            std::size_t alive_count_{};
            std::vector<std::uint8_t> cells_;
            std::vector<std::uint8_t> next_;
            std::vector<std::uint32_t> compute_storage_;
            bool compute_disabled_{};
            bool last_update_used_compute_{};
        };
    }

    std::unique_ptr<IScene> make_cellular_automata_scene()
    {
        return std::make_unique<CellularAutomataScene>();
    }
}
