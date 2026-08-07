#include <epochengine/particle/epoch_particle_engine.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace
{
    template<class Integer>
    [[nodiscard]] bool parse(std::string_view text, Integer& value)
    {
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc{} && end == text.data() + text.size();
    }
}

int main(int argc, char** argv)
{
    using namespace epochengine::particle;

    std::uint64_t ticks = 600;
    std::uint64_t seed = 0x45504f4348504152ULL;
    std::size_t workers = TaskArena::recommended_worker_count();
    if (argc > 1 && !parse(argv[1], ticks))
    {
        std::cerr << "Usage: EpochParticleHeadless [ticks] [seed] [workers]\n";
        return 1;
    }
    if (argc > 2 && !parse(argv[2], seed))
        return 1;
    if (argc > 3 && !parse(argv[3], workers))
        return 1;

    Simulation simulation({
        .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
        .maximum_catch_up_steps = 8,
        .seed = seed,
        .worker_count = workers
    });
    simulation.resize({ 1280.0F, 720.0F });

    std::cout << "scene,ticks,particles,cells,state_hash,render_items\n";
    for (std::size_t scene = 0; scene < simulation.scene_count(); ++scene)
    {
        simulation.select_scene(scene);
        simulation.reset();
        for (std::uint64_t tick = 0; tick < ticks; ++tick)
            simulation.step_once();

        RenderFrame frame{ 600'000 };
        frame.begin(simulation.bounds());
        simulation.render(frame);
        frame.finalize();

        const SceneInfo info = simulation.active_scene_info();
        const SceneStats stats = simulation.active_scene_stats();
        std::cout << info.id << ',' << ticks << ',' << stats.particle_count << ','
                  << stats.active_cell_count << ',' << simulation.state_hash() << ','
                  << frame.items().size() << '\n';
    }
    return 0;
}
