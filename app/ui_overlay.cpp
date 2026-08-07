#include "ui_overlay.hpp"

#if EPOCH_PARTICLE_WITH_EPOCHGUI
#include <gui/layout_primitives.hpp>
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace epochengine::particle::demo
{
    namespace
    {
        constexpr Color panel_background{ 0.028F, 0.039F, 0.059F, 0.97F };
        constexpr Color panel_border{ 0.13F, 0.20F, 0.30F, 1.0F };
        constexpr Color row_background{ 0.050F, 0.068F, 0.098F, 1.0F };
        constexpr Color row_selected{ 0.105F, 0.245F, 0.390F, 1.0F };
        constexpr Color accent{ 0.22F, 0.70F, 1.0F, 1.0F };
        constexpr Color text_primary{ 0.91F, 0.95F, 1.0F, 1.0F };
        constexpr Color text_secondary{ 0.60F, 0.69F, 0.80F, 1.0F };
        constexpr Color text_muted{ 0.43F, 0.51F, 0.62F, 1.0F };
        constexpr float panel_padding = 16.0F;
        constexpr float row_height = 35.0F;
        constexpr float row_gap = 5.0F;
        constexpr float scene_list_top = 76.0F;

        [[nodiscard]] std::string number_string(std::uint64_t value)
        {
            std::array<char, 32> buffer{};
            const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            return error == std::errc{} ? std::string(buffer.data(), end) : std::string{ "0" };
        }

        [[nodiscard]] std::string decimal_string(double value, int precision)
        {
            std::array<char, 64> buffer{};
            const int count = std::snprintf(
                buffer.data(),
                buffer.size(),
                "%.*f",
                precision,
                value);
            if (count <= 0)
                return "0";
            return std::string(buffer.data(), static_cast<std::size_t>(count));
        }

        [[nodiscard]] std::string compact_device_name(std::string_view value)
        {
            constexpr std::size_t maximum = 38;
            if (value.size() <= maximum)
                return std::string{ value };
            std::string result{ value.substr(0, maximum - 3) };
            result.append("...");
            return result;
        }

        void label_value(
            RenderFrame& frame,
            float x,
            float y,
            std::string_view label,
            std::string_view value)
        {
            frame.text({ x, y }, label, 1.45F, text_muted, 240);
            frame.text({ x + 133.0F, y }, value, 1.45F, text_primary, 240);
        }
    }

    float UiLayout::scene_width() const noexcept
    {
        if (!panel_visible)
            return framebuffer.width;
        return std::max(1.0F, framebuffer.width - panel_width);
    }

    Bounds UiLayout::scene_bounds() const noexcept
    {
        return { scene_width(), framebuffer.height };
    }

    bool UiLayout::point_in_scene(Vec2 point) const noexcept
    {
        return point.x >= 0.0F && point.y >= 0.0F &&
            point.x < scene_width() && point.y < framebuffer.height;
    }

    UiOverlay::UiOverlay(UiLayout layout)
        : layout_(layout)
    {
    }

    void UiOverlay::resize(Bounds framebuffer) noexcept
    {
        layout_.framebuffer = framebuffer;
        const float maximum_panel = std::max(240.0F, framebuffer.width * 0.42F);
        layout_.panel_width = std::clamp(layout_.panel_width, 240.0F, maximum_panel);
    }

    void UiOverlay::toggle_panel() noexcept
    {
        layout_.panel_visible = !layout_.panel_visible;
    }

    void UiOverlay::set_panel_visible(bool visible) noexcept
    {
        layout_.panel_visible = visible;
    }

    const UiLayout& UiOverlay::layout() const noexcept
    {
        return layout_;
    }

    UiRect UiOverlay::scene_row_rect(std::size_t index, std::size_t count) const noexcept
    {
        const float panel_left = layout_.scene_width();
#if EPOCH_PARTICLE_WITH_EPOCHGUI
        const epochengine::gui_lib::SelectableListLayoutOptions options{
            .viewport = {
                .position = { panel_left + panel_padding, scene_list_top },
                .size = { layout_.panel_width - panel_padding * 2.0F,
                          static_cast<float>(count) * (row_height + row_gap) }
            },
            .row_count = static_cast<std::uint32_t>(count),
            .row_height = row_height,
            .row_gap = row_gap,
            .scroll_offset = 0.0F,
            .content_padding_x = 0.0F,
            .content_padding_y = 0.0F
        };
        const auto row = epochengine::gui_lib::make_selectable_row_layout(
            options,
            static_cast<std::uint32_t>(index),
            {});
        return {
            .center = {
                row.row.position.x + row.row.size.x * 0.5F,
                row.row.position.y + row.row.size.y * 0.5F
            },
            .half_extent = { row.row.size.x * 0.5F, row.row.size.y * 0.5F }
        };
#else
        (void)count;
        return {
            .center = {
                panel_left + layout_.panel_width * 0.5F,
                scene_list_top + static_cast<float>(index) * (row_height + row_gap) + row_height * 0.5F
            },
            .half_extent = {
                layout_.panel_width * 0.5F - panel_padding,
                row_height * 0.5F
            }
        };
#endif
    }

    std::optional<std::size_t> UiOverlay::scene_at(
        Vec2 point,
        const Simulation& simulation) const noexcept
    {
        if (!layout_.panel_visible || point.x < layout_.scene_width())
            return std::nullopt;

        for (std::size_t index = 0; index < simulation.scene_count(); ++index)
        {
            const UiRect row = scene_row_rect(index, simulation.scene_count());
            const Vec2 minimum = row.center - row.half_extent;
            const Vec2 maximum = row.center + row.half_extent;
            if (point.x >= minimum.x && point.x <= maximum.x &&
                point.y >= minimum.y && point.y <= maximum.y)
            {
                return index;
            }
        }
        return std::nullopt;
    }

    void UiOverlay::render(
        RenderFrame& frame,
        const Simulation& simulation,
        double frames_per_second,
        double frame_milliseconds,
        std::size_t gpu_capacity,
        std::string_view device_name) const
    {
        const float boundary = layout_.scene_width();
        frame.rectangle(
            { boundary, layout_.framebuffer.height * 0.5F },
            { 1.0F, layout_.framebuffer.height * 0.5F },
            panel_border,
            190);

        if (!layout_.panel_visible)
        {
            frame.rounded_rectangle(
                { layout_.framebuffer.width - 62.0F, 26.0F },
                { 50.0F, 16.0F },
                7.0F,
                panel_background,
                210);
            frame.text(
                { layout_.framebuffer.width - 62.0F, 21.0F },
                "F1 PANEL",
                1.35F,
                text_secondary,
                220,
                TextAlign::center);
            return;
        }

        const float panel_center = boundary + layout_.panel_width * 0.5F;
        frame.rectangle(
            { panel_center, layout_.framebuffer.height * 0.5F },
            { layout_.panel_width * 0.5F, layout_.framebuffer.height * 0.5F },
            panel_background,
            195);
        frame.rectangle(
            { boundary + 1.0F, layout_.framebuffer.height * 0.5F },
            { 1.0F, layout_.framebuffer.height * 0.5F },
            panel_border,
            205);

        frame.text(
            { boundary + panel_padding, 18.0F },
            "EPOCH PARTICLE ENGINE",
            2.1F,
            text_primary,
            220);
        frame.text(
            { boundary + panel_padding, 48.0F },
            "VULKAN HYBRID LAB",
            1.35F,
            accent,
            220);

        render_scene_list(frame, simulation);
        render_stats(
            frame,
            simulation,
            frames_per_second,
            frame_milliseconds,
            gpu_capacity,
            device_name);
    }

    void UiOverlay::render_scene_list(RenderFrame& frame, const Simulation& simulation) const
    {
        const float panel_left = layout_.scene_width();
        for (std::size_t index = 0; index < simulation.scene_count(); ++index)
        {
            const UiRect row = scene_row_rect(index, simulation.scene_count());
            const bool selected = index == simulation.active_scene_index();
            frame.rounded_rectangle(
                row.center,
                row.half_extent,
                7.0F,
                selected ? row_selected : row_background,
                210);

            if (selected)
            {
                frame.rounded_rectangle(
                    { row.center.x - row.half_extent.x + 2.5F, row.center.y },
                    { 2.5F, row.half_extent.y - 4.0F },
                    2.5F,
                    accent,
                    212);
            }

            const SceneInfo info = simulation.scene_info(index);
            const char shortcut = index < 9 ? static_cast<char>('1' + index) : '?';
            std::array<char, 2> shortcut_text{ shortcut, '\0' };
            frame.text(
                { panel_left + panel_padding + 13.0F, row.center.y - 5.0F },
                shortcut_text.data(),
                1.35F,
                selected ? text_primary : text_muted,
                220,
                TextAlign::center);
            frame.text(
                { panel_left + panel_padding + 31.0F, row.center.y - 5.0F },
                info.name,
                1.45F,
                selected ? text_primary : text_secondary,
                220);
        }
    }

    void UiOverlay::render_stats(
        RenderFrame& frame,
        const Simulation& simulation,
        double frames_per_second,
        double frame_milliseconds,
        std::size_t gpu_capacity,
        std::string_view device_name) const
    {
        const float panel_left = layout_.scene_width();
        const float x = panel_left + panel_padding;
        const float list_bottom = scene_list_top +
            static_cast<float>(simulation.scene_count()) * (row_height + row_gap);
        float y = list_bottom + 15.0F;

        const SceneInfo info = simulation.active_scene_info();
        frame.text({ x, y }, info.name, 1.75F, accent, 230);
        y += 23.0F;

        const SceneStats stats = simulation.active_scene_stats();
        label_value(frame, x, y, "PARTICLES", number_string(stats.particle_count));
        y += 18.0F;
        label_value(frame, x, y, "ACTIVE CELLS", number_string(stats.active_cell_count));
        y += 18.0F;
        label_value(frame, x, y, "TICK", number_string(simulation.tick()));
        y += 18.0F;
        label_value(frame, x, y, "STATE HASH", number_string(simulation.state_hash()));
        y += 23.0F;

        for (std::size_t index = 0; index < stats.metric_count; ++index)
        {
            label_value(
                frame,
                x,
                y,
                stats.metrics[index].label,
                decimal_string(stats.metrics[index].value, 2));
            y += 18.0F;
        }

        y = std::max(y + 10.0F, layout_.framebuffer.height - 174.0F);
        frame.line(
            { x, y },
            { panel_left + layout_.panel_width - panel_padding, y },
            1.0F,
            panel_border,
            220);
        y += 13.0F;

        label_value(frame, x, y, "FPS", decimal_string(frames_per_second, 1));
        y += 18.0F;
        label_value(frame, x, y, "FRAME MS", decimal_string(frame_milliseconds, 2));
        y += 18.0F;
        label_value(frame, x, y, "TIME SCALE", decimal_string(simulation.time_scale(), 2));
        y += 18.0F;
        label_value(frame, x, y, "WORKERS", number_string(simulation.worker_count()));
        y += 18.0F;
        label_value(frame, x, y, "GPU ITEMS", number_string(gpu_capacity));
        y += 22.0F;

        frame.text({ x, y }, compact_device_name(device_name), 1.15F, text_muted, 240);
        y += 20.0F;
        frame.text(
            { x, y },
            simulation.paused() ? "PAUSED  SPACE PLAY" : "SPACE PAUSE  R RESET  . STEP",
            1.15F,
            simulation.paused() ? accent : text_secondary,
            240);
        y += 17.0F;
        frame.text({ x, y }, "TAB SCENE  - + SPEED  F1 PANEL", 1.15F, text_muted, 240);
    }
}
