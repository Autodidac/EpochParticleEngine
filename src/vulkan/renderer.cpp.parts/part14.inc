            const PushConstants push_constants{
                .viewport = {
                    static_cast<float>(swapchain_extent_.width),
                    static_cast<float>(swapchain_extent_.height)
                }
            };
            vkCmdPushConstants(
                resources.command_buffer,
                pipeline_layout_,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(push_constants),
                &push_constants);

            if (item_count != 0)
            {
                vkCmdDraw(
                    resources.command_buffer,
                    6,
                    item_count,
                    0,
                    0);
            }
            vkCmdEndRenderPass(resources.command_buffer);

            result = vkEndCommandBuffer(resources.command_buffer);
            if (result != VK_SUCCESS)
                return std::unexpected(make_error(error_code_for(result), "vkEndCommandBuffer", result));
            return {};
        }

        void destroy() noexcept
        {
            if (device_ != VK_NULL_HANDLE)
                (void)vkDeviceWaitIdle(device_);

            if (device_ != VK_NULL_HANDLE)
            {
                destroy_swapchain_resources(true);

                for (FrameResources& resources : frames_)
                {
                    destroy_item_buffer(resources);
                    if (resources.in_flight != VK_NULL_HANDLE)
                        vkDestroyFence(device_, resources.in_flight, nullptr);
                    if (resources.render_finished != VK_NULL_HANDLE)
                        vkDestroySemaphore(device_, resources.render_finished, nullptr);
                    if (resources.image_available != VK_NULL_HANDLE)
                        vkDestroySemaphore(device_, resources.image_available, nullptr);
                    if (resources.command_pool != VK_NULL_HANDLE)
                        vkDestroyCommandPool(device_, resources.command_pool, nullptr);
                    resources = {};
                }

                if (descriptor_pool_ != VK_NULL_HANDLE)
                    vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
                if (pipeline_layout_ != VK_NULL_HANDLE)
                    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
                if (descriptor_set_layout_ != VK_NULL_HANDLE)
                    vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);

                vkDestroyDevice(device_, nullptr);
                device_ = VK_NULL_HANDLE;
            }

            if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE)
            {
                vkDestroySurfaceKHR(instance_, surface_, nullptr);
                surface_ = VK_NULL_HANDLE;
            }

            if (debug_messenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE)
            {
                const auto destroy_debug = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
                if (destroy_debug != nullptr)
                    destroy_debug(instance_, debug_messenger_, nullptr);
                debug_messenger_ = VK_NULL_HANDLE;
            }

            if (instance_ != VK_NULL_HANDLE)
            {
                vkDestroyInstance(instance_, nullptr);
                instance_ = VK_NULL_HANDLE;
            }
        }

        NativeSurface surface_source_{};
        RendererConfig config_{};
        bool validation_enabled_{};
        std::string device_name_{ "Unavailable" };

        VkInstance instance_{ VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT debug_messenger_{ VK_NULL_HANDLE };
        VkSurfaceKHR surface_{ VK_NULL_HANDLE };
        VkPhysicalDevice physical_device_{ VK_NULL_HANDLE };
        VkDevice device_{ VK_NULL_HANDLE };
        std::uint32_t graphics_family_{};
        std::uint32_t present_family_{};
        VkQueue graphics_queue_{ VK_NULL_HANDLE };
        VkQueue present_queue_{ VK_NULL_HANDLE };

        VkDescriptorSetLayout descriptor_set_layout_{ VK_NULL_HANDLE };
        VkDescriptorPool descriptor_pool_{ VK_NULL_HANDLE };
        VkPipelineLayout pipeline_layout_{ VK_NULL_HANDLE };
        std::array<FrameResources, frames_in_flight> frames_{};
        std::size_t item_capacity_{};
        std::size_t maximum_item_capacity_{};
        std::size_t current_frame_{};

        VkSwapchainKHR swapchain_{ VK_NULL_HANDLE };
        VkFormat swapchain_format_{ VK_FORMAT_UNDEFINED };
        VkExtent2D swapchain_extent_{};
        std::vector<VkImage> swapchain_images_;
        std::vector<VkImageView> swapchain_image_views_;
        std::vector<VkFramebuffer> swapchain_framebuffers_;
        std::vector<VkFence> images_in_flight_;
        VkRenderPass render_pass_{ VK_NULL_HANDLE };
        VkPipeline pipeline_{ VK_NULL_HANDLE };
    };

    Renderer::Renderer(std::unique_ptr<Impl> implementation) noexcept
        : impl_(std::move(implementation))
    {
    }

    std::expected<std::unique_ptr<Renderer>, RendererError> Renderer::create(
        NativeSurface surface,
        RendererConfig config)
    {
        auto implementation = std::make_unique<Impl>(surface, config);
        if (auto result = implementation->initialize(); !result)
            return std::unexpected(std::move(result.error()));
        return std::unique_ptr<Renderer>{ new Renderer{ std::move(implementation) } };
    }

