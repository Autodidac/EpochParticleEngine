#pragma once

#include "export.hpp"
#include "types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace epochengine::particle
{
    class EPOCH_PARTICLE_API UniformGridIndex
    {
    public:
        UniformGridIndex();
        UniformGridIndex(Bounds bounds, float cell_size);

        void configure(Bounds bounds, float cell_size);
        void build(std::span<const Vec2> positions);

        [[nodiscard]] Bounds bounds() const noexcept;
        [[nodiscard]] float cell_size() const noexcept;
        [[nodiscard]] std::uint32_t columns() const noexcept;
        [[nodiscard]] std::uint32_t rows() const noexcept;
        [[nodiscard]] std::size_t indexed_count() const noexcept;

        template<class Function>
        void for_each_neighbor(
            Vec2 position,
            float radius,
            bool wrap,
            Function&& function) const
        {
            if (offsets_.empty() || columns_ == 0 || rows_ == 0
                || !std::isfinite(position.x) || !std::isfinite(position.y)
                || !std::isfinite(radius) || radius < 0.0F)
            {
                return;
            }

            const auto coordinate_to_cell = [this, wrap](
                float coordinate,
                float extent,
                std::uint32_t dimension) noexcept
            {
                double value = static_cast<double>(coordinate);
                const double extent_value = static_cast<double>(extent);
                if (wrap)
                {
                    value = std::fmod(value, extent_value);
                    if (value < 0.0)
                        value += extent_value;
                }
                else
                {
                    if (value <= 0.0)
                        return std::uint32_t{ 0 };
                    if (value >= extent_value)
                        return dimension - 1U;
                }

                const double raw_cell = std::floor(value / static_cast<double>(cell_size_));
                return static_cast<std::uint32_t>(std::clamp(
                    raw_cell,
                    0.0,
                    static_cast<double>(dimension - 1U)));
            };

            const std::uint32_t center_x = coordinate_to_cell(
                position.x,
                bounds_.width,
                columns_);
            const std::uint32_t center_y = coordinate_to_cell(
                position.y,
                bounds_.height,
                rows_);

            const double raw_reach = std::ceil(
                static_cast<double>(radius) / static_cast<double>(cell_size_));
            const std::uint64_t maximum_reach = std::max<std::uint32_t>(columns_, rows_);
            const std::uint64_t reach = raw_reach >= static_cast<double>(maximum_reach)
                ? maximum_reach
                : static_cast<std::uint64_t>(raw_reach);

            const auto visit_cell = [this, &function](
                std::uint32_t cell_x,
                std::uint32_t cell_y)
            {
                const std::size_t cell = static_cast<std::size_t>(cell_y)
                    * columns_ + cell_x;
                const std::size_t begin = offsets_[cell];
                const std::size_t end = offsets_[cell + 1U];
                for (std::size_t cursor = begin; cursor < end; ++cursor)
                    function(indices_[cursor]);
            };

            const auto visit_columns = [&, this](std::uint32_t cell_y)
            {
                if (!wrap)
                {
                    const std::uint64_t start = reach > center_x ? 0U : center_x - reach;
                    const std::uint64_t end = std::min<std::uint64_t>(
                        static_cast<std::uint64_t>(columns_ - 1U),
                        static_cast<std::uint64_t>(center_x) + reach);
                    for (std::uint64_t cell_x = start; cell_x <= end; ++cell_x)
                        visit_cell(static_cast<std::uint32_t>(cell_x), cell_y);
                    return;
                }

                if (reach >= columns_ / 2U)
                {
                    for (std::uint32_t cell_x = 0; cell_x < columns_; ++cell_x)
                        visit_cell(cell_x, cell_y);
                    return;
                }

                const std::int64_t signed_reach = static_cast<std::int64_t>(reach);
                for (std::int64_t offset = -signed_reach; offset <= signed_reach; ++offset)
                {
                    const std::uint32_t cell_x = positive_modulo(
                        static_cast<std::int64_t>(center_x) + offset,
                        columns_);
                    visit_cell(cell_x, cell_y);
                }
            };

            if (!wrap)
            {
                const std::uint64_t start = reach > center_y ? 0U : center_y - reach;
                const std::uint64_t end = std::min<std::uint64_t>(
                    static_cast<std::uint64_t>(rows_ - 1U),
                    static_cast<std::uint64_t>(center_y) + reach);
                for (std::uint64_t cell_y = start; cell_y <= end; ++cell_y)
                    visit_columns(static_cast<std::uint32_t>(cell_y));
                return;
            }

            if (reach >= rows_ / 2U)
            {
                for (std::uint32_t cell_y = 0; cell_y < rows_; ++cell_y)
                    visit_columns(cell_y);
                return;
            }

            const std::int64_t signed_reach = static_cast<std::int64_t>(reach);
            for (std::int64_t offset = -signed_reach; offset <= signed_reach; ++offset)
            {
                const std::uint32_t cell_y = positive_modulo(
                    static_cast<std::int64_t>(center_y) + offset,
                    rows_);
                visit_columns(cell_y);
            }
        }

    private:
        [[nodiscard]] static std::uint32_t positive_modulo(
            std::int64_t value,
            std::uint32_t divisor) noexcept
        {
            const std::int64_t signed_divisor = static_cast<std::int64_t>(divisor);
            const std::int64_t remainder = value % signed_divisor;
            return static_cast<std::uint32_t>(
                remainder < 0 ? remainder + signed_divisor : remainder);
        }

        [[nodiscard]] std::size_t cell_index(Vec2 position) const noexcept;

        Bounds bounds_{};
        float cell_size_{ 1.0F };
        std::uint32_t columns_{ 1 };
        std::uint32_t rows_{ 1 };
        std::vector<std::size_t> offsets_;
        std::vector<std::uint32_t> indices_;
        std::vector<std::size_t> write_cursors_;
    };
}
