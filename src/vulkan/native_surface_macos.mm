#if !defined(__APPLE__)
#error native_surface_macos.mm must only be compiled on Apple platforms
#endif

#define VK_USE_PLATFORM_METAL_EXT

#include "native_surface.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <cstdint>
#include <string>

namespace epochengine::particle::vulkan::detail
{
    std::span<const char* const> required_instance_extensions(
        const NativeSurfaceKind kind) noexcept
    {
        static constexpr std::array metal{
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_EXT_METAL_SURFACE_EXTENSION_NAME
        };
        return kind == NativeSurfaceKind::metal
            ? std::span<const char* const>{ metal.data(), metal.size() }
            : std::span<const char* const>{};
    }

    std::expected<VkSurfaceKHR, RendererError> create_surface(
        const VkInstance instance,
        const NativeSurface& surface) noexcept
    {
        if (surface.kind != NativeSurfaceKind::metal || surface.window == nullptr)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::invalid_window,
                "Metal Vulkan surface requires a valid NSWindow"
            });
        }

        const auto create_metal_surface =
            reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
                vkGetInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT"));
        if (create_metal_surface == nullptr)
        {
            return std::unexpected(RendererError{
                RendererErrorCode::initialization_failed,
                "Vulkan loader did not expose vkCreateMetalSurfaceEXT"
            });
        }

        @autoreleasepool
        {
            NSWindow* window = static_cast<NSWindow*>(surface.window);
            NSView* view = [window contentView];
            if (view == nil)
            {
                return std::unexpected(RendererError{
                    RendererErrorCode::invalid_window,
                    "NSWindow has no content view"
                });
            }

            [view setWantsLayer:YES];
            CAMetalLayer* layer = [view layer] != nil
                    && [[view layer] isKindOfClass:[CAMetalLayer class]]
                ? static_cast<CAMetalLayer*>([view layer])
                : [CAMetalLayer layer];
            [view setLayer:layer];
            [layer setContentsScale:[window backingScaleFactor]];

            const VkMetalSurfaceCreateInfoEXT create_info{
                .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
                .pNext = nullptr,
                .flags = 0,
                .pLayer = layer
            };
            VkSurfaceKHR result_surface = VK_NULL_HANDLE;
            const VkResult result = create_metal_surface(
                instance,
                &create_info,
                nullptr,
                &result_surface);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(RendererError{
                    RendererErrorCode::initialization_failed,
                    "vkCreateMetalSurfaceEXT failed with VkResult "
                        + std::to_string(static_cast<std::int32_t>(result))
                });
            }
            return result_surface;
        }
    }

    NativeExtent query_extent(const NativeSurface& surface) noexcept
    {
        if (surface.kind != NativeSurfaceKind::metal || surface.window == nullptr)
            return {};

        @autoreleasepool
        {
            NSWindow* window = static_cast<NSWindow*>(surface.window);
            NSView* view = [window contentView];
            if (view == nil)
                return {};
            const NSRect backing = [view convertRectToBacking:[view bounds]];
            if (backing.size.width <= 0.0 || backing.size.height <= 0.0)
                return {};
            return {
                static_cast<std::uint32_t>(backing.size.width),
                static_cast<std::uint32_t>(backing.size.height)
            };
        }
    }
}
