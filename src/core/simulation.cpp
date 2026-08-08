#include <epochengine/particle/simulation.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace epochengine::particle
{
    Simulation::Simulation(
        SimulationConfig config,
        std::vector<std::unique_ptr<IScene>> scenes)
        : config_(config)
        , scenes_(std::move(scenes))
        , tasks_(config_.worker_count)
    {
        if (config_.fixed_step.count() <= 0)
            throw std::invalid_argument("Simulation fixed_step must be positive");
        if (scenes_.empty())
            throw std::invalid_argument("Simulation requires at least one scene");

        config_.maximum_catch_up_steps = std::max(config_.maximum_catch_up_steps, 1U);
        events_.reserve(256);
        reset_active_scene();
    }

    void Simulation::advance(std::chrono::nanoseconds elapsed)
    {
        events_.clear();
        if (paused_ || elapsed <= std::chrono::nanoseconds::zero())
            return;

        const auto clamped_elapsed = std::min(
            elapsed,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::milliseconds(250)));
        const double scaled_count =
            static_cast<double>(clamped_elapsed.count()) * time_scale_;
        accumulator_ += std::chrono::nanoseconds{
            static_cast<std::chrono::nanoseconds::rep>(std::llround(scaled_count))
        };

        std::uint32_t steps = 0;
        while (accumulator_ >= config_.fixed_step
            && steps < config_.maximum_catch_up_steps)
        {
            const SceneUpdateContext context{
                .delta_seconds = std::chrono::duration<float>(config_.fixed_step).count(),
                .tick = tick_,
                .bounds = bounds_,
                .tasks = tasks_,
                .events = events_
            };
            scenes_[active_scene_]->update(context);
            ++tick_;
            ++steps;
            accumulator_ -= config_.fixed_step;
        }

        if (steps == config_.maximum_catch_up_steps
            && accumulator_ >= config_.fixed_step)
        {
            accumulator_ %= config_.fixed_step;
        }
    }

    void Simulation::step_once()
    {
        events_.clear();
        const SceneUpdateContext context{
            .delta_seconds = std::chrono::duration<float>(config_.fixed_step).count(),
            .tick = tick_,
            .bounds = bounds_,
            .tasks = tasks_,
            .events = events_
        };
        scenes_[active_scene_]->update(context);
        ++tick_;
    }

    void Simulation::render(RenderFrame& frame)
    {
        scenes_[active_scene_]->render(frame, bounds_);
    }

    void Simulation::pointer(const PointerEvent& event)
    {
        scenes_[active_scene_]->pointer(event);
    }

    void Simulation::resize(Bounds bounds)
    {
        if (!bounds.valid())
            return;
        const Bounds old_bounds = bounds_;
        bounds_ = bounds;
        scenes_[active_scene_]->resize(old_bounds, bounds_);
    }

    void Simulation::select_scene(std::size_t index)
    {
        if (index >= scenes_.size() || index == active_scene_)
            return;
        active_scene_ = index;
        reset_active_scene();
    }

    void Simulation::next_scene()
    {
        select_scene((active_scene_ + 1U) % scenes_.size());
    }

    void Simulation::previous_scene()
    {
        select_scene((active_scene_ + scenes_.size() - 1U) % scenes_.size());
    }

    void Simulation::reset()
    {
        reset_active_scene();
    }

    void Simulation::set_paused(bool paused) noexcept { paused_ = paused; }
    void Simulation::toggle_paused() noexcept { paused_ = !paused_; }

    void Simulation::set_time_scale(double scale) noexcept
    {
        time_scale_ = std::isfinite(scale) ? std::clamp(scale, 0.05, 16.0) : 1.0;
    }

    bool Simulation::paused() const noexcept { return paused_; }
    double Simulation::time_scale() const noexcept { return time_scale_; }
    std::uint64_t Simulation::tick() const noexcept { return tick_; }
    std::uint64_t Simulation::seed() const noexcept { return config_.seed; }
    Bounds Simulation::bounds() const noexcept { return bounds_; }
    std::size_t Simulation::scene_count() const noexcept { return scenes_.size(); }
    std::size_t Simulation::active_scene_index() const noexcept { return active_scene_; }

    SceneInfo Simulation::scene_info(std::size_t index) const noexcept
    {
        return index < scenes_.size() ? scenes_[index]->info() : SceneInfo{};
    }

    SceneInfo Simulation::active_scene_info() const noexcept
    {
        return scenes_[active_scene_]->info();
    }

    SceneStats Simulation::active_scene_stats() const noexcept
    {
        return scenes_[active_scene_]->stats();
    }

    std::string Simulation::active_scene_document() const
    {
        return scenes_[active_scene_]->scene_document();
    }

    bool Simulation::apply_active_scene_document(std::string_view document)
    {
        if (document.empty()
            || !scenes_[active_scene_]->apply_scene_document(document))
        {
            return false;
        }
        tick_ = 0;
        accumulator_ = {};
        events_.clear();
        return true;
    }

    std::span<const SimulationEvent> Simulation::events() const noexcept
    {
        return events_;
    }

    void Simulation::clear_events() noexcept
    {
        events_.clear();
    }

    std::uint64_t Simulation::state_hash() const noexcept
    {
        StableHasher hasher;
        hasher.append_u64(config_.seed);
        hasher.append_u64(tick_);
        hasher.append_u64(active_scene_);
        hasher.append_u64(scenes_[active_scene_]->state_hash());
        return hasher.value();
    }

    std::size_t Simulation::worker_count() const noexcept
    {
        return tasks_.worker_count();
    }

    void Simulation::reset_active_scene()
    {
        tick_ = 0;
        accumulator_ = {};
        events_.clear();

        const std::uint64_t scene_seed =
            hash_combine(config_.seed, static_cast<std::uint64_t>(active_scene_) + 1U);
        scenes_[active_scene_]->reset({
            .bounds = bounds_,
            .seed = scene_seed
        });
    }
}
