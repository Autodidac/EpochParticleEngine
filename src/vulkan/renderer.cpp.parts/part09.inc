                const VkFenceCreateInfo fence_info{
                    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = VK_FENCE_CREATE_SIGNALED_BIT
                };
                result = vkCreateFence(device_, &fence_info, nullptr, &resources.in_flight);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::initialization_failed,
                        "vkCreateFence",
                        result));
                }

                if (auto buffer_result = create_item_buffer(resources, item_capacity_); !buffer_result)
                    return buffer_result;
                update_descriptor(resources);
            }
            return {};
        }

        [[nodiscard]] VkSurfaceFormatKHR choose_surface_format(
            std::span<const VkSurfaceFormatKHR> formats) const noexcept
        {
            for (const VkSurfaceFormatKHR& format : formats)
            {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return format;
                }
            }
            for (const VkSurfaceFormatKHR& format : formats)
            {
                if (format.format == VK_FORMAT_R8G8B8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return format;
                }
            }
            return formats.front();
        }

        [[nodiscard]] VkPresentModeKHR choose_present_mode(
            std::span<const VkPresentModeKHR> modes) const noexcept
        {
            if (config_.vertical_sync)
                return VK_PRESENT_MODE_FIFO_KHR;

            if (std::ranges::find(modes, VK_PRESENT_MODE_MAILBOX_KHR) != modes.end())
                return VK_PRESENT_MODE_MAILBOX_KHR;
            if (std::ranges::find(modes, VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end())
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        [[nodiscard]] VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities) const noexcept
        {
            if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
                return capabilities.currentExtent;

            const detail::NativeExtent native_extent = detail::query_extent(surface_source_);
            VkExtent2D extent{
                std::max(native_extent.width, 1U),
                std::max(native_extent.height, 1U)
            };
            extent.width = std::clamp(
                extent.width,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width);
            extent.height = std::clamp(
                extent.height,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);
            return extent;
        }

        [[nodiscard]] VkCompositeAlphaFlagBitsKHR choose_composite_alpha(
            VkCompositeAlphaFlagsKHR supported) const noexcept
        {
            constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> choices{
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
            };
            for (const auto choice : choices)
            {
                if ((supported & choice) != 0)
                    return choice;
            }
            return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }

        [[nodiscard]] std::expected<void, RendererError> create_swapchain(VkSwapchainKHR old_swapchain)
        {
            const SwapchainSupport support = query_swapchain_support(physical_device_);
            if (!support.adequate())
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "Surface has no usable Vulkan formats or presentation modes"));
            }

            const VkSurfaceFormatKHR surface_format = choose_surface_format(support.formats);
            const VkPresentModeKHR present_mode = choose_present_mode(support.present_modes);
            const VkExtent2D extent = choose_extent(support.capabilities);

            std::uint32_t image_count = support.capabilities.minImageCount + 1;
            if (support.capabilities.maxImageCount != 0)
                image_count = std::min(image_count, support.capabilities.maxImageCount);

            const std::array<std::uint32_t, 2> family_indices{ graphics_family_, present_family_ };
            const bool separate_queues = graphics_family_ != present_family_;
            const VkSwapchainCreateInfoKHR create_info{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .pNext = nullptr,
                .flags = 0,
                .surface = surface_,
                .minImageCount = image_count,
                .imageFormat = surface_format.format,
                .imageColorSpace = surface_format.colorSpace,
                .imageExtent = extent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = separate_queues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = separate_queues ? 2U : 0U,
                .pQueueFamilyIndices = separate_queues ? family_indices.data() : nullptr,
                .preTransform = support.capabilities.currentTransform,
                .compositeAlpha = choose_composite_alpha(support.capabilities.supportedCompositeAlpha),
                .presentMode = present_mode,
                .clipped = VK_TRUE,
                .oldSwapchain = old_swapchain
            };

