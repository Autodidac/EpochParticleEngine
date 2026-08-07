            const std::size_t byte_length = shaderc_result_get_length(result);
            if (byte_length == 0 || byte_length % sizeof(std::uint32_t) != 0)
            {
                shaderc_result_release(result);
                return std::unexpected(make_error(
                    RendererErrorCode::shader_compilation_failed,
                    std::string{ name } + ": shaderc returned malformed SPIR-V"));
            }

            std::vector<std::uint32_t> words(byte_length / sizeof(std::uint32_t));
            std::memcpy(words.data(), shaderc_result_get_bytes(result), byte_length);
            shaderc_result_release(result);
            return words;
        }
    }

    class Renderer::Impl
    {
    public:
        Impl(NativeSurface surface, RendererConfig config)
            : surface_source_(surface), config_(config)
        {
        }

        ~Impl()
        {
            destroy();
        }

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;

        [[nodiscard]] std::expected<void, RendererError> initialize()
        {
            if (!surface_source_.valid())
            {
                return std::unexpected(make_error(
                    RendererErrorCode::invalid_window,
                    "Vulkan renderer requires a valid native window surface"));
            }
            if (detail::required_instance_extensions(surface_source_.kind).empty())
            {
                return std::unexpected(make_error(
                    RendererErrorCode::unavailable,
                    "The requested native window surface is unsupported by this build"));
            }

            if (auto result = create_instance(); !result)
                return result;
            if (auto result = create_debug_messenger(); !result)
                return result;
            if (auto result = create_surface(); !result)
                return result;
            if (auto result = select_physical_device(); !result)
                return result;
            if (auto result = create_logical_device(); !result)
                return result;
            if (auto result = create_descriptor_layout(); !result)
                return result;
            if (auto result = create_pipeline_layout(); !result)
                return result;
            if (auto result = create_frame_resources(); !result)
                return result;
            if (auto result = create_swapchain_resources(VK_NULL_HANDLE); !result)
                return result;

            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> draw(const RenderFrame& frame)
        {
            const detail::NativeExtent native_extent = detail::query_extent(surface_source_);
            if (!native_extent.visible())
                return {};

            const VkExtent2D window_extent{ native_extent.width, native_extent.height };
            if (window_extent.width != swapchain_extent_.width ||
                window_extent.height != swapchain_extent_.height)
            {
                if (auto result = recreate_swapchain(); !result)
                    return result;
            }

            if (frame.items().size() > maximum_item_capacity_)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::rendering_failed,
                    "render item count exceeds the selected device's storage-buffer limit"));
            }

            if (frame.items().size() > item_capacity_)
            {
                if (auto result = grow_item_buffers(frame.items().size()); !result)
                    return result;
            }

            FrameResources& resources = frames_[current_frame_];
            VkResult result = vkWaitForFences(
                device_,
                1,
                &resources.in_flight,
                VK_TRUE,
                std::numeric_limits<std::uint64_t>::max());
            if (result != VK_SUCCESS)
                return std::unexpected(make_error(error_code_for(result), "vkWaitForFences", result));

            if (auto upload_result = upload_items(resources, frame.items()); !upload_result)
                return upload_result;

            std::uint32_t image_index = 0;
            result = vkAcquireNextImageKHR(
                device_,
                swapchain_,
                std::numeric_limits<std::uint64_t>::max(),
                resources.image_available,
                VK_NULL_HANDLE,
                &image_index);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
                return recreate_swapchain();

            const bool acquire_suboptimal = result == VK_SUBOPTIMAL_KHR;
            if (result != VK_SUCCESS && !acquire_suboptimal)
                return std::unexpected(make_error(error_code_for(result), "vkAcquireNextImageKHR", result));

            if (images_in_flight_.at(image_index) != VK_NULL_HANDLE)
            {
                result = vkWaitForFences(
                    device_,
                    1,
                    &images_in_flight_.at(image_index),
                    VK_TRUE,
                    std::numeric_limits<std::uint64_t>::max());
                if (result != VK_SUCCESS)
                    return std::unexpected(make_error(error_code_for(result), "waiting for swapchain image", result));
            }

            result = vkResetCommandPool(device_, resources.command_pool, 0);
            if (result != VK_SUCCESS)
                return std::unexpected(make_error(error_code_for(result), "vkResetCommandPool", result));

