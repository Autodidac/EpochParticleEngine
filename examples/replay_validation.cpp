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
    [[nodiscard]] bool parse(std::string_view text, Integer& value) noexcept
    {
        const auto [end, error] = std::from_chars(
            text.data(),
            text.data() + text.size(),
            value);
        return error == std::errc{} && end == text.data() + text.size();
    }

    [[nodiscard]] std::uint64_t run_scene(
        std::size_t scene,
        std::uint64_t ticks,
        std::uint64_t seed,
        std::size_t workers)
    {
        using namespace epochengine::particle;
        Simulation simulation({
            .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
            .maximum_catch_up_steps = 8,
            .seed = seed,
            .worker_count = workers
        });
        simulation.resize({ 1280.0F, 720.0F });
        simulation.select_scene(scene);
        simulation.reset();
        for (std::uint64_t tick = 0; tick < ticks; ++tick)
            simulation.step_once();
        return simulation.state_hash();
    }
}

int main(int argc, char** argv)
{
    using namespace epochengine::particle;

    std::uint64_t ticks = 180;
    std::uint64_t seed = 42;
    std::size_t parallel_workers = TaskArena::recommended_worker_count();
    if (argc > 1 && !parse(argv[1], ticks))
    {
        std::cerr << "Usage: EpochParticleReplay [ticks] [seed] [parallel-workers]\n";
        return 2;
    }
    if (argc > 2 && !parse(argv[2], seed))
        return 2;
    if (argc > 3 && !parse(argv[3], parallel_workers))
        return 2;

    Simulation catalogue({ .seed = seed, .worker_count = 0 });
    bool matched = true;
    std::cout << "scene,serial_hash,parallel_hash,result\n";
    for (std::size_t scene = 0; scene < catalogue.scene_count(); ++scene)
    {
        const std::uint64_t serial_hash = run_scene(scene, ticks, seed, 0);
        const std::uint64_t parallel_hash = run_scene(
            scene,
            ticks,
            seed,
            parallel_workers);
        const bool scene_matched = serial_hash == parallel_hash;
        matched = matched && scene_matched;
        std::cout << catalogue.scene_info(scene).id << ','
                  << serial_hash << ',' << parallel_hash << ','
                  << (scene_matched ? "MATCH" : "MISMATCH") << '\n';
    }
    return matched ? 0 : 1;
}
