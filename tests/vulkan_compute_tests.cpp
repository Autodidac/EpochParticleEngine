#include <epochengine/particle/epoch_particle_engine.hpp>
#include <epochengine/particle/vulkan/compute_backend.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
    using namespace epochengine::particle;

    void require(bool condition, std::string_view message)
    {
        if (!condition)
            throw std::runtime_error(std::string{ message });
    }
}

int main()
{
    try
    {
        auto backend_result =
            epochengine::particle::vulkan::ComputeBackend::create();
        if (!backend_result)
        {
            std::cout << "SKIP: Vulkan compute unavailable: "
                      << backend_result.error() << '\n';
            return 0;
        }
        auto backend = std::move(*backend_result);
        std::cout << "Compute device: " << backend->name() << '\n';

        std::vector<std::uint32_t> probe{ 3U, 7U, 11U, 19U };
        static constexpr std::string_view probe_shader = R"glsl(
#version 450
layout(local_size_x = 64) in;
layout(std430, set = 0, binding = 0) buffer Probe
{
    uint values[];
} data;
void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index < data.values[0])
        data.values[index + 1u] += 5u;
}
)glsl";
        std::span<std::uint32_t> probe_words{ probe };
        auto probe_dispatch = backend->dispatch({
            .program_id = "epoch.compute.probe.v1",
            .shader_source = probe_shader,
            .storage = std::as_writable_bytes(probe_words),
            .push_constants = {},
            .workgroup_count_x = 1
        });
        require(static_cast<bool>(probe_dispatch), "Vulkan compute probe dispatch failed");
        require(
            probe[1] == 12U && probe[2] == 16U && probe[3] == 24U,
            "Vulkan compute probe readback mismatch");

        constexpr std::chrono::nanoseconds fixed_step{ 16'666'667 };
        constexpr std::size_t compute_scenes[]{ 3U, 4U, 9U, 10U, 11U };
        for (const std::size_t scene : compute_scenes)
        {
            const SimulationConfig config{
                .fixed_step = fixed_step,
                .maximum_catch_up_steps = 8,
                .seed = 0x475055434f4d5055ULL,
                .worker_count = 1
            };
            Simulation whole{ config };
            Simulation split{ config };
            whole.set_compute_backend(backend.get());
            split.set_compute_backend(backend.get());
            whole.select_scene(scene);
            split.select_scene(scene);

            whole.advance(fixed_step * 3);
            for (int step = 0; step < 3; ++step)
                split.advance(fixed_step);

            require(
                whole.tick() == split.tick(),
                "GPU scene tick differs by elapsed-time chunking");
            require(
                whole.state_hash() == split.state_hash(),
                "GPU scene state differs by elapsed-time chunking");
            const SceneStats stats = whole.active_scene_stats();
            bool reported_compute = false;
            for (std::size_t metric = 0; metric < stats.metric_count; ++metric)
            {
                if (stats.metrics[metric].label == "GPU COMPUTE"
                    && stats.metrics[metric].value == 1.0)
                {
                    reported_compute = true;
                }
            }
            require(reported_compute, "Scene silently used the CPU fallback");
            std::cout << "PASS: " << whole.active_scene_info().name
                      << " GPU frame-chunk equivalence\n";
        }
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
