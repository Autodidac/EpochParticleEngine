#pragma once

#include "export.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace epochengine::particle
{
    struct ComputeDispatch
    {
        std::string_view program_id;
        std::string_view shader_source;
        std::span<std::byte> storage;
        std::span<const std::byte> push_constants;
        std::uint32_t workgroup_count_x{ 1 };
        std::uint32_t workgroup_count_y{ 1 };
        std::uint32_t workgroup_count_z{ 1 };
    };

    struct ComputeStatus
    {
        std::string message;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return message.empty();
        }

        [[nodiscard]] const std::string& error() const noexcept { return message; }
    };
    class EPOCH_PARTICLE_API IComputeBackend
    {
    public:
        virtual ~IComputeBackend() = default;

        [[nodiscard]] virtual ComputeStatus dispatch(
            const ComputeDispatch& request) = 0;
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    };
}
