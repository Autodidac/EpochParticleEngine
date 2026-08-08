#include <epochengine/particle/render_frame.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace epochengine::particle
{
    namespace
    {
        using Glyph = std::array<std::uint8_t, bitmap_glyph_height>;

        [[nodiscard]] bool finite(Vec2 value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        [[nodiscard]] bool finite(Color value) noexcept
        {
            return std::isfinite(value.r) && std::isfinite(value.g)
                && std::isfinite(value.b) && std::isfinite(value.a);
        }

        [[nodiscard]] bool valid_shape(PrimitiveShape shape) noexcept
        {
            return shape == PrimitiveShape::circle
                || shape == PrimitiveShape::rectangle
                || shape == PrimitiveShape::rounded_rectangle;
        }

        [[nodiscard]] constexpr Glyph glyph_for(char character) noexcept
        {
            if (character >= 'a' && character <= 'z')
                character = static_cast<char>(character - 'a' + 'A');

            switch (character)
            {
            case 'A': return { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
            case 'B': return { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E };
            case 'C': return { 0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F };
            case 'D': return { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E };
            case 'E': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
            case 'F': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 };
            case 'G': return { 0x0F, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0F };
            case 'H': return { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
            case 'I': return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F };
            case 'J': return { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C };
            case 'K': return { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
            case 'L': return { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F };
            case 'M': return { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 };
            case 'N': return { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
            case 'O': return { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
            case 'P': return { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 };
            case 'Q': return { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D };
            case 'R': return { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
            case 'S': return { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
            case 'T': return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
            case 'U': return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
            case 'V': return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 };
            case 'W': return { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A };
            case 'X': return { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 };
            case 'Y': return { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 };
            case 'Z': return { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F };
            case '0': return { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E };
            case '1': return { 0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F };
            case '2': return { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F };
            case '3': return { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E };
            case '4': return { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 };
            case '5': return { 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E };
            case '6': return { 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E };
            case '7': return { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
            case '8': return { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E };
            case '9': return { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E };
            case ':': return { 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00 };
            case '.': return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C };
            case ',': return { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 };
            case '-': return { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 };
            case '+': return { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 };
            case '/': return { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 };
            case '\\': return { 0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01 };
            case '(': return { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 };
            case ')': return { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 };
            case '[': return { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E };
            case ']': return { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E };
            case '=': return { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 };
            case '%': return { 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13 };
            case '#': return { 0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00 };
            case '_': return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F };
            case '!': return { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 };
            case '?': return { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 };
            default: return {};
            }
        }
    }

    RenderFrame::RenderFrame(std::size_t maximum_items)
        : maximum_items_(std::max<std::size_t>(maximum_items, 1U))
    {
        items_.reserve(std::min<std::size_t>(maximum_items_, 65'536U));
    }

    void RenderFrame::begin(Bounds extent)
    {
        extent_ = extent.valid() ? extent : Bounds{};
        items_.clear();
        dropped_items_ = 0;
        finalized_ = false;
    }

    void RenderFrame::reserve(std::size_t item_count)
    {
        items_.reserve(std::min(item_count, maximum_items_));
    }

    void RenderFrame::add(RenderItem item)
    {
        if (items_.size() >= maximum_items_
            || !finite(item.center)
            || !finite(item.half_extent)
            || !finite(item.color)
            || !std::isfinite(item.rotation)
            || !std::isfinite(item.corner_radius)
            || !valid_shape(item.shape))
        {
            ++dropped_items_;
            return;
        }

        item.half_extent.x = std::max(item.half_extent.x, 0.0F);
        item.half_extent.y = std::max(item.half_extent.y, 0.0F);
        item.color.a = clamp(item.color.a, 0.0F, 1.0F);
        item.corner_radius = std::max(item.corner_radius, 0.0F);
        items_.push_back(item);
        finalized_ = false;
    }

    void RenderFrame::circle(
        Vec2 center,
        float radius,
        Color color,
        std::int32_t layer)
    {
        add({
            .center = center,
            .half_extent = { radius, radius },
            .color = color,
            .rotation = 0.0F,
            .shape = PrimitiveShape::circle,
            .corner_radius = radius,
            .layer = layer
        });
    }

    void RenderFrame::rectangle(
        Vec2 center,
        Vec2 half_extent,
        Color color,
        std::int32_t layer,
        float rotation)
    {
        add({
            .center = center,
            .half_extent = half_extent,
            .color = color,
            .rotation = rotation,
            .shape = PrimitiveShape::rectangle,
            .corner_radius = 0.0F,
            .layer = layer
        });
    }

    void RenderFrame::rounded_rectangle(
        Vec2 center,
        Vec2 half_extent,
        float radius,
        Color color,
        std::int32_t layer)
    {
        add({
            .center = center,
            .half_extent = half_extent,
            .color = color,
            .rotation = 0.0F,
            .shape = PrimitiveShape::rounded_rectangle,
            .corner_radius = std::max(radius, 0.0F),
            .layer = layer
        });
    }

    void RenderFrame::line(
        Vec2 from,
        Vec2 to,
        float thickness,
        Color color,
        std::int32_t layer)
    {
        const Vec2 delta = to - from;
        const float magnitude = length(delta);
        if (magnitude <= 1.0e-5F)
        {
            circle(from, thickness * 0.5F, color, layer);
            return;
        }

        rectangle(
            (from + to) * 0.5F,
            { magnitude * 0.5F, thickness * 0.5F },
            color,
            layer,
            std::atan2(delta.y, delta.x));
    }

    void RenderFrame::text(
        Vec2 position,
        std::string_view value,
        TextSize size,
        Color color,
        std::int32_t layer,
        TextAlign alignment,
        float letter_spacing,
        float line_spacing)
    {
        if (!finite(position) || !finite(color)
            || !std::isfinite(size.logical_height)
            || !std::isfinite(size.dpi_scale)
            || !std::isfinite(letter_spacing)
            || !std::isfinite(line_spacing)
            || size.logical_height <= 0.0F
            || size.dpi_scale <= 0.0F
            || value.empty())
        {
            return;
        }

        const BitmapTextMetrics metrics = make_bitmap_text_metrics(
            size,
            letter_spacing,
            line_spacing);
        const float origin_x = position.x;
        float cursor_y = position.y;
        std::size_t line_begin = 0;

        while (line_begin <= value.size())
        {
            const std::size_t line_end = value.find('\n', line_begin);
            const std::string_view line = line_end == std::string_view::npos
                ? value.substr(line_begin)
                : value.substr(line_begin, line_end - line_begin);

            float cursor_x = origin_x;
            const float width = text_width(line, metrics);
            if (alignment == TextAlign::center)
                cursor_x -= width * 0.5F;
            else if (alignment == TextAlign::right)
                cursor_x -= width;

            for (const char character : line)
            {
                if (character != ' ')
                {
                    const Glyph glyph = glyph_for(character);
                    for (std::size_t row = 0; row < glyph.size(); ++row)
                    {
                        for (std::size_t column = 0;
                             column < bitmap_glyph_width;
                             ++column)
                        {
                            const std::uint8_t mask = static_cast<std::uint8_t>(
                                1U << (bitmap_glyph_width - 1U - column));
                            if ((glyph[row] & mask) == 0)
                                continue;

                            rectangle(
                                {
                                    cursor_x
                                        + (static_cast<float>(column) + 0.5F)
                                            * metrics.cell_size,
                                    cursor_y
                                        + (static_cast<float>(row) + 0.5F)
                                            * metrics.cell_size
                                },
                                {
                                    metrics.cell_size * 0.45F,
                                    metrics.cell_size * 0.45F
                                },
                                color,
                                layer);
                        }
                    }
                }
                cursor_x += metrics.advance;
            }

            if (line_end == std::string_view::npos)
                break;
            line_begin = line_end + 1U;
            cursor_y += metrics.line_advance;
        }
    }

    void RenderFrame::text(
        Vec2 position,
        std::string_view value,
        float legacy_cell_scale,
        Color color,
        std::int32_t layer,
        TextAlign alignment)
    {
        if (!std::isfinite(legacy_cell_scale)
            || legacy_cell_scale <= 0.0F)
        {
            return;
        }

        text(
            position,
            value,
            TextSize{
                .logical_height = static_cast<float>(bitmap_glyph_height)
                    * legacy_cell_scale,
                .dpi_scale = 1.0F
            },
            color,
            layer,
            alignment);
    }

    void RenderFrame::finalize()
    {
        if (finalized_)
            return;

        std::stable_sort(
            items_.begin(),
            items_.end(),
            [](const RenderItem& lhs, const RenderItem& rhs)
            {
                return lhs.layer < rhs.layer;
            });
        finalized_ = true;
    }

    std::span<const RenderItem> RenderFrame::items() const noexcept
    {
        return items_;
    }

    Bounds RenderFrame::extent() const noexcept
    {
        return extent_;
    }

    std::size_t RenderFrame::dropped_items() const noexcept
    {
        return dropped_items_;
    }

    std::size_t RenderFrame::maximum_items() const noexcept
    {
        return maximum_items_;
    }

    float RenderFrame::text_width(
        std::string_view value,
        const BitmapTextMetrics& metrics) const noexcept
    {
        if (value.empty())
            return 0.0F;
        return static_cast<float>(value.size() - 1U) * metrics.advance
            + metrics.glyph_width;
    }
}
