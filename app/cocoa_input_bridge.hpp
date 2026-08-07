#pragma once

#if defined(__APPLE__)

#include <cstdint>

namespace epochengine::particle::demo
{
    enum class CocoaInputType : std::uint8_t
    {
        key_pressed,
        key_released,
        pointer_moved,
        pointer_pressed,
        pointer_released
    };

    struct CocoaInputEvent final
    {
        CocoaInputType type{ CocoaInputType::pointer_moved };
        std::int32_t key{};
        char32_t character{};
        std::int32_t button{};
        float x{};
        float y{};
        bool repeated{};
        bool shift{};
        bool control{};
    };

    using CocoaInputCallback = void (*)(void*, const CocoaInputEvent&) noexcept;

    [[nodiscard]] void* install_cocoa_input_bridge(
        void* native_window,
        void* user,
        CocoaInputCallback callback) noexcept;

    void uninstall_cocoa_input_bridge(void* token) noexcept;
}

#endif
