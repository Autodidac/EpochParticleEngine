#include <epochengine/particle/vulkan/renderer.hpp>

#include "../native_surface.hpp"
#include "../shader_sources.hpp"

#include <shaderc/shaderc.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace epochengine::particle::vulkan
{
    namespace
    {
        constexpr std::size_t frames_in_flight = 2;

        [[nodiscard]] std::string vk_result_name(VkResult result)
        {
            switch (result)
            {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            default:
                return "VkResult(" + std::to_string(static_cast<std::int32_t>(result)) + ')';
            }
        }

        [[nodiscard]] RendererError make_error(
            RendererErrorCode code,
            std::string_view operation,
            VkResult result)
        {
            std::string message;
            message.reserve(operation.size() + 48);
            message.append(operation);
            message.append(" failed: ");
            message.append(vk_result_name(result));
            return { code, std::move(message) };
        }

        [[nodiscard]] RendererError make_error(RendererErrorCode code, std::string message)
        {
            return { code, std::move(message) };
        }

        [[nodiscard]] RendererErrorCode error_code_for(VkResult result) noexcept
        {
            return result == VK_ERROR_DEVICE_LOST
                ? RendererErrorCode::device_lost
                : RendererErrorCode::rendering_failed;
        }

        [[nodiscard]] bool contains_name(
            std::span<const VkExtensionProperties> properties,
            const char* name) noexcept
        {
            return std::ranges::any_of(properties, [name](const VkExtensionProperties& property)
            {
                return std::strcmp(property.extensionName, name) == 0;
            });
        }

        [[nodiscard]] bool contains_name(
            std::span<const VkLayerProperties> properties,
            const char* name) noexcept
        {
            return std::ranges::any_of(properties, [name](const VkLayerProperties& property)
            {
                return std::strcmp(property.layerName, name) == 0;
            });
        }

        void append_unique(std::vector<const char*>& values, const char* value)
        {
            if (std::ranges::none_of(values, [value](const char* existing)
                {
                    return std::strcmp(existing, value) == 0;
                }))
            {
                values.push_back(value);
            }
        }

        VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
            void*)
        {
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT && callback_data != nullptr)
            {
                std::fputs("[EpochParticleEngine/Vulkan] ", stderr);
                std::fputs(callback_data->pMessage != nullptr ? callback_data->pMessage : "validation message", stderr);
                std::fputc('\n', stderr);
            }
            return VK_FALSE;
        }

        [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info() noexcept
        {
            VkDebugUtilsMessengerCreateInfoEXT info{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext = nullptr,
                .flags = 0,
                .messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = &debug_callback,
                .pUserData = nullptr
            };
            return info;
        }

