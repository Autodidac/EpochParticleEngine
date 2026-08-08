#pragma once

#include "compute.hpp"
#include "events.hpp"
#include "export.hpp"
#include "hash.hpp"
#include "render_frame.hpp"
#include "task_arena.hpp"
#include "types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace epochengine::particle
{
    enum class PointerButton : std::uint8_t
    {
        primary,
        secondary,
        middle
    };

    enum class PointerAction : std::uint8_t
    {
        move,
        press,
        release
    };

    struct PointerEvent
    {
        PointerAction action{ PointerAction::move };
        PointerButton button{ PointerButton::primary };
        Vec2 position{};
        bool primary_down{};
        bool secondary_down{};
        bool shift{};
        bool control{};
    };

    struct SceneInfo
    {
        std::string_view id;
        std::string_view name;
        std::string_view description;
    };

    struct SceneMetric
    {
        std::string_view label;
        double value{};
    };

    struct SceneStats
    {
        std::uint64_t particle_count{};
        std::uint64_t active_cell_count{};
        std::array<SceneMetric, 6> metrics{};
        std::size_t metric_count{};
    };

    struct SceneResetContext
    {
        Bounds bounds{};
        std::uint64_t seed{};
    };

    struct SceneUpdateContext
    {
        float delta_seconds{};
        std::uint64_t tick{};
        Bounds bounds{};
        TaskArena& tasks;
        std::vector<SimulationEvent>& events;
        IComputeBackend* compute{};
    };

    class EPOCH_PARTICLE_API IScene
    {
    public:
        virtual ~IScene() = default;

        [[nodiscard]] virtual SceneInfo info() const noexcept = 0;
        virtual void reset(const SceneResetContext& context) = 0;
        virtual void update(const SceneUpdateContext& context) = 0;
        virtual void render(RenderFrame& frame, Bounds bounds) const = 0;
        virtual void pointer(const PointerEvent& event) = 0;
        virtual void resize(Bounds old_bounds, Bounds new_bounds) = 0;

        [[nodiscard]] virtual SceneStats stats() const noexcept = 0;
        [[nodiscard]] virtual std::uint64_t state_hash() const noexcept = 0;

        // Optional editor/document contract. Simulation scenes that do not
        // expose editable persistent state keep the default no-op behavior.
        [[nodiscard]] virtual std::string scene_document() const
        {
            return {};
        }

        virtual bool apply_scene_document(std::string_view)
        {
            return false;
        }
    };
}
