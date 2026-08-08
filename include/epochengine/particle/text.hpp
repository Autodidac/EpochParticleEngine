#pragma once

#include <cstdint>

namespace epochengine::particle
{
    inline constexpr std::uint32_t bitmap_glyph_width = 5U;
    inline constexpr std::uint32_t bitmap_glyph_height = 7U;
    inline constexpr std::uint32_t bitmap_glyph_advance = 6U;
    inline constexpr std::uint32_t bitmap_line_advance = 9U;
    inline constexpr float default_text_logical_height = 16.0F;
    inline constexpr float minimum_readable_text_logical_height = 12.0F;

    struct TextSize final
    {
        // Logical UI pixels. dpi_scale converts them to framebuffer pixels.
        float logical_height{ default_text_logical_height };
        float dpi_scale{ 1.0F };
    };

    struct BitmapTextMetrics final
    {
        float pixel_height{};
        float cell_size{};
        float glyph_width{};
        float glyph_height{};
        float advance{};
        float line_advance{};
    };

    [[nodiscard]] constexpr float resolved_text_pixel_height(TextSize size) noexcept
    {
        const float logical_height = size.logical_height > 0.0F
            ? size.logical_height
            : default_text_logical_height;
        const float dpi_scale = size.dpi_scale > 0.0F ? size.dpi_scale : 1.0F;
        return logical_height * dpi_scale;
    }

    [[nodiscard]] constexpr BitmapTextMetrics make_bitmap_text_metrics(
        TextSize size = {},
        float letter_spacing = 0.0F,
        float line_spacing = 0.0F) noexcept
    {
        const float pixel_height = resolved_text_pixel_height(size);
        const float cell_size = pixel_height / static_cast<float>(bitmap_glyph_height);
        const float safe_letter_spacing = letter_spacing > 0.0F ? letter_spacing : 0.0F;
        const float safe_line_spacing = line_spacing > 0.0F ? line_spacing : 0.0F;
        return {
            .pixel_height = pixel_height,
            .cell_size = cell_size,
            .glyph_width = cell_size * static_cast<float>(bitmap_glyph_width),
            .glyph_height = pixel_height,
            .advance = cell_size * static_cast<float>(bitmap_glyph_advance)
                + safe_letter_spacing,
            .line_advance = cell_size * static_cast<float>(bitmap_line_advance)
                + safe_line_spacing
        };
    }
}
