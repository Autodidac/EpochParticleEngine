
        const std::size_t cell_count = width_size * height_size;
        if (cell_count > cells_.max_size() || cell_count > moved_.max_size())
            throw std::length_error("MaterialGrid cell count exceeds container limits");

        std::vector<GridCell> new_cells(cell_count);
        std::vector<std::uint8_t> new_moved(cell_count, std::uint8_t{ 0 });
        width_ = new_width;
        height_ = new_height;
        cells_.swap(new_cells);
        moved_.swap(new_moved);
    }

    void MaterialGrid::clear() noexcept
    {
        std::fill(cells_.begin(), cells_.end(), GridCell{});
        std::fill(moved_.begin(), moved_.end(), std::uint8_t{ 0 });
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