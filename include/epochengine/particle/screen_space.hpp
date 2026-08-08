#pragma once

#include "types.hpp"

#include <cstdint>

namespace epochengine::particle
{
    enum class ScreenOrigin : std::uint8_t
    {
        upper_left = 0
    };

    struct ScreenSpaceConvention final
    {
        static constexpr ScreenOrigin origin = ScreenOrigin::upper_left;
        static constexpr bool positive_x_right = true;
        static constexpr bool positive_y_down = true;
    };

    // RenderFrame and platform pointer coordinates use framebuffer pixels with
    // an upper-left origin. A positive-height Vulkan viewport maps NDC -1 to
    // the upper edge, so both axes use the same pixel-to-NDC conversion.
    [[nodiscard]] constexpr Vec2 screen_to_vulkan_ndc(Vec2 pixel, Vec2 viewport) noexcept
    {
        const float safe_width = viewport.x > 1.0F ? viewport.x : 1.0F;
        const float safe_height = viewport.y > 1.0F ? viewport.y : 1.0F;
        return {
            pixel.x / safe_width * 2.0F - 1.0F,
            pixel.y / safe_height * 2.0F - 1.0F
        };
    }

    namespace detail
    {
        inline constexpr Vec2 screen_space_test_extent{ 1'280.0F, 720.0F };
        inline constexpr Vec2 screen_space_top_left =
            screen_to_vulkan_ndc({ 0.0F, 0.0F }, screen_space_test_extent);
        inline constexpr Vec2 screen_space_center =
            screen_to_vulkan_ndc({ 640.0F, 360.0F }, screen_space_test_extent);
        inline constexpr Vec2 screen_space_bottom_right =
            screen_to_vulkan_ndc(screen_space_test_extent, screen_space_test_extent);

        static_assert(screen_space_top_left.x == -1.0F && screen_space_top_left.y == -1.0F);
        static_assert(screen_space_center.x == 0.0F && screen_space_center.y == 0.0F);
        static_assert(screen_space_bottom_right.x == 1.0F && screen_space_bottom_right.y == 1.0F);
    }
}
