                GridCell& neighbor = at(
                    static_cast<std::uint32_t>(neighbor_x),
                    static_cast<std::uint32_t>(neighbor_y));
                if (neighbor.material == Material::water)
                {
                    cell.material = Material::smoke;
                    cell.age = 0;
                    neighbor.material = Material::empty;
                    moved_[current] = 1U;
                    return;
                }

                if ((neighbor.material == Material::wood || neighbor.material == Material::oil)
                    && coordinate_hash(neighbor_x, neighbor_y, tick, seed) % 37U == 0U)
                {
                    neighbor.material = Material::fire;
                    neighbor.age = 0;
                    neighbor.variant = static_cast<std::uint8_t>(
                        coordinate_hash(neighbor_x, neighbor_y, tick, seed) & 3U);
                }
            }

            if (cell.age > 70U + static_cast<std::uint16_t>(cell.variant) * 12U)
            {
                cell.material = Material::smoke;
                cell.age = 0;
            }
        }
        else if (cell.material == Material::smoke)
        {
            cell.age = increment_saturated(cell.age);
            if (cell.age > 220U
                && coordinate_hash(x, y, tick, seed) % 11U == 0U)
            {
                cell = {};
                moved_[current] = 1U;
            }
        }
        else if (cell.material == Material::acid)
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
                GridCell& neighbor = at(
                    static_cast<std::uint32_t>(neighbor_x),
                    static_cast<std::uint32_t>(neighbor_y));
                if ((neighbor.material == Material::wood || neighbor.material == Material::sand)
                    && coordinate_hash(neighbor_x, neighbor_y, tick, seed) % 53U == 0U)
                {
                    neighbor = {};
                    if (cell.age > 160U)
                        cell = {};
                    break;
                }
            }
        }
    }
}
