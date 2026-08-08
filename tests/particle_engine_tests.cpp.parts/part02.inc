        std::size_t invalid_query_visits = 0;
        default_grid.for_each_neighbor(
            { std::numeric_limits<float>::quiet_NaN(), 0.0F },
            1.0F,
            false,
            [&invalid_query_visits](std::uint32_t) { ++invalid_query_visits; });
        require(invalid_query_visits == 0, "UniformGridIndex accepted a non-finite query");

        UniformGridIndex tiny_wrap{ { 20.0F, 20.0F }, 10.0F };
        const std::array<Vec2, 4> tiny_positions{
            Vec2{ 2.0F, 2.0F }, Vec2{ 12.0F, 2.0F },
            Vec2{ 2.0F, 12.0F }, Vec2{ 12.0F, 12.0F }
        };
        tiny_wrap.build(tiny_positions);
        std::vector<std::uint32_t> unique_wrapped;
        tiny_wrap.for_each_neighbor({ 1.0F, 1.0F }, 1'000.0F, true,
            [&unique_wrapped](std::uint32_t index) { unique_wrapped.push_back(index); });
        std::ranges::sort(unique_wrapped);
        require(unique_wrapped == std::vector<std::uint32_t>({ 0U, 1U, 2U, 3U }),
            "UniformGridIndex repeated cells during a large wrapped query");

        require_throws<std::invalid_argument>(
            [] { UniformGridIndex invalid{ { 100.0F, 100.0F }, 0.0F }; },
            "UniformGridIndex accepted a zero cell size");
        require_throws<std::invalid_argument>(
            [] {
                UniformGridIndex invalid{
                    { std::numeric_limits<float>::infinity(), 100.0F },
                    10.0F
                };
            },
            "UniformGridIndex accepted infinite bounds");
        require_throws<std::length_error>(
            [] {
                UniformGridIndex invalid{
                    { std::numeric_limits<float>::max(), 1.0F },
                    std::numeric_limits<float>::min()
                };
            },
            "UniformGridIndex accepted dimensions beyond UINT32_MAX");
    }

    void test_material_grid()
    {
        MaterialGrid first{ 96, 64 };
        MaterialGrid second{ 96, 64 };
        for (MaterialGrid* grid : { &first, &second })
        {
            grid->fill_rectangle(0, 58, 96, 6, Material::stone);
            grid->fill_circle(32, 8, 7, Material::sand);
            grid->fill_circle(48, 8, 7, Material::water);
            grid->fill_rectangle(62, 35, 8, 20, Material::wood);
            grid->set(66, 34, Material::fire);
            grid->fill_circle(77, 8, 5, Material::acid);
        }

        for (std::uint64_t tick = 0; tick < 240; ++tick)
        {
            first.step_granular(tick, 1234);
            second.step_granular(tick, 1234);
            require(first.state_hash() == second.state_hash(), "MaterialGrid deterministic replay diverged");
        }
        require(first.size() == static_cast<std::size_t>(first.width()) * first.height(),
            "MaterialGrid dimensions are inconsistent");

        MaterialGrid default_grid;
        require(default_grid.width() == 1U && default_grid.height() == 1U
                && default_grid.size() == 1U,
            "Default MaterialGrid is not usable");

        MaterialGrid clipped{ 4, 4 };
        clipped.fill_rectangle(-2, -2, 4, 4, Material::sand);
        require(clipped.count(Material::sand) == 4U,
            "MaterialGrid rectangle clipping failed");
        clipped.clear();
        clipped.fill_rectangle(
            std::numeric_limits<std::int32_t>::max(),
            std::numeric_limits<std::int32_t>::max(),
            std::numeric_limits<std::int32_t>::max(),
            std::numeric_limits<std::int32_t>::max(),
            Material::sand);
        require(clipped.count(Material::sand) == 0U,
            "MaterialGrid overflow-safe rectangle clipping failed");
        clipped.fill_circle(0, 0, std::numeric_limits<std::int32_t>::max(), Material::water);
        require(clipped.count(Material::water) == clipped.size(),
            "MaterialGrid large-radius circle clipping failed");

        const auto [nan_x, infinity_y] = clipped.world_to_cell(
            { std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity() },
            { 100.0F, 100.0F });
        require(nan_x == 0 && infinity_y == 3,
            "MaterialGrid non-finite world-coordinate handling failed");

        MaterialGrid aging{ 1, 1 };
        aging.set(0, 0, Material::acid);
        aging.at(0, 0).age = std::numeric_limits<std::uint16_t>::max();
        aging.step_granular(1, 1);
        require(aging.at(0, 0).age == std::numeric_limits<std::uint16_t>::max(),
            "MaterialGrid cell age wrapped around");

        MaterialGrid retained{ 2, 2 };
        require_throws<std::length_error>(
            [&retained] {
                retained.resize(
                    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) + 1U,
                    1U);
            },
            "MaterialGrid accepted dimensions beyond INT32_MAX");
        require(retained.width() == 2U && retained.height() == 2U,
            "MaterialGrid lost its previous state after rejected dimensions");
    }

    void test_render_frame()
    {
        RenderFrame frame{ 3 };
        frame.begin({ 320.0F, 200.0F });
        frame.circle({ 20.0F, 20.0F }, 5.0F, {}, 10);
        frame.rectangle({ 30.0F, 30.0F }, { 4.0F, 4.0F }, {}, -2);
        frame.rounded_rectangle({ 40.0F, 40.0F }, { 4.0F, 4.0F }, 2.0F, {}, 5);
        frame.circle({ 50.0F, 50.0F }, 5.0F, {}, 1);
        frame.finalize();

        require(frame.items().size() == 3, "RenderFrame cap failed");
        require(frame.dropped_items() == 1, "RenderFrame dropped count failed");
        require(frame.items()[0].layer == -2 && frame.items()[1].layer == 5
                && frame.items()[2].layer == 10,
            "RenderFrame layer ordering failed");

        RenderFrame text_frame{ 8 };
        text_frame.begin({ 320.0F, 200.0F });
        text_frame.text(
            { 4.0F, 8.0F },
            "HI",
            TextSize{ 14.0F, 1.0F },
            { 1.0F, 1.0F, 1.0F, 1.0F });
        text_frame.finalize();
        require(text_frame.items().size() == 2U,
            "RenderFrame did not emit one GPU instance per glyph");
        require(text_frame.items()[0].shape == PrimitiveShape::bitmap_glyph
                && text_frame.items()[1].shape == PrimitiveShape::bitmap_glyph,
            "RenderFrame text did not use GPU glyph primitives");
        require(text_frame.items()[0].glyph_bitmap != 0U
                && text_frame.items()[1].glyph_bitmap != 0U,
            "RenderFrame emitted an empty GPU glyph bitmap");

