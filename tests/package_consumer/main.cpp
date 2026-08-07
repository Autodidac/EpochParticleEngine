#include <epochengine/particle/epoch_particle_engine.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>

int main()
{
    using namespace epochengine::particle;

    Fixed32 fixed = Fixed32::from_float(1.25F);
    Pcg32 random{ 42 };
    StableHasher api_hash;
    api_hash.append_u32(random.next_u32());
    api_hash.append_i32(fixed.raw());
    api_hash.append_u64(hash_combine(api_hash.value(), 0x45504f4348ULL));

    Simulation simulation({
        .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
        .maximum_catch_up_steps = 8,
        .seed = 42,
        .worker_count = 2
    });
    for (std::uint32_t tick = 0; tick < 5; ++tick)
        simulation.step_once();

    std::cout << version_string << ' '
              << simulation.active_scene_info().id << ' '
              << simulation.state_hash() << ' '
              << api_hash.value() << '\n';
    return simulation.state_hash() == 0 || api_hash.value() == 0 ? 1 : 0;
}
