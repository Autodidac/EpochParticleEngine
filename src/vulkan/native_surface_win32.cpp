#if !defined(_WIN32)
#error native_surface_win32.cpp must only be compiled on Windows
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define VK_USE_PLATFORM_WIN32_KHR

#include "native_surface.hpp"

#include <windows.h>

#include <array>
#include <string>

namespace epochengine::particle::vulkan::detail
{
    std::span<const char* const> required_instance_extensions(
        const NativeSurfaceKind kind) noexcept
    {
        static constexpr std::array win32{
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
        };
        return kind == NativeSurfaceKind::win32
            ? std::span<const char* const>{ win32.data(), win32.size() }
            : std::span<const char* const>{};
    }

    std::expected<VkSurfaceKHR, RendererError> create_surface(
        const VkInstance instance,
        const NativeSurface& surface) noexcept
    {
        if (surface.kind != NativeSurfaceKind::win32 || surface.window == nullptr)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::invalid_window,
                "Win32 Vulkan surface requires a valid HWND"
            });
        }

        const auto window = static_cast<HWND>(surface.window);
        HINSTANCE application = reinterpret_cast<HINSTANCE>(
            GetWindowLongPtrW(window, GWLP_HINSTANCE));
        if (application == nullptr)
            application = GetModuleHandleW(nullptr);
        if (application == nullptr)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::initialization_failed,
                "Unable to resolve the Win32 application instance"
            });
        }

        const VkWin32SurfaceCreateInfoKHR create_info{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .hinstance = application,
            .hwnd = window
        };
        VkSurfaceKHR result_surface = VK_NULL_HANDLE;
        const VkResult result = vkCreateWin32SurfaceKHR(
            instance,
            &create_info,
            nullptr,
            &result_surface);
        if (result != VK_SUCCESS)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::initialization_failed,
                "vkCreateWin32SurfaceKHR failed with VkResult " +
                    std::to_string(static_cast<std::int32_t>(result))
            });
        }
        return result_surface;
    }

    NativeExtent query_extent(const NativeSurface& surface) noexcept
    {
        if (surface.kind != NativeSurfaceKind::win32 || surface.window == nullptr)
            return {};

        RECT rectangle{};
        if (!GetClientRect(static_cast<HWND>(surface.window), &rectangle))
            return {};
        const LONG width = rectangle.right - rectangle.left;
        const LONG height = rectangle.bottom - rectangle.top;
        if (width <= 0 || height <= 0)
            return {};

        return {
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height)
        };
    }
}
