#if !defined(__linux__)
#error native_surface_xlib.cpp must only be compiled on Linux
#endif

#define VK_USE_PLATFORM_XLIB_KHR

#include "native_surface.hpp"

#include <X11/Xlib.h>

#include <array>
#include <cstdint>
#include <string>

namespace epochengine::particle::vulkan::detail
{
    std::span<const char* const> required_instance_extensions(
        const NativeSurfaceKind kind) noexcept
    {
        static constexpr std::array xlib{
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_XLIB_SURFACE_EXTENSION_NAME
        };
        return kind == NativeSurfaceKind::xlib
            ? std::span<const char* const>{ xlib.data(), xlib.size() }
            : std::span<const char* const>{};
    }

    std::expected<VkSurfaceKHR, RendererError> create_surface(
        const VkInstance instance,
        const NativeSurface& surface) noexcept
    {
        if (surface.kind != NativeSurfaceKind::xlib
            || surface.display == nullptr
            || surface.value == 0)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::invalid_window,
                "Xlib Vulkan surface requires a valid Display and Window"
            });
        }

        const auto create_xlib_surface =
            reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
                vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));
        if (create_xlib_surface == nullptr)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::initialization_failed,
                "Vulkan loader did not expose vkCreateXlibSurfaceKHR"
            });
        }

        const VkXlibSurfaceCreateInfoKHR create_info{
            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .dpy = static_cast<Display*>(surface.display),
            .window = static_cast<::Window>(surface.value)
        };
        VkSurfaceKHR result_surface = VK_NULL_HANDLE;
        const VkResult result = create_xlib_surface(
            instance,
            &create_info,
            nullptr,
            &result_surface);
        if (result != VK_SUCCESS)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::initialization_failed,
                "vkCreateXlibSurfaceKHR failed with VkResult "
                    + std::to_string(static_cast<std::int32_t>(result))
            });
        }
        return result_surface;
    }

    NativeExtent query_extent(const NativeSurface& surface) noexcept
    {
        if (surface.kind != NativeSurfaceKind::xlib
            || surface.display == nullptr
            || surface.value == 0)
        {
            return {};
        }

        XWindowAttributes attributes{};
        if (XGetWindowAttributes(
                static_cast<Display*>(surface.display),
                static_cast<::Window>(surface.value),
                &attributes) == 0
            || attributes.width <= 0
            || attributes.height <= 0)
        {
            return {};
        }
        return {
            static_cast<std::uint32_t>(attributes.width),
            static_cast<std::uint32_t>(attributes.height)
        };
    }
}
