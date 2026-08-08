#pragma once

#include "../compute.hpp"
#include "../export.hpp"

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace epochengine::particle::vulkan
{
    class EPOCH_PARTICLE_VULKAN_API ComputeBackend final : public IComputeBackend
    {
    public:
        [[nodiscard]] static std::expected<std::unique_ptr<ComputeBackend>, std::string>
            create();

        ~ComputeBackend() override;

        ComputeBackend(const ComputeBackend&) = delete;
        ComputeBackend& operator=(const ComputeBackend&) = delete;
        ComputeBackend(ComputeBackend&&) noexcept;
        ComputeBackend& operator=(ComputeBackend&&) noexcept;

        [[nodiscard]] ComputeStatus dispatch(
            const ComputeDispatch& request) override;
        [[nodiscard]] std::string_view name() const noexcept override;

    private:
        class Impl;

        explicit ComputeBackend(std::unique_ptr<Impl> implementation) noexcept;
        std::unique_ptr<Impl> impl_;
    };
}
