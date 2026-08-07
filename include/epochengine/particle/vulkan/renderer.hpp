#pragma once

#include "../export.hpp"
#include "../render_frame.hpp"
#include "../types.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace epochengine::particle::vulkan
{
    enum class NativeSurfaceKind : std::uint8_t
    {
        none = 0,
        win32,
        xlib,
        metal
    };

    struct NativeSurface final
    {
        NativeSurfaceKind kind{ NativeSurfaceKind::none };
        void* display{};
        void* window{};
        std::uintptr_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            switch (kind)
            {
            case NativeSurfaceKind::win32:
            case NativeSurfaceKind::metal:
                return window != nullptr;
            case NativeSurfaceKind::xlib:
                return display != nullptr && value != 0;
            case NativeSurfaceKind::none:
            default:
                return false;
            }
        }
    };

    enum class RendererErrorCode
    {
        invalid_window,
        unavailable,
        initialization_failed,
        shader_compilation_failed,
        device_lost,
        rendering_failed
    };

    struct RendererError
    {
        RendererErrorCode code{ RendererErrorCode::rendering_failed };
        std::string message;
    };

    struct RendererConfig
    {
        bool enable_validation{ true };
        bool vertical_sync{ true };
        std::size_t initial_item_capacity{ 262'144 };
        Color clear_color{ 0.018F, 0.024F, 0.038F, 1.0F };
    };

    class EPOCH_PARTICLE_VULKAN_API Renderer
    {
    public:
        [[nodiscard]] static std::expected<std::unique_ptr<Renderer>, RendererError> create(
            NativeSurface surface,
            RendererConfig config = {});

        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) noexcept;
        Renderer& operator=(Renderer&&) noexcept;

        [[nodiscard]] std::expected<void, RendererError> draw(const RenderFrame& frame);
        void wait_idle() noexcept;

        [[nodiscard]] const std::string& device_name() const noexcept;
        [[nodiscard]] std::size_t item_capacity() const noexcept;
        [[nodiscard]] std::size_t maximum_item_capacity() const noexcept;
        [[nodiscard]] bool validation_enabled() const noexcept;
        [[nodiscard]] bool vertical_sync() const noexcept;
        [[nodiscard]] Bounds drawable_bounds() const noexcept;

    private:
        class Impl;

        explicit Renderer(std::unique_ptr<Impl> implementation) noexcept;
        std::unique_ptr<Impl> impl_;
    };
}
