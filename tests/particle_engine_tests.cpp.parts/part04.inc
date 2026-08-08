            for (int tick = 0; tick < ticks; ++tick)
            {
                single.step_once();
                parallel.step_once();
                if ((tick % 30) == 29)
                {
                    require(single.state_hash() == parallel.state_hash(),
                        "Scene state differs between single-thread and parallel execution");
                }
            }

            frame.begin(bounds);
            single.render(frame);
            frame.finalize();
            require(!frame.items().empty(),
                "Scene produced an empty render frame");
            require_finite_frame(frame);

            const std::uint64_t completed_hash = single.state_hash();
            single.reset();
            for (int tick = 0; tick < ticks; ++tick)
                single.step_once();
            require(single.state_hash() == completed_hash,
                "Scene reset/replay is not deterministic");
        }
    }

    void test_particle_studio_document()
    {
        Simulation simulation({
            .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
            .maximum_catch_up_steps = 8,
            .seed = 0x53545544494f5445ULL,
            .worker_count = 0
        });
        simulation.resize({ 1280.0F, 720.0F });

        require(simulation.active_scene_info().id == "particle-studio",
            "Particle Studio is not the default scene");
        const std::string document = simulation.active_scene_document();
        require(!document.empty(),
            "Particle Studio produced an empty scene document");
        require(document.starts_with("EPOCH_PARTICLE_SCENE 1\n"),
            "Particle Studio scene document header is invalid");

        const std::uint64_t initial_hash = simulation.state_hash();
        for (int tick = 0; tick < 30; ++tick)
            simulation.step_once();
        require(simulation.state_hash() != initial_hash,
            "Particle Studio failed to advance before document restore");

        require(simulation.apply_active_scene_document(document),
            "Particle Studio rejected its own scene document");
        require(simulation.tick() == 0,
            "Scene document restore did not reset the simulation tick");
        require(simulation.state_hash() == initial_hash,
            "Particle Studio document roundtrip did not restore editor state");

        const std::uint64_t restored_hash = simulation.state_hash();
        require(!simulation.apply_active_scene_document("invalid document"),
            "Particle Studio accepted a malformed scene document");
        require(simulation.state_hash() == restored_hash,
            "Malformed scene document mutated Particle Studio state");
    }

    struct TestCase
    {
        std::string_view name;
        void (*function)();
    };
}

int main()
{
    constexpr std::array tests{
        TestCase{ "fixed point", &test_fixed_point },
        TestCase{ "text metrics", &test_text_metrics },
        TestCase{ "random repeatability", &test_random_repeatability },
        TestCase{ "task arena", &test_task_arena },
        TestCase{ "particle pool", &test_particle_pool },
        TestCase{ "uniform grid", &test_uniform_grid },
        TestCase{ "material grid", &test_material_grid },
        TestCase{ "render frame", &test_render_frame },
        TestCase{ "simulation scheduler", &test_simulation_scheduler },
        TestCase{ "all scene determinism", &test_all_scene_determinism },
        TestCase{ "particle studio document", &test_particle_studio_document }
    };

    std::size_t passed = 0;
    for (const TestCase& test : tests)
    {
        try
        {
            test.function();
            ++passed;
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& exception)
        {
            std::cerr
                << "[FAIL] " << test.name
                << ": " << exception.what() << '\n';
            return 1;
        }
    }

    std::cout << passed << " tests passed.\n";
    return 0;
}
