        RenderFrame sanitized{ 4 };
        sanitized.begin({ std::numeric_limits<float>::quiet_NaN(), 200.0F });
        require(sanitized.extent().width == 1.0F && sanitized.extent().height == 1.0F,
            "RenderFrame retained an invalid extent");
        sanitized.circle(
            { std::numeric_limits<float>::quiet_NaN(), 0.0F },
            1.0F,
            {});
        sanitized.add({
            .center = {},
            .half_extent = { -3.0F, -4.0F },
            .color = { 1.0F, 1.0F, 1.0F, 2.0F },
            .shape = PrimitiveShape::rounded_rectangle,
            .corner_radius = -5.0F
        });
        sanitized.finalize();
        require(sanitized.items().size() == 1U && sanitized.dropped_items() == 1U,
            "RenderFrame did not reject a non-finite item");
        require(sanitized.items()[0].half_extent.x == 0.0F
                && sanitized.items()[0].half_extent.y == 0.0F
                && sanitized.items()[0].corner_radius == 0.0F
                && sanitized.items()[0].color.a == 1.0F,
            "RenderFrame item sanitization failed");
    }

    void test_simulation_scheduler()
    {
        std::vector<std::unique_ptr<IScene>> scenes;
        scenes.push_back(std::make_unique<CountingScene>());
        Simulation simulation({
            .fixed_step = std::chrono::milliseconds(10),
            .maximum_catch_up_steps = 2,
            .seed = 7,
            .worker_count = 0
        }, std::move(scenes));

        simulation.advance(-std::chrono::milliseconds(10));
        require(simulation.tick() == 0U, "Simulation accepted negative elapsed time");

        simulation.set_time_scale(std::numeric_limits<double>::quiet_NaN());
        require(simulation.time_scale() == 1.0, "Simulation retained a NaN time scale");
        simulation.set_time_scale(std::numeric_limits<double>::infinity());
        require(simulation.time_scale() == 1.0, "Simulation retained an infinite time scale");

        simulation.advance(std::chrono::milliseconds(5));
        require(simulation.tick() == 0U, "Simulation stepped before a complete fixed interval");
        simulation.advance(std::chrono::milliseconds(5));
        require(simulation.tick() == 1U, "Simulation failed to consume a complete fixed interval");

        simulation.set_paused(true);
        simulation.advance(std::chrono::seconds(1));
        require(simulation.tick() == 1U, "Paused Simulation advanced");
        simulation.step_once();
        require(simulation.tick() == 2U, "Simulation single-step failed while paused");

        simulation.set_paused(false);
        simulation.reset();
        simulation.set_time_scale(0.5);
        simulation.advance(std::chrono::milliseconds(20));
        require(simulation.tick() == 1U, "Simulation time scale was not applied");

        simulation.reset();
        simulation.set_time_scale(1.0);
        simulation.advance(std::chrono::milliseconds(500));
        require(simulation.tick() == 2U,
            "Simulation catch-up work budget failed");
        require(simulation.events().size() == 2U,
            "Simulation event aggregation across catch-up steps failed");

        for (std::uint32_t frame = 0; frame < 24U; ++frame)
            simulation.advance(std::chrono::nanoseconds{ 1 });
        require(simulation.tick() == 50U,
            "Simulation discarded fixed-step backlog after a long frame");
        const std::uint64_t long_frame_hash = simulation.state_hash();

        simulation.reset();
        for (std::uint32_t frame = 0; frame < 50U; ++frame)
            simulation.advance(std::chrono::milliseconds{ 10 });
        require(simulation.tick() == 50U,
            "Simulation did not consume short-frame fixed steps");
        require(simulation.state_hash() == long_frame_hash,
            "Simulation state depends on elapsed-time frame chunking");

        const auto make_fractional_simulation = [] {
            std::vector<std::unique_ptr<IScene>> fractional_scenes;
            fractional_scenes.push_back(std::make_unique<CountingScene>());
            return Simulation({
                .fixed_step = std::chrono::nanoseconds{ 1 },
                .maximum_catch_up_steps = 4,
                .seed = 9,
                .worker_count = 0
            }, std::move(fractional_scenes));
        };

        auto whole_fraction = make_fractional_simulation();
        auto split_fraction = make_fractional_simulation();
        whole_fraction.set_time_scale(0.5);
        split_fraction.set_time_scale(0.5);
        whole_fraction.advance(std::chrono::nanoseconds{ 2 });
        split_fraction.advance(std::chrono::nanoseconds{ 1 });
        split_fraction.advance(std::chrono::nanoseconds{ 1 });
        require(whole_fraction.tick() == 1U
                && split_fraction.tick() == 1U,
            "Fractional time scale depends on elapsed-time frame chunking");
        require(whole_fraction.state_hash() == split_fraction.state_hash(),
            "Fractional scaled-time remainder changed simulation state");


        const Bounds original = simulation.bounds();
        simulation.resize({ std::numeric_limits<float>::infinity(), 720.0F });
        require(simulation.bounds().width == original.width
                && simulation.bounds().height == original.height,
            "Simulation accepted invalid resize bounds");
    }
    void require_finite_frame(const RenderFrame& frame)
    {
        for (const RenderItem& item : frame.items())
        {
            require(std::isfinite(item.center.x) && std::isfinite(item.center.y),
                "Scene rendered a non-finite center");
            require(std::isfinite(item.half_extent.x) && std::isfinite(item.half_extent.y),
                "Scene rendered a non-finite extent");
            require(item.half_extent.x >= 0.0F && item.half_extent.y >= 0.0F,
                "Scene rendered a negative extent");
            require(std::isfinite(item.color.r) && std::isfinite(item.color.g)
                    && std::isfinite(item.color.b) && std::isfinite(item.color.a),
                "Scene rendered a non-finite color");
        }
    }

    void test_all_scene_determinism()
    {
        constexpr std::uint64_t seed = 0x0123456789abcdefULL;
        constexpr Bounds bounds{ 1280.0F, 720.0F };
        const int ticks = configured_scene_ticks();

        Simulation single({
            .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
            .maximum_catch_up_steps = 8,
            .seed = seed,
            .worker_count = 0
        });
        Simulation parallel({
            .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
            .maximum_catch_up_steps = 8,
            .seed = seed,
            .worker_count = 4
        });

        require(single.scene_count() == 15, "Default scene suite is incomplete");
        require(parallel.scene_count() == single.scene_count(), "Scene suites differ by worker count");
        single.resize(bounds);
        parallel.resize(bounds);

        RenderFrame frame{ 600'000 };
        for (std::size_t scene = 0; scene < single.scene_count(); ++scene)
        {
            single.select_scene(scene);
            parallel.select_scene(scene);
            single.reset();
            parallel.reset();
            require(single.state_hash() == parallel.state_hash(), "Scene reset differs by worker count");

