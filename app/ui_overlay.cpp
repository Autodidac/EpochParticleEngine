#include "ui_overlay.hpp"

#if EPOCH_PARTICLE_WITH_EPOCHGUI
#include <gui/font.hpp>
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

        constexpr float panel_width_logical = 344.0F;
        constexpr float panel_padding_logical = 16.0F;
        constexpr float row_height_logical = 31.0F;
        constexpr float row_gap_logical = 4.0F;
        constexpr float scene_list_top_logical = 76.0F;
        constexpr float body_font_height = 12.0F;
        constexpr float scene_font_height = 13.0F;

#if EPOCH_PARTICLE_WITH_EPOCHGUI
        static_assert(epochengine::gui_lib::font::glyph_width == bitmap_glyph_width);
        static_assert(epochengine::gui_lib::font::glyph_height == bitmap_glyph_height);
        static_assert(epochengine::gui_lib::font::glyph_advance == bitmap_glyph_advance);
        static_assert(epochengine::gui_lib::font::line_advance == bitmap_line_advance);
#endif

        [[nodiscard]] float panel_padding(const UiLayout& layout) noexcept
        {
            return layout.scaled(panel_padding_logical);
        }

        [[nodiscard]] float scene_list_top(const UiLayout& layout) noexcept
        {
            return layout.scaled(scene_list_top_logical);
        }

        [[nodiscard]] float scene_row_height(const UiLayout& layout) noexcept
        {
            return std::max(
                layout.scaled(row_height_logical),
                resolved_text_pixel_height(layout.text_size(scene_font_height))
                    + layout.scaled(10.0F));
        }

        [[nodiscard]] float scene_row_gap(const UiLayout& layout) noexcept
        {
            return layout.scaled(row_gap_logical);
        }

        [[nodiscard]] float line_step(
            const UiLayout& layout,
            float logical_font_height = body_font_height,
            float logical_gap = 4.0F) noexcept
        {
            return resolved_text_pixel_height(layout.text_size(logical_font_height))
                + layout.scaled(logical_gap);
        }

        [[nodiscard]] std::string number_string(std::uint64_t value)
        {
            std::array<char, 32> buffer{};
            const auto [end, error] = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), value);
            return error == std::errc{}
                ? std::string(buffer.data(), end)
                : std::string{ "0" };
        }

        [[nodiscard]] std::string decimal_string(double value, int precision)
        {
            std::array<char, 64> buffer{};
            const int count = std::snprintf(
                buffer.data(), buffer.size(), "%.*f", precision, value);
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
            const UiLayout& layout,
            float x,
            float y,
            std::string_view label,
            std::string_view value)
        {
            const TextSize size = layout.text_size(body_font_height);
            frame.text({ x, y }, label, size, text_muted, 240);
            frame.text(
                { x + layout.scaled(133.0F), y },
                value,
                size,
                text_primary,
                240);
        }
    }

    float UiLayout::scaled(float logical_pixels) const noexcept
    {
        return logical_pixels * geometry_scale;
    }

    TextSize UiLayout::text_size(float logical_height) const noexcept
    {
        return {
            .logical_height = std::max(
                logical_height,
                minimum_readable_text_logical_height),
            .dpi_scale = dpi_scale
        };
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
        return point.x >= 0.0F && point.y >= 0.0F
            && point.x < scene_width() && point.y < framebuffer.height;
    }

    UiOverlay::UiOverlay(UiLayout layout)
        : layout_(layout)
    {
        set_dpi_scale(layout_.dpi_scale);
    }

    void UiOverlay::resize(Bounds framebuffer) noexcept
    {
        if (!framebuffer.valid())
            return;

        layout_.framebuffer = framebuffer;
        const float height_limited_scale = std::max(
            1.0F,
            framebuffer.height / 720.0F);
        layout_.geometry_scale = std::clamp(
            std::min(layout_.dpi_scale, height_limited_scale),
            1.0F,
            2.0F);

        const float maximum_panel = std::max(
            1.0F,
            framebuffer.width * 0.46F);
        const float minimum_panel = std::min(
            layout_.scaled(240.0F),
            maximum_panel);
        layout_.panel_width = std::clamp(
            layout_.scaled(panel_width_logical),
            minimum_panel,
            maximum_panel);
    }

    void UiOverlay::set_dpi_scale(float dpi_scale) noexcept
    {
        if (!std::isfinite(dpi_scale) || dpi_scale <= 0.0F)
            return;
        layout_.dpi_scale = std::clamp(dpi_scale, 0.5F, 4.0F);
        if (layout_.framebuffer.valid())
            resize(layout_.framebuffer);
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

    UiRect UiOverlay::scene_row_rect(
        std::size_t index,
        std::size_t count) const noexcept
    {
        const float panel_left = layout_.scene_width();
        const float padding = panel_padding(layout_);
        const float top = scene_list_top(layout_);
        const float height = scene_row_height(layout_);
        const float gap = scene_row_gap(layout_);
#if EPOCH_PARTICLE_WITH_EPOCHGUI
        const epochengine::gui_lib::SelectableListLayoutOptions options{
            .viewport = {
                .position = { panel_left + padding, top },
                .size = {
                    layout_.panel_width - padding * 2.0F,
                    static_cast<float>(count) * (height + gap)
                }
            },
            .row_count = static_cast<std::uint32_t>(count),
            .row_height = height,
            .row_gap = gap,
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
            .half_extent = {
                row.row.size.x * 0.5F,
                row.row.size.y * 0.5F
            }
        };
#else
        (void)count;
        return {
            .center = {
                panel_left + layout_.panel_width * 0.5F,
                top + static_cast<float>(index) * (height + gap)
                    + height * 0.5F
            },
            .half_extent = {
                layout_.panel_width * 0.5F - padding,
                height * 0.5F
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

        for (std::size_t index = 0;
             index < simulation.scene_count();
             ++index)
        {
            const UiRect row = scene_row_rect(
                index,
                simulation.scene_count());
            const Vec2 minimum = row.center - row.half_extent;
            const Vec2 maximum = row.center + row.half_extent;
            if (point.x >= minimum.x && point.x <= maximum.x
                && point.y >= minimum.y && point.y <= maximum.y)
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
                {
                    layout_.framebuffer.width - layout_.scaled(62.0F),
                    layout_.scaled(26.0F)
                },
                { layout_.scaled(50.0F), layout_.scaled(18.0F) },
                layout_.scaled(7.0F),
                panel_background,
                210);
            const TextSize size = layout_.text_size(12.0F);
            frame.text(
                {
                    layout_.framebuffer.width - layout_.scaled(62.0F),
                    layout_.scaled(26.0F)
                        - resolved_text_pixel_height(size) * 0.5F
                },
                "F1 PANEL",
                size,
                text_secondary,
                220,
                TextAlign::center);
            return;
        }

        const float panel_center = boundary + layout_.panel_width * 0.5F;
        frame.rectangle(
            { panel_center, layout_.framebuffer.height * 0.5F },
            {
                layout_.panel_width * 0.5F,
                layout_.framebuffer.height * 0.5F
            },
            panel_background,
            195);
        frame.rectangle(
            { boundary + 1.0F, layout_.framebuffer.height * 0.5F },
            { 1.0F, layout_.framebuffer.height * 0.5F },
            panel_border,
            205);

        const float padding = panel_padding(layout_);
        frame.text(
            { boundary + padding, layout_.scaled(18.0F) },
            "EPOCH PARTICLE ENGINE",
            layout_.text_size(18.0F),
            text_primary,
            220);
        frame.text(
            { boundary + padding, layout_.scaled(48.0F) },
            "VULKAN HYBRID LAB",
            layout_.text_size(12.0F),
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

    void UiOverlay::render_scene_list(
        RenderFrame& frame,
        const Simulation& simulation) const
    {
        const float panel_left = layout_.scene_width();
        const float padding = panel_padding(layout_);
        const TextSize shortcut_size = layout_.text_size(12.0F);
        const TextSize name_size = layout_.text_size(scene_font_height);

        for (std::size_t index = 0;
             index < simulation.scene_count();
             ++index)
        {
            const UiRect row = scene_row_rect(
                index,
                simulation.scene_count());
            const bool selected = index == simulation.active_scene_index();
            frame.rounded_rectangle(
                row.center,
                row.half_extent,
                layout_.scaled(7.0F),
                selected ? row_selected : row_background,
                210);

            if (selected)
            {
                frame.rounded_rectangle(
                    {
                        row.center.x - row.half_extent.x
                            + layout_.scaled(2.5F),
                        row.center.y
                    },
                    {
                        layout_.scaled(2.5F),
                        row.half_extent.y - layout_.scaled(4.0F)
                    },
                    layout_.scaled(2.5F),
                    accent,
                    212);
            }

            const SceneInfo info = simulation.scene_info(index);
            const char shortcut = index < 9
                ? static_cast<char>('1' + index)
                : '?';
            std::array<char, 2> shortcut_text{ shortcut, '\0' };
            frame.text(
                {
                    panel_left + padding + layout_.scaled(13.0F),
                    row.center.y
                        - resolved_text_pixel_height(shortcut_size) * 0.5F
                },
                shortcut_text.data(),
                shortcut_size,
                selected ? text_primary : text_muted,
                220,
                TextAlign::center);
            frame.text(
                {
                    panel_left + padding + layout_.scaled(31.0F),
                    row.center.y
                        - resolved_text_pixel_height(name_size) * 0.5F
                },
                info.name,
                name_size,
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
        const float x = panel_left + panel_padding(layout_);
        const float list_bottom = scene_list_top(layout_)
            + static_cast<float>(simulation.scene_count())
                * (scene_row_height(layout_) + scene_row_gap(layout_));
        float y = list_bottom + layout_.scaled(12.0F);

        const float body_step = line_step(layout_);
        const float footer_step = line_step(layout_, 12.0F, 4.0F);
        const float footer_height = footer_step * 7.0F
            + layout_.scaled(8.0F);
        const float footer_top = std::max(
            list_bottom,
            layout_.framebuffer.height - footer_height);

        const SceneInfo info = simulation.active_scene_info();
        frame.text(
            { x, y },
            info.name,
            layout_.text_size(15.0F),
            accent,
            230);
        y += line_step(layout_, 15.0F, 6.0F);

        const SceneStats stats = simulation.active_scene_stats();
        auto draw_value = [&](std::string_view label, std::string value)
        {
            if (y + body_step > footer_top - layout_.scaled(6.0F))
                return false;
            label_value(frame, layout_, x, y, label, value);
            y += body_step;
            return true;
        };

        if (draw_value("PARTICLES", number_string(stats.particle_count))
            && draw_value(
                "ACTIVE CELLS",
                number_string(stats.active_cell_count))
            && draw_value("TICK", number_string(simulation.tick())))
        {
            draw_value("STATE HASH", number_string(simulation.state_hash()));
        }

        y += layout_.scaled(3.0F);
        for (std::size_t index = 0; index < stats.metric_count; ++index)
        {
            if (!draw_value(
                    stats.metrics[index].label,
                    decimal_string(stats.metrics[index].value, 2)))
            {
                break;
            }
        }

        y = footer_top;
        frame.line(
            { x, y },
            {
                panel_left + layout_.panel_width
                    - panel_padding(layout_),
                y
            },
            1.0F,
            panel_border,
            220);
        y += layout_.scaled(10.0F);

        label_value(
            frame,
            layout_,
            x,
            y,
            "FPS",
            decimal_string(frames_per_second, 1));
        y += footer_step;
        label_value(
            frame,
            layout_,
            x,
            y,
            "FRAME MS",
            decimal_string(frame_milliseconds, 2));
        y += footer_step;
        label_value(
            frame,
            layout_,
            x,
            y,
            "TIME SCALE",
            decimal_string(simulation.time_scale(), 2));
        y += footer_step;
        label_value(
            frame,
            layout_,
            x,
            y,
            "WORKERS",
            number_string(simulation.worker_count()));
        y += footer_step;
        label_value(
            frame,
            layout_,
            x,
            y,
            "GPU ITEMS",
            number_string(gpu_capacity));
        y += footer_step;

        const TextSize footer_size = layout_.text_size(12.0F);
        frame.text(
            { x, y },
            compact_device_name(device_name),
            footer_size,
            text_muted,
            240);
        y += footer_step;
        frame.text(
            { x, y },
            simulation.paused()
                ? "PAUSED  SPACE PLAY"
                : "SPACE PAUSE  R RESET  . STEP",
            footer_size,
            simulation.paused() ? accent : text_secondary,
            240);
        y += footer_step;
        frame.text(
            { x, y },
            "TAB SCENE  - + SPEED  F1 PANEL",
            footer_size,
            text_muted,
            240);
    }
}
