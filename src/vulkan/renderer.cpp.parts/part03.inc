            const auto item_count = static_cast<std::uint32_t>(frame.items().size());
            if (auto record_result = record_commands(resources, image_index, item_count); !record_result)
                return record_result;

            result = vkResetFences(device_, 1, &resources.in_flight);
            if (result != VK_SUCCESS)
                return std::unexpected(make_error(error_code_for(result), "vkResetFences", result));

            const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            const VkSubmitInfo submit_info{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = nullptr,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &resources.image_available,
                .pWaitDstStageMask = &wait_stage,
                .commandBufferCount = 1,
                .pCommandBuffers = &resources.command_buffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &resources.render_finished
            };

            result = vkQueueSubmit(graphics_queue_, 1, &submit_info, resources.in_flight);
            if (result != VK_SUCCESS)
                return std::unexpected(make_error(error_code_for(result), "vkQueueSubmit", result));
            images_in_flight_.at(image_index) = resources.in_flight;

            const VkPresentInfoKHR present_info{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .pNext = nullptr,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &resources.render_finished,
                .swapchainCount = 1,
                .pSwapchains = &swapchain_,
                .pImageIndices = &image_index,
                .pResults = nullptr
            };

            result = vkQueuePresentKHR(present_queue_, &present_info);
            const bool present_requires_rebuild =
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR;
            if (result != VK_SUCCESS && !present_requires_rebuild)
                return std::unexpected(make_error(error_code_for(result), "vkQueuePresentKHR", result));

            current_frame_ = (current_frame_ + 1) % frames_in_flight;

            if (acquire_suboptimal || present_requires_rebuild)
                return recreate_swapchain();
            return {};
        }

        void wait_idle() noexcept
        {
            if (device_ != VK_NULL_HANDLE)
                (void)vkDeviceWaitIdle(device_);
        }

        [[nodiscard]] const std::string& device_name() const noexcept
        {
            return device_name_;
        }

        [[nodiscard]] std::size_t item_capacity() const noexcept
        {
            return item_capacity_;
        }

        [[nodiscard]] std::size_t maximum_item_capacity() const noexcept
        {
            return maximum_item_capacity_;
        }

        [[nodiscard]] bool validation_enabled() const noexcept
        {
            return validation_enabled_;
        }

        [[nodiscard]] bool vertical_sync() const noexcept
        {
            return config_.vertical_sync;
        }


        [[nodiscard]] const NativeSurface& surface_source() const noexcept
        {
            return surface_source_;
        }

    private:
        struct FrameResources
        {
            VkCommandPool command_pool{ VK_NULL_HANDLE };
            VkCommandBuffer command_buffer{ VK_NULL_HANDLE };
            VkSemaphore image_available{ VK_NULL_HANDLE };
            VkSemaphore render_finished{ VK_NULL_HANDLE };
            VkFence in_flight{ VK_NULL_HANDLE };
            VkBuffer item_buffer{ VK_NULL_HANDLE };
            VkDeviceMemory item_memory{ VK_NULL_HANDLE };
            void* mapped_items{};
            VkDeviceSize allocation_size{};
            VkDeviceSize buffer_size{};
            bool host_coherent{};
            VkDescriptorSet descriptor_set{ VK_NULL_HANDLE };
        };

        [[nodiscard]] std::expected<void, RendererError> create_instance()
        {
            std::uint32_t extension_count = 0;
            VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkEnumerateInstanceExtensionProperties",
                    result));
            }
            std::vector<VkExtensionProperties> available_extensions(extension_count);
            result = vkEnumerateInstanceExtensionProperties(
                nullptr,
                &extension_count,
                available_extensions.data());
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkEnumerateInstanceExtensionProperties",
                    result));
            }

            std::uint32_t layer_count = 0;
            result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkEnumerateInstanceLayerProperties",
                    result));
            }
            std::vector<VkLayerProperties> available_layers(layer_count);
            result = vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkEnumerateInstanceLayerProperties",
                    result));
            }

