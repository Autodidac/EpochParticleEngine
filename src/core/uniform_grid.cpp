#include <epochengine/particle/uniform_grid.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace epochengine::particle
{
    UniformGridIndex::UniformGridIndex()
    {
        configure({}, 1.0F);
    }

    UniformGridIndex::UniformGridIndex(Bounds bounds, float cell_size)
    {
        configure(bounds, cell_size);
    }

    void UniformGridIndex::configure(Bounds bounds, float cell_size)
    {
        if (!bounds.valid())
            throw std::invalid_argument("UniformGridIndex bounds must be finite and positive");
        if (!std::isfinite(cell_size) || cell_size <= 0.0F)
            throw std::invalid_argument("UniformGridIndex cell_size must be finite and positive");

        const double column_count = std::max(
            1.0,
            std::ceil(static_cast<double>(bounds.width) / static_cast<double>(cell_size)));
        const double row_count = std::max(
            1.0,
            std::ceil(static_cast<double>(bounds.height) / static_cast<double>(cell_size)));
        constexpr double maximum_dimension =
            static_cast<double>(std::numeric_limits<std::uint32_t>::max());
        if (column_count > maximum_dimension || row_count > maximum_dimension)
            throw std::length_error("UniformGridIndex dimensions exceed UINT32_MAX");

        const auto columns = static_cast<std::uint32_t>(column_count);
        const auto rows = static_cast<std::uint32_t>(row_count);
        const std::size_t columns_size = static_cast<std::size_t>(columns);
        const std::size_t rows_size = static_cast<std::size_t>(rows);
        if (columns_size > std::numeric_limits<std::size_t>::max() / rows_size)
            throw std::length_error("UniformGridIndex cell count overflows size_t");

        const std::size_t cell_count = columns_size * rows_size;
        if (cell_count == std::numeric_limits<std::size_t>::max()
            || cell_count + 1U > offsets_.max_size()
            || cell_count > write_cursors_.max_size())
        {
            throw std::length_error("UniformGridIndex cell count exceeds container limits");
        }

        std::vector<std::size_t> new_offsets(cell_count + 1U, 0U);
        std::vector<std::size_t> new_write_cursors(cell_count, 0U);

        bounds_ = bounds;
        cell_size_ = cell_size;
        columns_ = columns;
        rows_ = rows;
        offsets_.swap(new_offsets);
        write_cursors_.swap(new_write_cursors);
        indices_.clear();
    }

    void UniformGridIndex::build(std::span<const Vec2> positions)
    {
        if (positions.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("UniformGridIndex supports at most UINT32_MAX entries");

        std::fill(offsets_.begin(), offsets_.end(), 0U);
        for (const Vec2 position : positions)
            ++offsets_[cell_index(position) + 1U];

        for (std::size_t index = 1; index < offsets_.size(); ++index)
            offsets_[index] += offsets_[index - 1U];

        indices_.resize(positions.size());
        std::copy(offsets_.begin(), offsets_.end() - 1, write_cursors_.begin());

        for (std::size_t particle = 0; particle < positions.size(); ++particle)
        {
            const std::size_t cell = cell_index(positions[particle]);
            indices_[write_cursors_[cell]++] = static_cast<std::uint32_t>(particle);
        }
    }

    Bounds UniformGridIndex::bounds() const noexcept { return bounds_; }
    float UniformGridIndex::cell_size() const noexcept { return cell_size_; }
    std::uint32_t UniformGridIndex::columns() const noexcept { return columns_; }
    std::uint32_t UniformGridIndex::rows() const noexcept { return rows_; }
    std::size_t UniformGridIndex::indexed_count() const noexcept { return indices_.size(); }

    std::size_t UniformGridIndex::cell_index(Vec2 position) const noexcept
    {
        const auto coordinate_to_cell = [this](
            float coordinate,
            std::uint32_t dimension) noexcept
        {
            if (std::isnan(coordinate) || coordinate <= 0.0F)
                return std::uint32_t{ 0 };
            if (!std::isfinite(coordinate))
                return dimension - 1U;

            const double raw_cell = std::floor(
                static_cast<double>(coordinate) / static_cast<double>(cell_size_));
            return static_cast<std::uint32_t>(std::clamp(
                raw_cell,
                0.0,
                static_cast<double>(dimension - 1U)));
        };

        const std::uint32_t column = coordinate_to_cell(position.x, columns_);
        const std::uint32_t row = coordinate_to_cell(position.y, rows_);
        return static_cast<std::size_t>(row) * columns_ + column;
    }
}
