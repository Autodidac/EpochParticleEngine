#pragma once

#include <epochengine/particle/simulation.hpp>

#include <cstddef>
#include <optional>

namespace epochengine::particle::demo
{
    struct UiRect
    {
        Vec2 center{};
        Vec2 half_extent{};
    };

    struct UiLayout
    {
        Bounds framebuffer{};
        float panel_width{ 344.0F };
        bool panel_visible{ true };

        [[nodiscard]] float scene_width() const noexcept;
        [[nodiscard]] Bounds scene_bounds() const noexcept;
        [[nodiscard]] bool point_in_scene(Vec2 point) const noexcept;
    };

    class UiOverlay
    {
    public:
        explicit UiOverlay(UiLayout layout = {});

        void resize(Bounds framebuffer) noexcept;
        void toggle_panel() noexcept;
        void set_panel_visible(bool visible) noexcept;

        [[nodiscard]] const UiLayout& layout() const noexcept;
        [[nodiscard]] std::optional<std::size_t> scene_at(
            Vec2 point,
            const Simulation& simulation) const noexcept;

        void render(
            RenderFrame& frame,
            const Simulation& simulation,
            double frames_per_second,
            double frame_milliseconds,
            std::size_t gpu_capacity,
            std::string_view device_name) const;

    private:
        [[nodiscard]] UiRect scene_row_rect(std::size_t index, std::size_t count) const noexcept;
        void render_scene_list(RenderFrame& frame, const Simulation& simulation) const;
        void render_stats(
            RenderFrame& frame,
            const Simulation& simulation,
            double frames_per_second,
            double frame_milliseconds,
            std::size_t gpu_capacity,
            std::string_view device_name) const;

        UiLayout layout_{};
    };
}
