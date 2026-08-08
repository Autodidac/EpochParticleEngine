#include <epochengine/particle/vulkan/compute_backend.hpp>

#include <shaderc/shaderc.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace epochengine::particle::vulkan
{
    namespace
    {
        [[nodiscard]] std::string failure(std::string_view operation, VkResult result)
        {
            return std::string{ operation } + " failed with VkResult "
                + std::to_string(static_cast<std::int32_t>(result));
        }

        [[nodiscard]] std::expected<std::vector<std::uint32_t>, std::string> compile_compute(
            std::string_view source,
            std::string_view name)
        {
            shaderc_compiler_t compiler = shaderc_compiler_initialize();
            shaderc_compile_options_t options = shaderc_compile_options_initialize();
            if (compiler == nullptr || options == nullptr)
            {
                if (options != nullptr)
                    shaderc_compile_options_release(options);
                if (compiler != nullptr)
                    shaderc_compiler_release(compiler);
                return std::unexpected("Unable to initialize shaderc for compute");
            }

            shaderc_compile_options_set_source_language(
                options, shaderc_source_language_glsl);
            shaderc_compile_options_set_target_env(
                options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
            shaderc_compile_options_set_target_spirv(
                options, shaderc_spirv_version_1_5);
            shaderc_compile_options_set_optimization_level(
                options, shaderc_optimization_level_performance);

            const std::string owned_name{ name };
            shaderc_compilation_result_t result = shaderc_compile_into_spv(
                compiler,
                source.data(),
                source.size(),
                shaderc_compute_shader,
                owned_name.c_str(),
                "main",
                options);
            shaderc_compile_options_release(options);
            shaderc_compiler_release(compiler);
            if (result == nullptr)
                return std::unexpected("shaderc returned no compute result");

            if (shaderc_result_get_compilation_status(result)
                != shaderc_compilation_status_success)
            {
                const char* message = shaderc_result_get_error_message(result);
                std::string error = owned_name + ": "
                    + (message != nullptr ? message : "compute compilation failed");
                shaderc_result_release(result);
                return std::unexpected(std::move(error));
            }

            const std::size_t bytes = shaderc_result_get_length(result);
            if (bytes == 0 || (bytes % sizeof(std::uint32_t)) != 0)
            {
                shaderc_result_release(result);
                return std::unexpected(owned_name + ": malformed compute SPIR-V");
            }
            std::vector<std::uint32_t> words(bytes / sizeof(std::uint32_t));
            std::memcpy(words.data(), shaderc_result_get_bytes(result), bytes);
            shaderc_result_release(result);
            return words;
        }
    }

    class ComputeBackend::Impl
    {
    public:
        ~Impl()
        {
            destroy();
        }

        [[nodiscard]] std::expected<void, std::string> initialize()
        {
            const VkApplicationInfo application{
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = nullptr,
                .pApplicationName = "EpochParticleCompute",
                .applicationVersion = VK_MAKE_API_VERSION(0, 0, 5, 0),
                .pEngineName = "EpochParticleEngine",
                .engineVersion = VK_MAKE_API_VERSION(0, 0, 5, 0),
                .apiVersion = VK_API_VERSION_1_2
            };
            std::vector<const char*> instance_extensions;
            VkInstanceCreateFlags instance_flags = 0;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
            std::uint32_t instance_extension_count = 0;
            (void)vkEnumerateInstanceExtensionProperties(
                nullptr, &instance_extension_count, nullptr);
            std::vector<VkExtensionProperties> instance_properties(
                instance_extension_count);
            (void)vkEnumerateInstanceExtensionProperties(
                nullptr,
                &instance_extension_count,
                instance_properties.data());
            if (std::ranges::any_of(
                    instance_properties,
                    [](const VkExtensionProperties& property)
                    {
                        return std::strcmp(
                            property.extensionName,
                            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0;
                    }))
            {
                instance_extensions.push_back(
                    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                instance_flags |=
                    VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
#endif
            const VkInstanceCreateInfo instance_info{
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = instance_flags,
                .pApplicationInfo = &application,
                .enabledLayerCount = 0,
                .ppEnabledLayerNames = nullptr,
                .enabledExtensionCount =
                    static_cast<std::uint32_t>(instance_extensions.size()),
                .ppEnabledExtensionNames = instance_extensions.data()
            };
            VkResult result = vkCreateInstance(
                &instance_info, nullptr, &instance_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateInstance(compute)", result));

            std::uint32_t device_count = 0;
            result = vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
            if (result != VK_SUCCESS || device_count == 0)
                return std::unexpected("No Vulkan compute device is available");
            std::vector<VkPhysicalDevice> devices(device_count);
            result = vkEnumeratePhysicalDevices(
                instance_, &device_count, devices.data());
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkEnumeratePhysicalDevices", result));

            std::uint32_t best_score = 0;
            for (VkPhysicalDevice candidate : devices)
            {
                std::uint32_t family_count = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(
                    candidate, &family_count, nullptr);
                std::vector<VkQueueFamilyProperties> families(family_count);
                vkGetPhysicalDeviceQueueFamilyProperties(
                    candidate, &family_count, families.data());
                for (std::uint32_t index = 0; index < family_count; ++index)
                {
                    if ((families[index].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
                        continue;
                    VkPhysicalDeviceProperties properties{};
                    vkGetPhysicalDeviceProperties(candidate, &properties);
                    const std::uint32_t score =
                        properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                            ? 3U : properties.deviceType
                                == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2U : 1U;
                    if (score > best_score)
                    {
                        best_score = score;
                        physical_device_ = candidate;
                        queue_family_ = index;
                        properties_ = properties;
                    }
                }
            }
            if (physical_device_ == VK_NULL_HANDLE)
                return std::unexpected("No Vulkan queue supports compute");

            constexpr float priority = 1.0F;
            const VkDeviceQueueCreateInfo queue_info{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = queue_family_,
                .queueCount = 1,
                .pQueuePriorities = &priority
            };
            std::vector<const char*> device_extensions;
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            std::uint32_t device_extension_count = 0;
            (void)vkEnumerateDeviceExtensionProperties(
                physical_device_, nullptr, &device_extension_count, nullptr);
            std::vector<VkExtensionProperties> device_properties(
                device_extension_count);
            (void)vkEnumerateDeviceExtensionProperties(
                physical_device_,
                nullptr,
                &device_extension_count,
                device_properties.data());
            if (std::ranges::any_of(
                    device_properties,
                    [](const VkExtensionProperties& property)
                    {
                        return std::strcmp(
                            property.extensionName,
                            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0;
                    }))
            {
                device_extensions.push_back(
                    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            }
#endif
            const VkPhysicalDeviceFeatures features{};
            const VkDeviceCreateInfo device_info{
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &queue_info,
                .enabledLayerCount = 0,
                .ppEnabledLayerNames = nullptr,
                .enabledExtensionCount =
                    static_cast<std::uint32_t>(device_extensions.size()),
                .ppEnabledExtensionNames = device_extensions.data(),
                .pEnabledFeatures = &features
            };
            result = vkCreateDevice(
                physical_device_, &device_info, nullptr, &device_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateDevice(compute)", result));
            vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
            name_ = properties_.deviceName;

            const VkDescriptorSetLayoutBinding binding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .pImmutableSamplers = nullptr
            };
            const VkDescriptorSetLayoutCreateInfo layout_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = 1,
                .pBindings = &binding
            };
            result = vkCreateDescriptorSetLayout(
                device_, &layout_info, nullptr, &descriptor_layout_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateDescriptorSetLayout(compute)", result));

            const VkPushConstantRange push_range{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 128
            };
            const VkPipelineLayoutCreateInfo pipeline_layout_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = 1,
                .pSetLayouts = &descriptor_layout_,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_range
            };
            result = vkCreatePipelineLayout(
                device_, &pipeline_layout_info, nullptr, &pipeline_layout_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreatePipelineLayout(compute)", result));

            const VkDescriptorPoolSize pool_size{
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1
            };
            const VkDescriptorPoolCreateInfo pool_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &pool_size
            };
            result = vkCreateDescriptorPool(
                device_, &pool_info, nullptr, &descriptor_pool_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateDescriptorPool(compute)", result));

            const VkDescriptorSetAllocateInfo set_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = descriptor_pool_,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptor_layout_
            };
            result = vkAllocateDescriptorSets(device_, &set_info, &descriptor_set_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkAllocateDescriptorSets(compute)", result));

            const VkCommandPoolCreateInfo command_pool_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queue_family_
            };
            result = vkCreateCommandPool(
                device_, &command_pool_info, nullptr, &command_pool_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateCommandPool(compute)", result));

            const VkCommandBufferAllocateInfo command_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = command_pool_,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };
            result = vkAllocateCommandBuffers(
                device_, &command_info, &command_buffer_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkAllocateCommandBuffers(compute)", result));

            const VkFenceCreateInfo fence_info{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };
            result = vkCreateFence(device_, &fence_info, nullptr, &fence_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateFence(compute)", result));
            return {};
        }

        [[nodiscard]] std::expected<void, std::string> dispatch(
            const ComputeDispatch& request)
        {
            if (request.program_id.empty() || request.shader_source.empty())
                return std::unexpected("Compute dispatch requires a program id and shader");
            if (request.storage.empty())
                return std::unexpected("Compute dispatch requires non-empty storage");
            if (request.push_constants.size() > 128)
                return std::unexpected("Compute push constants exceed Vulkan's 128-byte floor");
            if (request.workgroup_count_x == 0 || request.workgroup_count_y == 0
                || request.workgroup_count_z == 0)
            {
                return std::unexpected("Compute dispatch workgroup counts must be positive");
            }
            if (request.storage.size() > properties_.limits.maxStorageBufferRange)
                return std::unexpected("Compute storage exceeds maxStorageBufferRange");

            if (auto ensured = ensure_buffer(request.storage.size()); !ensured)
                return ensured;
            std::memcpy(mapped_, request.storage.data(), request.storage.size());
            if (!host_coherent_)
            {
                const VkMappedMemoryRange range{
                    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                    .pNext = nullptr,
                    .memory = memory_,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE
                };
                const VkResult flushed = vkFlushMappedMemoryRanges(device_, 1, &range);
                if (flushed != VK_SUCCESS)
                    return std::unexpected(failure("vkFlushMappedMemoryRanges(compute)", flushed));
            }

            auto pipeline = pipeline_for(request.program_id, request.shader_source);
            if (!pipeline)
                return std::unexpected(std::move(pipeline.error()));

            VkResult result = vkResetFences(device_, 1, &fence_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkResetFences(compute)", result));
            result = vkResetCommandPool(device_, command_pool_, 0);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkResetCommandPool(compute)", result));

            const VkCommandBufferBeginInfo begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr
            };
            result = vkBeginCommandBuffer(command_buffer_, &begin_info);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkBeginCommandBuffer(compute)", result));
            vkCmdBindPipeline(
                command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
            vkCmdBindDescriptorSets(
                command_buffer_,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline_layout_,
                0,
                1,
                &descriptor_set_,
                0,
                nullptr);
            if (!request.push_constants.empty())
            {
                vkCmdPushConstants(
                    command_buffer_,
                    pipeline_layout_,
                    VK_SHADER_STAGE_COMPUTE_BIT,
                    0,
                    static_cast<std::uint32_t>(request.push_constants.size()),
                    request.push_constants.data());
            }
            vkCmdDispatch(
                command_buffer_,
                request.workgroup_count_x,
                request.workgroup_count_y,
                request.workgroup_count_z);
            result = vkEndCommandBuffer(command_buffer_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkEndCommandBuffer(compute)", result));

            const VkSubmitInfo submit{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = nullptr,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = nullptr,
                .pWaitDstStageMask = nullptr,
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffer_,
                .signalSemaphoreCount = 0,
                .pSignalSemaphores = nullptr
            };
            result = vkQueueSubmit(queue_, 1, &submit, fence_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkQueueSubmit(compute)", result));
            result = vkWaitForFences(
                device_, 1, &fence_, VK_TRUE,
                std::numeric_limits<std::uint64_t>::max());
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkWaitForFences(compute)", result));

            if (!host_coherent_)
            {
                const VkMappedMemoryRange range{
                    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                    .pNext = nullptr,
                    .memory = memory_,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE
                };
                result = vkInvalidateMappedMemoryRanges(device_, 1, &range);
                if (result != VK_SUCCESS)
                    return std::unexpected(failure("vkInvalidateMappedMemoryRanges(compute)", result));
            }
            std::memcpy(
                request.storage.data(), mapped_, request.storage.size());
            return {};
        }

        [[nodiscard]] std::string_view name() const noexcept
        {
            return name_;
        }

    private:
        [[nodiscard]] std::optional<std::uint32_t> memory_type(
            std::uint32_t allowed,
            VkMemoryPropertyFlags required,
            VkMemoryPropertyFlags preferred) const noexcept
        {
            VkPhysicalDeviceMemoryProperties properties{};
            vkGetPhysicalDeviceMemoryProperties(physical_device_, &properties);
            std::optional<std::uint32_t> fallback;
            for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index)
            {
                if ((allowed & (1U << index)) == 0)
                    continue;
                const VkMemoryPropertyFlags flags =
                    properties.memoryTypes[index].propertyFlags;
                if ((flags & required) != required)
                    continue;
                if ((flags & preferred) == preferred)
                    return index;
                if (!fallback.has_value())
                    fallback = index;
            }
            return fallback;
        }

        void destroy_buffer() noexcept
        {
            if (mapped_ != nullptr && memory_ != VK_NULL_HANDLE)
                vkUnmapMemory(device_, memory_);
            mapped_ = nullptr;
            if (buffer_ != VK_NULL_HANDLE)
                vkDestroyBuffer(device_, buffer_, nullptr);
            if (memory_ != VK_NULL_HANDLE)
                vkFreeMemory(device_, memory_, nullptr);
            buffer_ = VK_NULL_HANDLE;
            memory_ = VK_NULL_HANDLE;
            buffer_size_ = 0;
            allocation_size_ = 0;
            host_coherent_ = false;
        }

        [[nodiscard]] std::expected<void, std::string> ensure_buffer(
            std::size_t required)
        {
            if (required <= buffer_size_)
                return {};
            if (buffer_ != VK_NULL_HANDLE)
                (void)vkDeviceWaitIdle(device_);
            destroy_buffer();

            const std::size_t capacity = std::bit_ceil(required);
            const VkBufferCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .size = static_cast<VkDeviceSize>(capacity),
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr
            };
            VkResult result = vkCreateBuffer(device_, &info, nullptr, &buffer_);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateBuffer(compute)", result));
            buffer_size_ = static_cast<VkDeviceSize>(capacity);

            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device_, buffer_, &requirements);
            const auto type = memory_type(
                requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (!type.has_value())
            {
                destroy_buffer();
                return std::unexpected("No host-visible Vulkan compute memory type");
            }
            VkPhysicalDeviceMemoryProperties memory_properties{};
            vkGetPhysicalDeviceMemoryProperties(
                physical_device_, &memory_properties);
            host_coherent_ = (memory_properties.memoryTypes[*type].propertyFlags
                & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

            const VkMemoryAllocateInfo allocation{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = nullptr,
                .allocationSize = requirements.size,
                .memoryTypeIndex = *type
            };
            result = vkAllocateMemory(device_, &allocation, nullptr, &memory_);
            if (result != VK_SUCCESS)
            {
                destroy_buffer();
                return std::unexpected(failure("vkAllocateMemory(compute)", result));
            }
            allocation_size_ = requirements.size;
            result = vkBindBufferMemory(device_, buffer_, memory_, 0);
            if (result != VK_SUCCESS)
            {
                destroy_buffer();
                return std::unexpected(failure("vkBindBufferMemory(compute)", result));
            }
            result = vkMapMemory(
                device_, memory_, 0, allocation_size_, 0, &mapped_);
            if (result != VK_SUCCESS)
            {
                destroy_buffer();
                return std::unexpected(failure("vkMapMemory(compute)", result));
            }

            const VkDescriptorBufferInfo buffer_info{
                .buffer = buffer_,
                .offset = 0,
                .range = buffer_size_
            };
            const VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptor_set_,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &buffer_info,
                .pTexelBufferView = nullptr
            };
            vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
            return {};
        }

        [[nodiscard]] std::expected<VkPipeline, std::string> pipeline_for(
            std::string_view program_id,
            std::string_view shader)
        {
            const std::string key{ program_id };
            if (const auto found = pipelines_.find(key); found != pipelines_.end())
                return found->second;

            auto words = compile_compute(shader, program_id);
            if (!words)
                return std::unexpected(std::move(words.error()));
            const VkShaderModuleCreateInfo module_info{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = words->size() * sizeof(std::uint32_t),
                .pCode = words->data()
            };
            VkShaderModule module = VK_NULL_HANDLE;
            VkResult result = vkCreateShaderModule(
                device_, &module_info, nullptr, &module);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateShaderModule(compute)", result));

            const VkPipelineShaderStageCreateInfo stage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = module,
                .pName = "main",
                .pSpecializationInfo = nullptr
            };
            const VkComputePipelineCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = stage,
                .layout = pipeline_layout_,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1
            };
            VkPipeline pipeline = VK_NULL_HANDLE;
            result = vkCreateComputePipelines(
                device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
            vkDestroyShaderModule(device_, module, nullptr);
            if (result != VK_SUCCESS)
                return std::unexpected(failure("vkCreateComputePipelines", result));
            pipelines_.emplace(key, pipeline);
            return pipeline;
        }

        void destroy() noexcept
        {
            if (device_ != VK_NULL_HANDLE)
                (void)vkDeviceWaitIdle(device_);
            if (device_ != VK_NULL_HANDLE)
            {
                destroy_buffer();
                for (const auto& [key, pipeline] : pipelines_)
                {
                    static_cast<void>(key);
                    vkDestroyPipeline(device_, pipeline, nullptr);
                }
                pipelines_.clear();
                if (fence_ != VK_NULL_HANDLE)
                    vkDestroyFence(device_, fence_, nullptr);
                if (command_pool_ != VK_NULL_HANDLE)
                    vkDestroyCommandPool(device_, command_pool_, nullptr);
                if (descriptor_pool_ != VK_NULL_HANDLE)
                    vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
                if (pipeline_layout_ != VK_NULL_HANDLE)
                    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
                if (descriptor_layout_ != VK_NULL_HANDLE)
                    vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
                vkDestroyDevice(device_, nullptr);
                device_ = VK_NULL_HANDLE;
            }
            if (instance_ != VK_NULL_HANDLE)
            {
                vkDestroyInstance(instance_, nullptr);
                instance_ = VK_NULL_HANDLE;
            }
        }

        VkInstance instance_{ VK_NULL_HANDLE };
        VkPhysicalDevice physical_device_{ VK_NULL_HANDLE };
        VkPhysicalDeviceProperties properties_{};
        VkDevice device_{ VK_NULL_HANDLE };
        std::uint32_t queue_family_{};
        VkQueue queue_{ VK_NULL_HANDLE };
        std::string name_{ "Unavailable" };

        VkDescriptorSetLayout descriptor_layout_{ VK_NULL_HANDLE };
        VkPipelineLayout pipeline_layout_{ VK_NULL_HANDLE };
        VkDescriptorPool descriptor_pool_{ VK_NULL_HANDLE };
        VkDescriptorSet descriptor_set_{ VK_NULL_HANDLE };
        VkCommandPool command_pool_{ VK_NULL_HANDLE };
        VkCommandBuffer command_buffer_{ VK_NULL_HANDLE };
        VkFence fence_{ VK_NULL_HANDLE };
        VkBuffer buffer_{ VK_NULL_HANDLE };
        VkDeviceMemory memory_{ VK_NULL_HANDLE };
        void* mapped_{};
        VkDeviceSize buffer_size_{};
        VkDeviceSize allocation_size_{};
        bool host_coherent_{};
        std::unordered_map<std::string, VkPipeline> pipelines_;
    };

    ComputeBackend::ComputeBackend(std::unique_ptr<Impl> implementation) noexcept
        : impl_(std::move(implementation))
    {
    }

    ComputeBackend::~ComputeBackend() = default;
    ComputeBackend::ComputeBackend(ComputeBackend&&) noexcept = default;
    ComputeBackend& ComputeBackend::operator=(ComputeBackend&&) noexcept = default;

    std::expected<std::unique_ptr<ComputeBackend>, std::string>
        ComputeBackend::create()
    {
        auto implementation = std::make_unique<Impl>();
        if (auto initialized = implementation->initialize(); !initialized)
            return std::unexpected(std::move(initialized.error()));
        return std::unique_ptr<ComputeBackend>{
            new ComputeBackend{ std::move(implementation) }
        };
    }

    ComputeStatus ComputeBackend::dispatch(
        const ComputeDispatch& request)
    {
        if (!impl_)
            return { "Vulkan compute backend is moved from" };
        auto result = impl_->dispatch(request);
        return result ? ComputeStatus{}
                      : ComputeStatus{ std::move(result.error()) };
    }

    std::string_view ComputeBackend::name() const noexcept
    {
        return impl_ ? impl_->name() : std::string_view{ "Unavailable" };
    }
}
