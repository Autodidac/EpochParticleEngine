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
            require(!frame.items().empty(), "Scene produced an empty render frame");
            require_finite_frame(frame);

            const std::uint64_t completed_hash = single.state_hash();
            single.reset();
            for (int tick = 0; tick < ticks; ++tick)
                single.step_once();
            require(single.state_hash() == completed_hash, "Scene reset/replay is not deterministic");
        }
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
        TestCase{ "random repeatability", &test_random_repeatability },
        TestCase{ "task arena", &test_task_arena },
        TestCase{ "particle pool", &test_particle_pool },
        TestCase{ "uniform grid", &test_uniform_grid },
        TestCase{ "material grid", &test_material_grid },
        TestCase{ "render frame", &test_render_frame },
        TestCase{ "simulation scheduler", &test_simulation_scheduler },
        TestCase{ "all scene determinism", &test_all_scene_determinism }
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
            std::cerr << "[FAIL] " << test.name << ": " << exception.what() << '\n';
            return 1;
        }
    }

    std::cout << passed << " tests passed.\n";
    return 0;
}
