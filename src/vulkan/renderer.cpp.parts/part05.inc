            QueueFamilies families;
            for (std::uint32_t index = 0; index < family_count; ++index)
            {
                if (properties[index].queueCount != 0 &&
                    (properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
                {
                    families.graphics = index;
                }

                VkBool32 present_support = VK_FALSE;
                if (properties[index].queueCount != 0)
                {
                    (void)vkGetPhysicalDeviceSurfaceSupportKHR(
                        device,
                        index,
                        surface_,
                        &present_support);
                }
                if (present_support == VK_TRUE)
                    families.present = index;

                if (families.complete())
                    break;
            }
            return families;
        }

        [[nodiscard]] std::vector<VkExtensionProperties> device_extensions(VkPhysicalDevice device) const
        {
            std::uint32_t count = 0;
            if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS)
                return {};
            std::vector<VkExtensionProperties> properties(count);
            if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, properties.data()) != VK_SUCCESS)
                return {};
            return properties;
        }

        [[nodiscard]] SwapchainSupport query_swapchain_support(VkPhysicalDevice device) const
        {
            SwapchainSupport support;
            if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    device,
                    surface_,
                    &support.capabilities) != VK_SUCCESS)
            {
                return {};
            }

            std::uint32_t format_count = 0;
            if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr) == VK_SUCCESS &&
                format_count != 0)
            {
                support.formats.resize(format_count);
                if (vkGetPhysicalDeviceSurfaceFormatsKHR(
                        device,
                        surface_,
                        &format_count,
                        support.formats.data()) != VK_SUCCESS)
                {
                    support.formats.clear();
                }
            }

            std::uint32_t mode_count = 0;
            if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &mode_count, nullptr) == VK_SUCCESS &&
                mode_count != 0)
            {
                support.present_modes.resize(mode_count);
                if (vkGetPhysicalDeviceSurfacePresentModesKHR(
                        device,
                        surface_,
                        &mode_count,
                        support.present_modes.data()) != VK_SUCCESS)
                {
                    support.present_modes.clear();
                }
            }
            return support;
        }

        [[nodiscard]] int score_device(VkPhysicalDevice device) const
        {
            const QueueFamilies families = find_queue_families(device);
            if (!families.complete())
                return -1;

            const auto extensions = device_extensions(device);
            if (!contains_name(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                return -1;

            if (!query_swapchain_support(device).adequate())
                return -1;

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);
            int score = static_cast<int>(std::min<std::uint32_t>(
                properties.limits.maxImageDimension2D,
                static_cast<std::uint32_t>(std::numeric_limits<int>::max() / 2)));
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                score += 100'000;
            else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                score += 50'000;
            return score;
        }

        [[nodiscard]] std::expected<void, RendererError> select_physical_device()
        {
            std::uint32_t count = 0;
            VkResult result = vkEnumeratePhysicalDevices(instance_, &count, nullptr);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkEnumeratePhysicalDevices",
                    result));
            }
            if (count == 0)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::unavailable,
                    "No Vulkan physical device is available"));
            }

            std::vector<VkPhysicalDevice> devices(count);
            result = vkEnumeratePhysicalDevices(instance_, &count, devices.data());
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkEnumeratePhysicalDevices",
                    result));
            }

            int best_score = -1;
            for (VkPhysicalDevice device : devices)
            {
                const int score = score_device(device);
                if (score > best_score)
                {
                    best_score = score;
                    physical_device_ = device;
                }
            }
            if (physical_device_ == VK_NULL_HANDLE)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::unavailable,
                    "No Vulkan device supports graphics, presentation, and VK_KHR_swapchain"));
            }

