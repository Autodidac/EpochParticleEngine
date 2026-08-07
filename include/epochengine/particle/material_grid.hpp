#pragma once

#include "export.hpp"
#include "hash.hpp"
#include "render_frame.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace epochengine::particle
{
    enum class Material : std::uint8_t
    {
        empty,
        sand,
        water,
        stone,
        wood,
        fire,
        smoke,
        oil,
        acid
    };

    struct GridCell
    {
        Material material{ Material::empty };
        std::uint8_t variant{};
        std::uint16_t age{};
    };

    class EPOCH_PARTICLE_API MaterialGrid
    {
    public:
        MaterialGrid();
        MaterialGrid(std::uint32_t width, std::uint32_t height);

        void resize(std::uint32_t width, std::uint32_t height);
        void clear() noexcept;

        [[nodiscard]] std::uint32_t width() const noexcept;
        [[nodiscard]] std::uint32_t height() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool in_bounds(std::int32_t x, std::int32_t y) const noexcept;

        [[nodiscard]] const GridCell& at(std::uint32_t x, std::uint32_t y) const noexcept;
        [[nodiscard]] GridCell& at(std::uint32_t x, std::uint32_t y) noexcept;
        void set(std::int32_t x, std::int32_t y, Material material, std::uint8_t variant = 0) noexcept;

        void fill_rectangle(
            std::int32_t x,
            std::int32_t y,
            std::int32_t width,
            std::int32_t height,
            Material material) noexcept;
        void fill_circle(
            std::int32_t center_x,
            std::int32_t center_y,
            std::int32_t radius,
            Material material) noexcept;

        void step_granular(std::uint64_t tick, std::uint64_t seed);
        void render(RenderFrame& frame, Bounds bounds, std::int32_t layer = 0) const;

        [[nodiscard]] std::pair<std::int32_t, std::int32_t> world_to_cell(
            Vec2 position,
            Bounds bounds) const noexcept;
        [[nodiscard]] Vec2 cell_center(
            std::int32_t x,
            std::int32_t y,
            Bounds bounds) const noexcept;
        [[nodiscard]] bool is_solid_at(Vec2 position, Bounds bounds) const noexcept;
        [[nodiscard]] bool deposit(Vec2 position, Bounds bounds, Material material) noexcept;

        [[nodiscard]] std::size_t count(Material material) const noexcept;
        [[nodiscard]] std::span<const GridCell> cells() const noexcept;
        [[nodiscard]] std::uint64_t state_hash() const noexcept;

        [[nodiscard]] static Color material_color(Material material, std::uint8_t variant) noexcept;
        [[nodiscard]] static bool is_solid(Material material) noexcept;
        [[nodiscard]] static bool is_liquid(Material material) noexcept;
        [[nodiscard]] static bool is_gas(Material material) noexcept;

    private:
        [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y) const noexcept;
        [[nodiscard]] bool try_swap(
            std::int32_t from_x,
            std::int32_t from_y,
            std::int32_t to_x,
            std::int32_t to_y,
            Material moving_material) noexcept;
        void update_reactive_cell(
            std::int32_t x,
            std::int32_t y,
            std::uint64_t tick,
            std::uint64_t seed) noexcept;

        std::uint32_t width_{};
        std::uint32_t height_{};
        std::vector<GridCell> cells_;
        std::vector<std::uint8_t> moved_;
    };
}
