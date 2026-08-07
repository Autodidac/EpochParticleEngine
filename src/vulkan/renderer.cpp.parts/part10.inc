            VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
            VkResult result = vkCreateSwapchainKHR(device_, &create_info, nullptr, &new_swapchain);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateSwapchainKHR",
                    result));
            }

            image_count = 0;
            result = vkGetSwapchainImagesKHR(device_, new_swapchain, &image_count, nullptr);
            if (result != VK_SUCCESS || image_count == 0)
            {
                vkDestroySwapchainKHR(device_, new_swapchain, nullptr);
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkGetSwapchainImagesKHR",
                    result));
            }
            std::vector<VkImage> new_images(image_count);
            result = vkGetSwapchainImagesKHR(device_, new_swapchain, &image_count, new_images.data());
            if (result != VK_SUCCESS)
            {
                vkDestroySwapchainKHR(device_, new_swapchain, nullptr);
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkGetSwapchainImagesKHR",
                    result));
            }

            swapchain_ = new_swapchain;
            swapchain_images_ = std::move(new_images);
            swapchain_format_ = surface_format.format;
            swapchain_extent_ = extent;
            images_in_flight_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_image_views()
        {
            swapchain_image_views_.resize(swapchain_images_.size(), VK_NULL_HANDLE);
            for (std::size_t index = 0; index < swapchain_images_.size(); ++index)
            {
                const VkImageViewCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .image = swapchain_images_[index],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapchain_format_,
                    .components = {
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY
                    },
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                };
                const VkResult result = vkCreateImageView(
                    device_,
                    &info,
                    nullptr,
                    &swapchain_image_views_[index]);
                if (result != VK_SUCCESS)
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::initialization_failed,
                        "vkCreateImageView",
                        result));
                }
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_render_pass()
        {
            const VkAttachmentDescription color_attachment{
                .flags = 0,
                .format = swapchain_format_,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            };
            const VkAttachmentReference color_reference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            };
            const VkSubpassDescription subpass{
                .flags = 0,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 0,
                .pInputAttachments = nullptr,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_reference,
                .pResolveAttachments = nullptr,
                .pDepthStencilAttachment = nullptr,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments = nullptr
            };
            const VkSubpassDependency dependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = 0
            };
            const VkRenderPassCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .attachmentCount = 1,
                .pAttachments = &color_attachment,
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = 1,
                .pDependencies = &dependency
            };
            const VkResult result = vkCreateRenderPass(device_, &info, nullptr, &render_pass_);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateRenderPass",
                    result));
            }
            return {};
        }
