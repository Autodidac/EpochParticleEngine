#pragma once

#include "events.hpp"
#include "export.hpp"
#include "scene.hpp"
#include "scenes.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace epochengine::particle
{
    struct SimulationConfig
    {
        std::chrono::nanoseconds fixed_step{ 16'666'667 };
        std::uint32_t maximum_catch_up_steps{ 8 };
        std::uint64_t seed{ 0x45504f4348504152ULL };
        std::size_t worker_count{ TaskArena::recommended_worker_count() };
    };

    class EPOCH_PARTICLE_API Simulation
    {
    public:
        explicit Simulation(
            SimulationConfig config = {},
            std::vector<std::unique_ptr<IScene>> scenes = make_default_scenes());

        void advance(std::chrono::nanoseconds elapsed);
        void step_once();
        void render(RenderFrame& frame);
        void pointer(const PointerEvent& event);
        void resize(Bounds bounds);

        void select_scene(std::size_t index);
        void next_scene();
        void previous_scene();
        void reset();

        void set_paused(bool paused) noexcept;
        void toggle_paused() noexcept;
        void set_time_scale(double scale) noexcept;

        [[nodiscard]] bool paused() const noexcept;
        [[nodiscard]] double time_scale() const noexcept;
        [[nodiscard]] std::uint64_t tick() const noexcept;
        [[nodiscard]] std::uint64_t seed() const noexcept;
        [[nodiscard]] Bounds bounds() const noexcept;

        [[nodiscard]] std::size_t scene_count() const noexcept;
        [[nodiscard]] std::size_t active_scene_index() const noexcept;
        [[nodiscard]] SceneInfo scene_info(std::size_t index) const noexcept;
        [[nodiscard]] SceneInfo active_scene_info() const noexcept;
        [[nodiscard]] SceneStats active_scene_stats() const noexcept;

        [[nodiscard]] std::string active_scene_document() const;
        bool apply_active_scene_document(std::string_view document);

        [[nodiscard]] std::span<const SimulationEvent> events() const noexcept;
        void clear_events() noexcept;

        [[nodiscard]] std::uint64_t state_hash() const noexcept;
        [[nodiscard]] std::size_t worker_count() const noexcept;

    private:
        void reset_active_scene();

        SimulationConfig config_{};
        std::vector<std::unique_ptr<IScene>> scenes_;
        TaskArena tasks_;
        Bounds bounds_{ 1280.0F, 720.0F };
        std::chrono::nanoseconds accumulator_{};
        long double scaled_time_remainder_{};
        std::vector<SimulationEvent> events_;
        std::size_t active_scene_{};
        std::uint64_t tick_{};
        bool paused_{};
        double time_scale_{ 1.0 };
    };
}
