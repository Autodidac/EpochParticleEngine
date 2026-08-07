    void test_task_arena()
    {
        constexpr std::size_t count = 20'000;
        for (int repetition = 0; repetition < 20; ++repetition)
        {
            TaskArena arena{ 4 };
            std::vector<std::atomic_uint32_t> visits(count);
            arena.parallel_for(count, 37, [&visits](std::size_t begin, std::size_t end)
            {
                for (std::size_t index = begin; index < end; ++index)
                    visits[index].fetch_add(1, std::memory_order_relaxed);
            });
            for (const auto& visit : visits)
                require(visit.load(std::memory_order_relaxed) == 1, "TaskArena missed or duplicated work");
        }

        TaskArena source{ 2 };
        TaskArena destination{ std::move(source) };
        require(source.worker_count() == 0U, "Moved-from TaskArena retained workers");
        std::size_t serial_visits = 0;
        source.parallel_for(17, 0, [&serial_visits](std::size_t begin, std::size_t end)
        {
            serial_visits += end - begin;
        });
        require(serial_visits == 17U, "Moved-from TaskArena did not preserve serial execution");

        require_throws<std::runtime_error>(
            [&destination]
            {
                destination.parallel_for(100, 1, [](std::size_t begin, std::size_t)
                {
                    if (begin == 0U)
                        throw std::runtime_error("expected");
                });
            },
            "TaskArena failed to propagate a worker exception");

        std::atomic_size_t recovered_visits{};
        destination.parallel_for(100, 7, [&recovered_visits](std::size_t begin, std::size_t end)
        {
            recovered_visits.fetch_add(end - begin, std::memory_order_relaxed);
        });
        require(recovered_visits.load(std::memory_order_relaxed) == 100U,
            "TaskArena did not recover after an exception");
    }

    void test_particle_pool()
    {
        ParticlePool pool;
        for (int index = 0; index < 8; ++index)
        {
            pool.spawn({
                .position = { static_cast<float>(index), static_cast<float>(index * 2) },
                .velocity = {},
                .color = {},
                .radius = 1.0F,
                .lifetime = -1.0F,
                .inverse_mass = 1.0F,
                .species = static_cast<std::uint32_t>(index),
                .flags = particle_none
            });
        }
        const std::vector<std::uint64_t> original_ids(pool.ids().begin(), pool.ids().end());
        pool.mark_dead(1);
        pool.mark_dead(4);
        pool.mark_dead(7);
        pool.compact_stable();

        require(pool.size() == 5, "ParticlePool compaction count is wrong");
        constexpr std::array<std::size_t, 5> survivors{ 0, 2, 3, 5, 6 };
        for (std::size_t index = 0; index < survivors.size(); ++index)
        {
            require(pool.ids()[index] == original_ids[survivors[index]],
                "ParticlePool compaction was not stable");
        }

        require(pool.positions().size() == pool.velocities().size()
                && pool.positions().size() == pool.colors().size()
                && pool.positions().size() == pool.radii().size()
                && pool.positions().size() == pool.lifetimes().size()
                && pool.positions().size() == pool.inverse_masses().size()
                && pool.positions().size() == pool.species().size()
                && pool.positions().size() == pool.flags().size()
                && pool.positions().size() == pool.ids().size(),
            "ParticlePool structure-of-arrays sizes diverged");

        ParticlePool aging_pool;
        aging_pool.spawn({ .lifetime = 1.0F });
        aging_pool.age(-1.0F);
        aging_pool.age(std::numeric_limits<float>::quiet_NaN());
        require(aging_pool.lifetimes()[0] == 1.0F,
            "ParticlePool accepted an invalid aging delta");
        aging_pool.age(1.0F);
        require((aging_pool.flags()[0] & particle_dead) != 0U,
            "ParticlePool lifetime expiration failed");
    }

    void test_uniform_grid()
    {
        constexpr Bounds bounds{ 100.0F, 100.0F };
        const std::array<Vec2, 6> positions{
            Vec2{ 4.0F, 4.0F }, Vec2{ 8.0F, 7.0F }, Vec2{ 55.0F, 55.0F },
            Vec2{ 98.0F, 98.0F }, Vec2{ 1.0F, 99.0F }, Vec2{ 99.0F, 1.0F }
        };
        UniformGridIndex grid{ bounds, 10.0F };
        grid.build(positions);
        require(grid.indexed_count() == positions.size(), "UniformGridIndex lost entries");

        std::vector<std::uint32_t> nearby;
        grid.for_each_neighbor({ 5.0F, 5.0F }, 10.0F, false,
            [&nearby](std::uint32_t index) { nearby.push_back(index); });
        require(std::ranges::find(nearby, 0U) != nearby.end(), "Uniform grid omitted the query cell");
        require(std::ranges::find(nearby, 1U) != nearby.end(), "Uniform grid omitted a neighbor cell");

        std::vector<std::uint32_t> wrapped;
        grid.for_each_neighbor({ 1.0F, 1.0F }, 10.0F, true,
            [&wrapped](std::uint32_t index) { wrapped.push_back(index); });
        require(std::ranges::find(wrapped, 3U) != wrapped.end(), "Uniform grid wrap lookup failed");

        UniformGridIndex default_grid;
        const std::array<Vec2, 3> exceptional_positions{
            Vec2{},
            Vec2{ std::numeric_limits<float>::quiet_NaN(), 0.0F },
            Vec2{ std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity() }
        };
        default_grid.build(exceptional_positions);
        require(default_grid.indexed_count() == exceptional_positions.size(),
            "Default UniformGridIndex could not index sanitized positions");

