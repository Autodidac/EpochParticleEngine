            std::array<FrameResources, frames_in_flight> replacements{};
            for (FrameResources& replacement : replacements)
            {
                if (auto result = create_item_buffer(replacement, new_capacity); !result)
                {
                    for (FrameResources& created : replacements)
                        destroy_item_buffer(created);
                    return result;
                }
            }

            for (std::size_t index = 0; index < frames_.size(); ++index)
            {
                FrameResources& resources = frames_[index];
                FrameResources& replacement = replacements[index];
                destroy_item_buffer(resources);
                resources.item_buffer = std::exchange(
                    replacement.item_buffer,
                    VK_NULL_HANDLE);
                resources.item_memory = std::exchange(
                    replacement.item_memory,
                    VK_NULL_HANDLE);
                resources.mapped_items = std::exchange(
                    replacement.mapped_items,
                    nullptr);
                resources.allocation_size = std::exchange(
                    replacement.allocation_size,
                    VkDeviceSize{});
                resources.buffer_size = std::exchange(
                    replacement.buffer_size,
                    VkDeviceSize{});
                resources.host_coherent = std::exchange(
                    replacement.host_coherent,
                    false);
                update_descriptor(resources);
            }
            item_capacity_ = new_capacity;
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> upload_items(
            FrameResources& resources,
            std::span<const RenderItem> items) noexcept
        {
            auto* destination = static_cast<GpuRenderItem*>(resources.mapped_items);
            for (std::size_t index = 0; index < items.size(); ++index)
                destination[index] = to_gpu_item(items[index]);

            if (!resources.host_coherent && !items.empty())
            {
                const VkMappedMemoryRange range{
                    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                    .pNext = nullptr,
                    .memory = resources.item_memory,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE
                };
                const VkResult result = vkFlushMappedMemoryRanges(device_, 1, &range);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        error_code_for(result),
                        "vkFlushMappedMemoryRanges",
                        result));
                }
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> record_commands(
            FrameResources& resources,
            std::uint32_t image_index,
            std::uint32_t item_count)
        {
            const VkCommandBufferBeginInfo begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr
            };
            VkResult result = vkBeginCommandBuffer(resources.command_buffer, &begin_info);
            if (result != VK_SUCCESS)
                return std::unexpected(make_error(error_code_for(result), "vkBeginCommandBuffer", result));

            const VkClearValue clear_value{
                .color = {
                    {
                        config_.clear_color.r,
                        config_.clear_color.g,
                        config_.clear_color.b,
                        config_.clear_color.a
                    }
                }
            };
            const VkRenderPassBeginInfo render_pass_info{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext = nullptr,
                .renderPass = render_pass_,
                .framebuffer = swapchain_framebuffers_.at(image_index),
                .renderArea = {
                    .offset = { 0, 0 },
                    .extent = swapchain_extent_
                },
                .clearValueCount = 1,
                .pClearValues = &clear_value
            };
            vkCmdBeginRenderPass(resources.command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(resources.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

            const VkViewport viewport{
                .x = 0.0F,
                .y = 0.0F,
                .width = static_cast<float>(swapchain_extent_.width),
                .height = static_cast<float>(swapchain_extent_.height),
                .minDepth = 0.0F,
                .maxDepth = 1.0F
            };
            const VkRect2D scissor{
                .offset = { 0, 0 },
                .extent = swapchain_extent_
            };
            vkCmdSetViewport(resources.command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(resources.command_buffer, 0, 1, &scissor);
            vkCmdBindDescriptorSets(
                resources.command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_layout_,
                0,
                1,
                &resources.descriptor_set,
                0,
                nullptr);

