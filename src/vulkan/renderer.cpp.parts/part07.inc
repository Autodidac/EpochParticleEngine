        [[nodiscard]] std::optional<std::uint32_t> find_memory_type(
            std::uint32_t type_bits,
            VkMemoryPropertyFlags required,
            VkMemoryPropertyFlags preferred) const noexcept
        {
            VkPhysicalDeviceMemoryProperties properties{};
            vkGetPhysicalDeviceMemoryProperties(physical_device_, &properties);

            std::optional<std::uint32_t> required_only;
            for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index)
            {
                const bool allowed = (type_bits & (1U << index)) != 0;
                const VkMemoryPropertyFlags flags = properties.memoryTypes[index].propertyFlags;
                if (!allowed || (flags & required) != required)
                    continue;
                if ((flags & preferred) == preferred)
                    return index;
                if (!required_only.has_value())
                    required_only = index;
            }
            return required_only;
        }

        void destroy_item_buffer(FrameResources& resources) noexcept
        {
            if (resources.mapped_items != nullptr && resources.item_memory != VK_NULL_HANDLE)
                vkUnmapMemory(device_, resources.item_memory);
            resources.mapped_items = nullptr;
            if (resources.item_buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(device_, resources.item_buffer, nullptr);
            if (resources.item_memory != VK_NULL_HANDLE)
                vkFreeMemory(device_, resources.item_memory, nullptr);
            resources.item_buffer = VK_NULL_HANDLE;
            resources.item_memory = VK_NULL_HANDLE;
            resources.allocation_size = 0;
            resources.buffer_size = 0;
            resources.host_coherent = false;
        }

        [[nodiscard]] std::expected<void, RendererError> create_item_buffer(
            FrameResources& resources,
            std::size_t capacity)
        {
            if (capacity == 0 || capacity > maximum_item_capacity_)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "render-item buffer capacity exceeds the selected device limit"));
            }

            const VkDeviceSize requested_size =
                static_cast<VkDeviceSize>(capacity) * sizeof(GpuRenderItem);
            const VkBufferCreateInfo buffer_info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .size = requested_size,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr
            };
            VkResult result = vkCreateBuffer(device_, &buffer_info, nullptr, &resources.item_buffer);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateBuffer(item storage)",
                    result));
            }

            resources.buffer_size = requested_size;

            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device_, resources.item_buffer, &requirements);
            const auto memory_type = find_memory_type(
                requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (!memory_type.has_value())
            {
                destroy_item_buffer(resources);
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "No host-visible memory type is available for particle upload"));
            }

            VkPhysicalDeviceMemoryProperties memory_properties{};
            vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
            resources.host_coherent =
                (memory_properties.memoryTypes[*memory_type].propertyFlags &
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

            const VkMemoryAllocateInfo allocation_info{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = nullptr,
                .allocationSize = requirements.size,
                .memoryTypeIndex = *memory_type
            };
            result = vkAllocateMemory(device_, &allocation_info, nullptr, &resources.item_memory);
            if (result != VK_SUCCESS)
            {
                destroy_item_buffer(resources);
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkAllocateMemory(item storage)",
                    result));
            }
            resources.allocation_size = requirements.size;

            result = vkBindBufferMemory(device_, resources.item_buffer, resources.item_memory, 0);
            if (result != VK_SUCCESS)
            {
                destroy_item_buffer(resources);
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkBindBufferMemory(item storage)",
                    result));
            }

            result = vkMapMemory(
                device_,
                resources.item_memory,
                0,
                resources.allocation_size,
                0,
                &resources.mapped_items);
            if (result != VK_SUCCESS)
            {
                destroy_item_buffer(resources);
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkMapMemory(item storage)",
                    result));
            }
            return {};
        }

