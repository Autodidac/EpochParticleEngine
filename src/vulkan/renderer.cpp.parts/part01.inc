        struct QueueFamilies
        {
            std::optional<std::uint32_t> graphics;
            std::optional<std::uint32_t> present;

            [[nodiscard]] bool complete() const noexcept
            {
                return graphics.has_value() && present.has_value();
            }
        };

        struct SwapchainSupport
        {
            VkSurfaceCapabilitiesKHR capabilities{};
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> present_modes;

            [[nodiscard]] bool adequate() const noexcept
            {
                return !formats.empty() && !present_modes.empty();
            }
        };

        struct alignas(16) GpuRenderItem
        {
            float center[2]{};
            float half_extent[2]{};
            float color[4]{};
            float rotation{};
            float shape{};
            float corner_radius{};
            float padding{};
        };

        static_assert(sizeof(GpuRenderItem) == 48);
        static_assert(alignof(GpuRenderItem) == 16);
        static_assert(offsetof(GpuRenderItem, center) == 0);
        static_assert(offsetof(GpuRenderItem, half_extent) == 8);
        static_assert(offsetof(GpuRenderItem, color) == 16);
        static_assert(offsetof(GpuRenderItem, rotation) == 32);
        static_assert(offsetof(GpuRenderItem, shape) == 36);
        static_assert(offsetof(GpuRenderItem, corner_radius) == 40);

        struct PushConstants
        {
            float viewport[2]{};
        };

        [[nodiscard]] GpuRenderItem to_gpu_item(const RenderItem& item) noexcept
        {
            return {
                .center = { item.center.x, item.center.y },
                .half_extent = { item.half_extent.x, item.half_extent.y },
                .color = { item.color.r, item.color.g, item.color.b, item.color.a },
                .rotation = item.rotation,
                .shape = static_cast<float>(std::to_underlying(item.shape)),
                .corner_radius = item.corner_radius,
                .padding = 0.0F
            };
        }

        [[nodiscard]] std::size_t normalized_capacity(
            std::size_t requested,
            std::size_t maximum) noexcept
        {
            if (maximum == 0)
                return 0;

            constexpr std::size_t preferred_minimum = 1'024;
            const std::size_t minimum = std::min(preferred_minimum, maximum);
            requested = std::clamp(requested, minimum, maximum);
            if (std::has_single_bit(requested))
                return requested;

            const auto maximum_power_of_two = std::size_t{ 1 }
                << (std::numeric_limits<std::size_t>::digits - 1);
            if (requested > maximum_power_of_two)
                return requested;

            const std::size_t rounded = std::bit_ceil(requested);
            return std::min(rounded, maximum);
        }

        [[nodiscard]] std::expected<std::vector<std::uint32_t>, RendererError> compile_shader(
            std::string_view source,
            shaderc_shader_kind kind,
            const char* name)
        {
            shaderc_compiler_t compiler = shaderc_compiler_initialize();
            if (compiler == nullptr)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::shader_compilation_failed,
                    "shaderc compiler initialization failed"));
            }

            shaderc_compile_options_t options = shaderc_compile_options_initialize();
            if (options == nullptr)
            {
                shaderc_compiler_release(compiler);
                return std::unexpected(make_error(
                    RendererErrorCode::shader_compilation_failed,
                    "shaderc options initialization failed"));
            }

            shaderc_compile_options_set_source_language(options, shaderc_source_language_glsl);
            shaderc_compile_options_set_target_env(
                options,
                shaderc_target_env_vulkan,
                shaderc_env_version_vulkan_1_2);
            shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_5);
            shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);

            shaderc_compilation_result_t result = shaderc_compile_into_spv(
                compiler,
                source.data(),
                source.size(),
                kind,
                name,
                "main",
                options);

            shaderc_compile_options_release(options);
            shaderc_compiler_release(compiler);

            if (result == nullptr)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::shader_compilation_failed,
                    std::string{ name } + ": shaderc returned no result"));
            }

            const shaderc_compilation_status status = shaderc_result_get_compilation_status(result);
            if (status != shaderc_compilation_status_success)
            {
                const char* error_message = shaderc_result_get_error_message(result);
                std::string message = std::string{ name } + ": ";
                message.append(error_message != nullptr ? error_message : "shader compilation failed");
                shaderc_result_release(result);
                return std::unexpected(make_error(
                    RendererErrorCode::shader_compilation_failed,
                    std::move(message)));
            }

