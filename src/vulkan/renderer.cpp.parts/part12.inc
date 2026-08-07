            const VkResult result = vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &info,
                nullptr,
                &pipeline_);
            vkDestroyShaderModule(device_, fragment_module, nullptr);
            vkDestroyShaderModule(device_, vertex_module, nullptr);

            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateGraphicsPipelines",
                    result));
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_framebuffers()
        {
            swapchain_framebuffers_.resize(swapchain_image_views_.size(), VK_NULL_HANDLE);
            for (std::size_t index = 0; index < swapchain_image_views_.size(); ++index)
            {
                const VkImageView attachment = swapchain_image_views_[index];
                const VkFramebufferCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .renderPass = render_pass_,
                    .attachmentCount = 1,
                    .pAttachments = &attachment,
                    .width = swapchain_extent_.width,
                    .height = swapchain_extent_.height,
                    .layers = 1
                };
                const VkResult result = vkCreateFramebuffer(
                    device_,
                    &info,
                    nullptr,
                    &swapchain_framebuffers_[index]);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::initialization_failed,
                        "vkCreateFramebuffer",
                        result));
                }
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_swapchain_resources(
            VkSwapchainKHR old_swapchain)
        {
            if (auto result = create_swapchain(old_swapchain); !result)
                return result;
            if (auto result = create_image_views(); !result)
                return result;
            if (auto result = create_render_pass(); !result)
                return result;
            if (auto result = create_pipeline(); !result)
                return result;
            if (auto result = create_framebuffers(); !result)
                return result;
            return {};
        }

        void destroy_swapchain_resources(bool destroy_swapchain) noexcept
        {
            for (VkFramebuffer framebuffer : swapchain_framebuffers_)
            {
                if (framebuffer != VK_NULL_HANDLE)
                    vkDestroyFramebuffer(device_, framebuffer, nullptr);
            }
            swapchain_framebuffers_.clear();

            if (pipeline_ != VK_NULL_HANDLE)
                vkDestroyPipeline(device_, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;

            if (render_pass_ != VK_NULL_HANDLE)
                vkDestroyRenderPass(device_, render_pass_, nullptr);
            render_pass_ = VK_NULL_HANDLE;

            for (VkImageView image_view : swapchain_image_views_)
            {
                if (image_view != VK_NULL_HANDLE)
                    vkDestroyImageView(device_, image_view, nullptr);
            }
            swapchain_image_views_.clear();
            swapchain_images_.clear();
            images_in_flight_.clear();

            if (destroy_swapchain && swapchain_ != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(device_, swapchain_, nullptr);
                swapchain_ = VK_NULL_HANDLE;
            }
        }

        [[nodiscard]] std::expected<void, RendererError> recreate_swapchain()
        {
            const detail::NativeExtent native_extent = detail::query_extent(surface_source_);
            if (!native_extent.visible())
                return {};

            VkResult result = vkDeviceWaitIdle(device_);
            if (result != VK_SUCCESS)
                return std::unexpected(make_error(error_code_for(result), "vkDeviceWaitIdle", result));

            const VkSwapchainKHR old_swapchain = swapchain_;
            swapchain_ = VK_NULL_HANDLE;
            destroy_swapchain_resources(false);

            auto create_result = create_swapchain_resources(old_swapchain);
            vkDestroySwapchainKHR(device_, old_swapchain, nullptr);
            return create_result;
        }

        [[nodiscard]] std::expected<void, RendererError> grow_item_buffers(std::size_t required)
        {
            if (required > maximum_item_capacity_)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::rendering_failed,
                    "render-item buffer growth exceeds the selected device limit"));
            }

            const std::size_t new_capacity = normalized_capacity(
                required,
                maximum_item_capacity_);
            const VkResult idle_result = vkDeviceWaitIdle(device_);
            if (idle_result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    error_code_for(idle_result),
                    "vkDeviceWaitIdle before storage-buffer growth",
                    idle_result));
            }

