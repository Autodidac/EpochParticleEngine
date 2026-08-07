            const QueueFamilies families = find_queue_families(physical_device_);
            graphics_family_ = *families.graphics;
            present_family_ = *families.present;

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physical_device_, &properties);
            device_name_ = properties.deviceName;
            const std::size_t storage_capacity = static_cast<std::size_t>(
                properties.limits.maxStorageBufferRange) / sizeof(GpuRenderItem);
            maximum_item_capacity_ = std::min<std::size_t>(
                storage_capacity,
                std::numeric_limits<std::uint32_t>::max());
            if (maximum_item_capacity_ == 0)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::unavailable,
                    "Selected Vulkan device cannot expose one render item through a storage buffer"));
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_logical_device()
        {
            const std::set<std::uint32_t> unique_families{ graphics_family_, present_family_ };
            constexpr float priority = 1.0F;
            std::vector<VkDeviceQueueCreateInfo> queue_infos;
            queue_infos.reserve(unique_families.size());
            for (const std::uint32_t family : unique_families)
            {
                queue_infos.push_back({
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .queueFamilyIndex = family,
                    .queueCount = 1,
                    .pQueuePriorities = &priority
                });
            }

            std::vector<const char*> extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            const auto available_extensions = device_extensions(physical_device_);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            if (contains_name(available_extensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
                append_unique(extensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

            const VkPhysicalDeviceFeatures features{};
            const VkDeviceCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size()),
                .pQueueCreateInfos = queue_infos.data(),
                .enabledLayerCount = 0,
                .ppEnabledLayerNames = nullptr,
                .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
                .ppEnabledExtensionNames = extensions.data(),
                .pEnabledFeatures = &features
            };

            const VkResult result = vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateDevice",
                    result));
            }

            vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
            vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_descriptor_layout()
        {
            const VkDescriptorSetLayoutBinding binding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = nullptr
            };
            const VkDescriptorSetLayoutCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = 1,
                .pBindings = &binding
            };
            const VkResult result = vkCreateDescriptorSetLayout(
                device_,
                &info,
                nullptr,
                &descriptor_set_layout_);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateDescriptorSetLayout",
                    result));
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_pipeline_layout()
        {
            const VkPushConstantRange push_range{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(PushConstants)
            };
            const VkPipelineLayoutCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = 1,
                .pSetLayouts = &descriptor_set_layout_,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_range
            };
            const VkResult result = vkCreatePipelineLayout(device_, &info, nullptr, &pipeline_layout_);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreatePipelineLayout",
                    result));
            }
            return {};
        }

