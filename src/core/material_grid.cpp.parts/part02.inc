    bool MaterialGrid::is_solid_at(Vec2 position, Bounds bounds) const noexcept
    {
        if (!bounds.contains(position))
            return true;
        const auto [x, y] = world_to_cell(position, bounds);
        return is_solid(at(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)).material);
    }

    bool MaterialGrid::deposit(Vec2 position, Bounds bounds, Material material) noexcept
    {
        if (!bounds.contains(position))
            return false;
        const auto [x, y] = world_to_cell(position, bounds);
        GridCell& cell = at(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
        if (cell.material != Material::empty && is_solid(cell.material))
            return false;
        cell = GridCell{
            .material = material,
            .variant = static_cast<std::uint8_t>(coordinate_hash(x, y, 0, 0) & 3U),
            .age = 0
        };
        return true;
    }

    std::size_t MaterialGrid::count(Material material) const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            cells_.begin(),
            cells_.end(),
            [material](const GridCell& cell)
            {
                return cell.material == material;
            }));
    }

    std::span<const GridCell> MaterialGrid::cells() const noexcept
    {
        return cells_;
    }

    std::uint64_t MaterialGrid::state_hash() const noexcept
    {
        StableHasher hasher;
        hasher.append_u32(width_);
        hasher.append_u32(height_);
        for (const GridCell& cell : cells_)
        {
            hasher.append_byte(static_cast<std::uint8_t>(cell.material));
            hasher.append_byte(cell.variant);
            hasher.append_u32(cell.age);
        }
        return hasher.value();
    }

    Color MaterialGrid::material_color(Material material, std::uint8_t variant) noexcept
    {
        const float variation = static_cast<float>(variant & 3U) * 0.025F;
        switch (material)
        {
        case Material::sand: return { 0.83F + variation, 0.67F + variation, 0.28F, 1.0F };
        case Material::water: return { 0.12F, 0.42F + variation, 0.92F, 0.82F };
        case Material::stone: return { 0.35F + variation, 0.37F + variation, 0.41F + variation, 1.0F };
        case Material::wood: return { 0.45F + variation, 0.24F, 0.09F, 1.0F };
        case Material::fire: return { 1.0F, 0.28F + variation * 3.0F, 0.03F, 0.92F };
        case Material::smoke: return { 0.35F + variation, 0.36F + variation, 0.39F + variation, 0.62F };
        case Material::oil: return { 0.18F + variation, 0.12F, 0.08F, 0.95F };
        case Material::acid: return { 0.25F, 0.92F, 0.18F + variation, 0.88F };
        case Material::empty: break;
        }
        return { 0.0F, 0.0F, 0.0F, 0.0F };
    }

    bool MaterialGrid::is_solid(Material material) noexcept
    {
        return material == Material::sand || material == Material::stone
            || material == Material::wood;
    }

    bool MaterialGrid::is_liquid(Material material) noexcept
    {
        return material == Material::water || material == Material::oil
            || material == Material::acid;
    }

    bool MaterialGrid::is_gas(Material material) noexcept
    {
        return material == Material::smoke || material == Material::fire;
    }

    std::size_t MaterialGrid::index(std::uint32_t x, std::uint32_t y) const noexcept
    {
        return static_cast<std::size_t>(y) * width_ + x;
    }

    bool MaterialGrid::try_swap(
        std::int32_t from_x,
        std::int32_t from_y,
        std::int32_t to_x,
        std::int32_t to_y,
        Material moving_material) noexcept
    {
        if (!in_bounds(from_x, from_y) || !in_bounds(to_x, to_y))
            return false;

        const std::size_t source = index(
            static_cast<std::uint32_t>(from_x),
            static_cast<std::uint32_t>(from_y));
        const std::size_t target = index(
            static_cast<std::uint32_t>(to_x),
            static_cast<std::uint32_t>(to_y));
        if (moved_[source] != 0U || moved_[target] != 0U)
            return false;

        const Material target_material = cells_[target].material;
        bool allowed = target_material == Material::empty;

        if (moving_material == Material::sand)
            allowed = allowed || is_liquid(target_material) || is_gas(target_material);
        else if (is_liquid(moving_material))
            allowed = allowed || is_gas(target_material);
        else if (is_gas(moving_material))
            allowed = allowed && target_material == Material::empty;

        if (!allowed)
            return false;

        std::swap(cells_[source], cells_[target]);
        moved_[source] = 1U;
        moved_[target] = 1U;
        return true;
    }

    void MaterialGrid::update_reactive_cell(
        std::int32_t x,
        std::int32_t y,
        std::uint64_t tick,
        std::uint64_t seed) noexcept
    {
        const std::size_t current = index(
            static_cast<std::uint32_t>(x),
            static_cast<std::uint32_t>(y));
        GridCell& cell = cells_[current];

        if (cell.material == Material::fire)
        {
            cell.age = increment_saturated(cell.age);
            constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> neighbors{
                std::pair{ -1, 0 }, std::pair{ 1, 0 },
                std::pair{ 0, -1 }, std::pair{ 0, 1 }
            };

            for (const auto& [offset_x, offset_y] : neighbors)
            {
                const std::int32_t neighbor_x = x + offset_x;
                const std::int32_t neighbor_y = y + offset_y;
                if (!in_bounds(neighbor_x, neighbor_y))
                    continue;

