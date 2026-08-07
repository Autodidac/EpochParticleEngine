#include <epochengine/particle/material_grid.hpp>
#include <epochengine/particle/random.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace epochengine::particle
{
    namespace
    {
        [[nodiscard]] constexpr std::uint16_t increment_saturated(
            std::uint16_t value) noexcept
        {
            return value == std::numeric_limits<std::uint16_t>::max()
                ? value
                : static_cast<std::uint16_t>(value + 1U);
        }

        [[nodiscard]] std::int32_t world_coordinate_to_cell(
            float coordinate,
            float extent,
            std::uint32_t dimension) noexcept
        {
            if (std::isnan(coordinate) || coordinate <= 0.0F)
                return 0;
            if (!std::isfinite(coordinate) || coordinate >= extent)
                return static_cast<std::int32_t>(dimension - 1U);

            const double scaled = std::floor(
                static_cast<double>(coordinate)
                / static_cast<double>(extent)
                * static_cast<double>(dimension));
            return static_cast<std::int32_t>(std::clamp(
                scaled,
                0.0,
                static_cast<double>(dimension - 1U)));
        }
    }

    MaterialGrid::MaterialGrid()
    {
        resize(1U, 1U);
    }

    MaterialGrid::MaterialGrid(std::uint32_t width, std::uint32_t height)
    {
        resize(width, height);
    }

    void MaterialGrid::resize(std::uint32_t width, std::uint32_t height)
    {
        const std::uint32_t new_width = std::max(width, 1U);
        const std::uint32_t new_height = std::max(height, 1U);
        constexpr auto maximum_coordinate = static_cast<std::uint32_t>(
            std::numeric_limits<std::int32_t>::max());
        if (new_width > maximum_coordinate || new_height > maximum_coordinate)
            throw std::length_error("MaterialGrid dimensions exceed INT32_MAX");

        const std::size_t width_size = static_cast<std::size_t>(new_width);
        const std::size_t height_size = static_cast<std::size_t>(new_height);
        if (width_size > std::numeric_limits<std::size_t>::max() / height_size)
            throw std::length_error("MaterialGrid cell count overflows size_t");

        const std::size_t cell_count = width_size * height_size;
        if (cell_count > cells_.max_size() || cell_count > moved_.max_size())
            throw std::length_error("MaterialGrid cell count exceeds container limits");

        std::vector<GridCell> new_cells(cell_count);
        std::vector<std::uint8_t> new_moved(cell_count, 0U);
        width_ = new_width;
        height_ = new_height;
        cells_.swap(new_cells);
        moved_.swap(new_moved);
    }

    void MaterialGrid::clear() noexcept
    {
        std::fill(cells_.begin(), cells_.end(), GridCell{});
        std::fill(moved_.begin(), moved_.end(), 0U);
    }

    std::uint32_t MaterialGrid::width() const noexcept { return width_; }
    std::uint32_t MaterialGrid::height() const noexcept { return height_; }
    std::size_t MaterialGrid::size() const noexcept { return cells_.size(); }

    bool MaterialGrid::in_bounds(std::int32_t x, std::int32_t y) const noexcept
    {
        return x >= 0 && y >= 0
            && x < static_cast<std::int32_t>(width_)
            && y < static_cast<std::int32_t>(height_);
    }

    const GridCell& MaterialGrid::at(std::uint32_t x, std::uint32_t y) const noexcept
    {
        return cells_[index(x, y)];
    }

    GridCell& MaterialGrid::at(std::uint32_t x, std::uint32_t y) noexcept
    {
        return cells_[index(x, y)];
    }

    void MaterialGrid::set(
        std::int32_t x,
        std::int32_t y,
        Material material,
        std::uint8_t variant) noexcept
    {
        if (!in_bounds(x, y))
            return;

        GridCell& cell = at(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
        cell = {
            .material = material,
            .variant = variant,
            .age = 0
        };
    }

    void MaterialGrid::fill_rectangle(
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height,
        Material material) noexcept
    {
        if (width <= 0 || height <= 0)
            return;

        const std::int64_t start_x = std::max<std::int64_t>(0, x);
        const std::int64_t start_y = std::max<std::int64_t>(0, y);
        const std::int64_t end_x = std::min<std::int64_t>(
            width_,
            static_cast<std::int64_t>(x) + width);
        const std::int64_t end_y = std::min<std::int64_t>(
            height_,
            static_cast<std::int64_t>(y) + height);
        if (start_x >= end_x || start_y >= end_y)
            return;

        for (std::int64_t row = start_y; row < end_y; ++row)
        {
            for (std::int64_t column = start_x; column < end_x; ++column)
            {
                at(static_cast<std::uint32_t>(column), static_cast<std::uint32_t>(row)) = {
                    .material = material,
                    .variant = 0,
                    .age = 0
                };
            }
        }
    }

    void MaterialGrid::fill_circle(
        std::int32_t center_x,
        std::int32_t center_y,
        std::int32_t radius,
        Material material) noexcept
    {
        if (radius < 0)
            return;

        const std::int64_t radius_64 = radius;
        const std::int64_t radius_squared = radius_64 * radius_64;
        const std::int64_t start_x = std::max<std::int64_t>(
            0,
            static_cast<std::int64_t>(center_x) - radius_64);
        const std::int64_t start_y = std::max<std::int64_t>(
            0,
            static_cast<std::int64_t>(center_y) - radius_64);
        const std::int64_t end_x = std::min<std::int64_t>(
            static_cast<std::int64_t>(width_) - 1,
            static_cast<std::int64_t>(center_x) + radius_64);
        const std::int64_t end_y = std::min<std::int64_t>(
            static_cast<std::int64_t>(height_) - 1,
            static_cast<std::int64_t>(center_y) + radius_64);
        if (start_x > end_x || start_y > end_y)
            return;

