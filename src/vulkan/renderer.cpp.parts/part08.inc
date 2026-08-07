        void update_descriptor(FrameResources& resources) noexcept
        {
            const VkDescriptorBufferInfo buffer_info{
                .buffer = resources.item_buffer,
                .offset = 0,
                .range = resources.buffer_size
            };
            const VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = resources.descriptor_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &buffer_info,
                .pTexelBufferView = nullptr
            };
            vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        }

        [[nodiscard]] std::expected<void, RendererError> create_frame_resources()
        {
            const VkDescriptorPoolSize pool_size{
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = static_cast<std::uint32_t>(frames_in_flight)
            };
            const VkDescriptorPoolCreateInfo pool_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .maxSets = static_cast<std::uint32_t>(frames_in_flight),
                .poolSizeCount = 1,
                .pPoolSizes = &pool_size
            };
            VkResult result = vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateDescriptorPool",
                    result));
            }

            const std::array<VkDescriptorSetLayout, frames_in_flight> layouts{
                descriptor_set_layout_, descriptor_set_layout_
            };
            std::array<VkDescriptorSet, frames_in_flight> sets{};
            const VkDescriptorSetAllocateInfo allocate_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = descriptor_pool_,
                .descriptorSetCount = static_cast<std::uint32_t>(sets.size()),
                .pSetLayouts = layouts.data()
            };
            result = vkAllocateDescriptorSets(device_, &allocate_info, sets.data());
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkAllocateDescriptorSets",
                    result));
            }

            item_capacity_ = normalized_capacity(
                config_.initial_item_capacity,
                maximum_item_capacity_);
            for (std::size_t index = 0; index < frames_.size(); ++index)
            {
                FrameResources& resources = frames_[index];
                resources.descriptor_set = sets[index];

                const VkCommandPoolCreateInfo pool_create_info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = graphics_family_
                };
                result = vkCreateCommandPool(
                    device_,
                    &pool_create_info,
                    nullptr,
                    &resources.command_pool);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::initialization_failed,
                        "vkCreateCommandPool",
                        result));
                }

                const VkCommandBufferAllocateInfo command_info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .pNext = nullptr,
                    .commandPool = resources.command_pool,
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1
                };
                result = vkAllocateCommandBuffers(device_, &command_info, &resources.command_buffer);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::initialization_failed,
                        "vkAllocateCommandBuffers",
                        result));
                }

                const VkSemaphoreCreateInfo semaphore_info{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0
                };
                result = vkCreateSemaphore(
                    device_,
                    &semaphore_info,
                    nullptr,
                    &resources.image_available);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::initialization_failed,
                        "vkCreateSemaphore(image available)",
                        result));
                }
                result = vkCreateSemaphore(
                    device_,
                    &semaphore_info,
                    nullptr,
                    &resources.render_finished);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::initialization_failed,
                        "vkCreateSemaphore(render finished)",
                        result));
                }

