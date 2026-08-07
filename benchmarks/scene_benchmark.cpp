#include <epochengine/particle/epoch_particle_engine.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>

int main()
{
    using namespace epochengine::particle;
    using clock = std::chrono::steady_clock;

    constexpr std::uint64_t warmup_ticks = 30;
    constexpr std::uint64_t measured_ticks = 300;
    Simulation simulation({
        .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
        .maximum_catch_up_steps = 8,
        .seed = 0x42454e43484d4152ULL,
        .worker_count = TaskArena::recommended_worker_count()
    });
    simulation.resize({ 1920.0F, 1080.0F });

    std::cout << "EpochParticleEngine CPU scene benchmark\n"
              << "Workers: " << simulation.worker_count() << "\n\n";
    for (std::size_t scene = 0; scene < simulation.scene_count(); ++scene)
    {
        simulation.select_scene(scene);
        simulation.reset();
        for (std::uint64_t tick = 0; tick < warmup_ticks; ++tick)
            simulation.step_once();

        const auto begin = clock::now();
        for (std::uint64_t tick = 0; tick < measured_ticks; ++tick)
            simulation.step_once();
        const auto end = clock::now();

        const double milliseconds = std::chrono::duration<double, std::milli>(end - begin).count();
        const double per_tick = milliseconds / static_cast<double>(measured_ticks);
        std::cout << std::left << std::setw(24) << simulation.active_scene_info().name
                  << std::right << std::fixed << std::setprecision(3)
                  << std::setw(10) << per_tick << " ms/tick  hash "
                  << simulation.state_hash() << '\n';
    }
    return 0;
}
