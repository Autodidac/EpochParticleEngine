#pragma once

#include <epochengine/particle/vulkan/renderer.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <span>

namespace epochengine::particle::vulkan::detail
{
    struct NativeExtent final
    {
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] constexpr bool visible() const noexcept
        {
            return width != 0 && height != 0;
        }
    };

    [[nodiscard]] std::span<const char* const> required_instance_extensions(
        NativeSurfaceKind kind) noexcept;

    [[nodiscard]] std::expected<VkSurfaceKHR, RendererError> create_surface(
        VkInstance instance,
        const NativeSurface& surface) noexcept;

    [[nodiscard]] NativeExtent query_extent(const NativeSurface& surface) noexcept;
}
