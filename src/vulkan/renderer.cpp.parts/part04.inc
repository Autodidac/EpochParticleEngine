            const auto native_extensions = detail::required_instance_extensions(surface_source_.kind);
            if (native_extensions.empty())
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "No Vulkan instance extensions are registered for the native surface"));
            }

            std::vector<const char*> extensions;
            extensions.reserve(native_extensions.size() + 2);
            for (const char* extension : native_extensions)
            {
                if (!contains_name(available_extensions, extension))
                {
                    return std::unexpected(make_error(
                        RendererErrorCode::unavailable,
                        std::string{ "Required Vulkan instance extension is unavailable: " } + extension));
                }
                append_unique(extensions, extension);
            }

            constexpr const char* validation_layer = "VK_LAYER_KHRONOS_validation";
            validation_enabled_ = config_.enable_validation &&
                contains_name(available_layers, validation_layer) &&
                contains_name(available_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            std::vector<const char*> layers;
            if (validation_enabled_)
            {
                layers.push_back(validation_layer);
                append_unique(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            VkInstanceCreateFlags flags = 0;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
            if (contains_name(available_extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
            {
                append_unique(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
#endif

            std::uint32_t loader_api_version = VK_API_VERSION_1_0;
            const auto enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
                vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
            if (enumerate_instance_version != nullptr)
                (void)enumerate_instance_version(&loader_api_version);

            const std::uint32_t requested_api_version = std::min(loader_api_version, VK_API_VERSION_1_2);
            const VkApplicationInfo application_info{
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = nullptr,
                .pApplicationName = "EpochParticleEngine Demo",
                .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
                .pEngineName = "EpochParticleEngine",
                .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
                .apiVersion = requested_api_version
            };

            VkDebugUtilsMessengerCreateInfoEXT debug_info = debug_messenger_info();
            const VkInstanceCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = validation_enabled_ ? &debug_info : nullptr,
                .flags = flags,
                .pApplicationInfo = &application_info,
                .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
                .ppEnabledLayerNames = layers.data(),
                .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
                .ppEnabledExtensionNames = extensions.data()
            };

            result = vkCreateInstance(&create_info, nullptr, &instance_);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateInstance",
                    result));
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_debug_messenger()
        {
            if (!validation_enabled_)
                return {};

            const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
            if (create == nullptr)
            {
                validation_enabled_ = false;
                return {};
            }

            const VkDebugUtilsMessengerCreateInfoEXT info = debug_messenger_info();
            const VkResult result = create(instance_, &info, nullptr, &debug_messenger_);
            if (result != VK_SUCCESS)
            {
                return std::unexpected(make_error(
                    RendererErrorCode::initialization_failed,
                    "vkCreateDebugUtilsMessengerEXT",
                    result));
            }
            return {};
        }

        [[nodiscard]] std::expected<void, RendererError> create_surface()
        {
            auto result = detail::create_surface(instance_, surface_source_);
            if (!result)
                return std::unexpected(std::move(result.error()));
            surface_ = *result;
            return {};
        }

        [[nodiscard]] QueueFamilies find_queue_families(VkPhysicalDevice device) const
        {
            std::uint32_t family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);
            std::vector<VkQueueFamilyProperties> properties(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, properties.data());

