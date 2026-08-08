        for (std::int64_t row = start_y; row <= end_y; ++row)
        {
            const std::int64_t delta_y = row - center_y;
            for (std::int64_t column = start_x; column <= end_x; ++column)
            {
                const std::int64_t delta_x = column - center_x;
                if (delta_x * delta_x + delta_y * delta_y <= radius_squared)
                {
                    at(static_cast<std::uint32_t>(column), static_cast<std::uint32_t>(row)) = {
                        .material = material,
                        .variant = 0,
                        .age = 0
                    };
                }
            }
        }
    }

    void MaterialGrid::step_granular(std::uint64_t tick, std::uint64_t seed)
    {
        std::fill(moved_.begin(), moved_.end(), std::uint8_t{ 0 });

        const bool scan_left_to_right = (tick & 1U) == 0U;
        for (std::int32_t y = static_cast<std::int32_t>(height_) - 1; y >= 0; --y)
        {
            for (std::uint32_t scan = 0; scan < width_; ++scan)
            {
                const std::int32_t x = scan_left_to_right
                    ? static_cast<std::int32_t>(scan)
                    : static_cast<std::int32_t>(width_ - 1U - scan);
                const std::size_t source_index =
                    index(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                if (moved_[source_index] != 0U)
                    continue;

                update_reactive_cell(x, y, tick, seed);
                if (moved_[source_index] != 0U)
                    continue;

                const Material material = cells_[source_index].material;
                if (material == Material::empty || material == Material::stone
                    || material == Material::wood)
                {
                    continue;
                }

                const std::uint32_t noise = coordinate_hash(x, y, tick, seed);
                const std::int32_t direction = (noise & 1U) == 0U ? -1 : 1;

                if (material == Material::sand)
                {
                    if (try_swap(x, y, x, y + 1, material))
                        continue;
                    if (try_swap(x, y, x + direction, y + 1, material))
                        continue;
                    static_cast<void>(try_swap(x, y, x - direction, y + 1, material));
                }
                else if (material == Material::water || material == Material::oil
                    || material == Material::acid)
                {
                    if (try_swap(x, y, x, y + 1, material))
                        continue;
                    if (try_swap(x, y, x + direction, y + 1, material))
                        continue;
                    if (try_swap(x, y, x - direction, y + 1, material))
                        continue;

                    for (std::int32_t distance = 1; distance <= 4; ++distance)
                    {
                        if (try_swap(x, y, x + direction * distance, y, material))
                            break;
                        if (!in_bounds(x + direction * distance, y)
                            || at(
                                static_cast<std::uint32_t>(x + direction * distance),
                                static_cast<std::uint32_t>(y)).material != Material::empty)
                        {
                            break;
                        }
                    }
                }
                else if (material == Material::smoke || material == Material::fire)
                {
                    if (try_swap(x, y, x, y - 1, material))
                        continue;
                    if (try_swap(x, y, x + direction, y - 1, material))
                        continue;
                    if (try_swap(x, y, x - direction, y - 1, material))
                        continue;
                    static_cast<void>(try_swap(x, y, x + direction, y, material));
                }
            }
        }
    }

    void MaterialGrid::render(RenderFrame& frame, Bounds bounds, std::int32_t layer) const
    {
        if (!bounds.valid())
            return;

        const float cell_width = bounds.width / static_cast<float>(width_);
        const float cell_height = bounds.height / static_cast<float>(height_);
        const Vec2 half_extent{ cell_width * 0.52F, cell_height * 0.52F };

        for (std::uint32_t y = 0; y < height_; ++y)
        {
            for (std::uint32_t x = 0; x < width_; ++x)
            {
                const GridCell& cell = at(x, y);
                if (cell.material == Material::empty)
                    continue;

                frame.rectangle(
                    {
                        (static_cast<float>(x) + 0.5F) * cell_width,
                        (static_cast<float>(y) + 0.5F) * cell_height
                    },
                    half_extent,
                    material_color(cell.material, cell.variant),
                    layer);
            }
        }
    }

    std::pair<std::int32_t, std::int32_t> MaterialGrid::world_to_cell(
        Vec2 position,
        Bounds bounds) const noexcept
    {
        if (!bounds.valid())
            return { 0, 0 };

        return {
            world_coordinate_to_cell(position.x, bounds.width, width_),
            world_coordinate_to_cell(position.y, bounds.height, height_)
        };
    }

    Vec2 MaterialGrid::cell_center(
        std::int32_t x,
        std::int32_t y,
        Bounds bounds) const noexcept
    {
        if (!bounds.valid() || !in_bounds(x, y))
            return {};
        return {
            (static_cast<float>(x) + 0.5F) * bounds.width / static_cast<float>(width_),
            (static_cast<float>(y) + 0.5F) * bounds.height / static_cast<float>(height_)
        };
    }

