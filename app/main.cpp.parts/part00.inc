#include "../ui_overlay.hpp"

#if defined(__APPLE__)
#include "../cocoa_input_bridge.hpp"
#endif

#include <epochengine/particle/epoch_particle_engine.hpp>
#include <epochengine/particle/vulkan/renderer.hpp>
#include <epochplatformengine/epoch_platform_engine.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <expected>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace epochengine::particle::demo
{
    namespace platform = epochengine::platform;

    struct LaunchOptions final
    {
        int width{ 1600 };
        int height{ 900 };
        std::uint64_t seed{ 0x45504f4348504152ULL };
        std::size_t workers{ TaskArena::recommended_worker_count() };
        std::size_t scene{};
        bool validation{
#ifndef NDEBUG
            true
#else
            false
#endif
        };
        bool vertical_sync{ true };
        bool show_help{};
    };

    namespace
    {
        template<class Integer>
        [[nodiscard]] bool parse_integer(std::string_view text, Integer& value) noexcept
        {
            const char* begin = text.data();
            const char* end = text.data() + text.size();
            const auto [parsed_end, error] = std::from_chars(begin, end, value);
            return error == std::errc{} && parsed_end == end;
        }

        [[nodiscard]] std::optional<LaunchOptions> parse_arguments(
            int argc,
            const char* const* argv)
        {
            LaunchOptions options;
            for (int index = 1; index < argc; ++index)
            {
                const std::string_view argument = argv[index] != nullptr ? argv[index] : "";
                auto next_value = [&]() -> std::optional<std::string_view>
                {
                    if (index + 1 >= argc || argv[index + 1] == nullptr)
                        return std::nullopt;
                    return std::string_view{ argv[++index] };
                };

                if (argument == "--help" || argument == "-h")
                {
                    options.show_help = true;
                }
                else if (argument == "--width")
                {
                    const auto value = next_value();
                    if (!value || !parse_integer(*value, options.width) || options.width < 640)
                        return std::nullopt;
                }
                else if (argument == "--height")
                {
                    const auto value = next_value();
                    if (!value || !parse_integer(*value, options.height) || options.height < 480)
                        return std::nullopt;
                }
                else if (argument == "--seed")
                {
                    const auto value = next_value();
                    if (!value || !parse_integer(*value, options.seed))
                        return std::nullopt;
                }
                else if (argument == "--workers")
                {
                    const auto value = next_value();
                    if (!value || !parse_integer(*value, options.workers) || options.workers > 256)
                        return std::nullopt;
                }
                else if (argument == "--scene")
                {
                    const auto value = next_value();
                    std::size_t one_based_scene = 0;
                    if (!value || !parse_integer(*value, one_based_scene) || one_based_scene == 0)
                        return std::nullopt;
                    options.scene = one_based_scene - 1;
                }
                else if (argument == "--validation")
                {
                    options.validation = true;
                }
                else if (argument == "--no-validation")
                {
                    options.validation = false;
                }
                else if (argument == "--vsync")
                {
                    options.vertical_sync = true;
                }
                else if (argument == "--no-vsync")
                {
                    options.vertical_sync = false;
                }
                else
                {
                    std::cerr << "Unknown argument: " << argument << '\n';
                    return std::nullopt;
                }
            }
            return options;
        }

        void print_usage(std::string_view executable)
        {
            std::cout
                << "EpochParticleEngine Vulkan Particle Studio\n\n"
                << "Usage: " << executable << " [options]\n\n"
                << "  --width N           Initial window width (minimum 640)\n"
                << "  --height N          Initial window height (minimum 480)\n"
                << "  --seed N            Deterministic 64-bit simulation seed\n"
                << "  --workers N         Background worker threads; 0 is single-threaded\n"
                << "  --scene N           Start on scene 1 through 15\n"
                << "  --validation        Request Vulkan validation\n"
                << "  --no-validation     Disable Vulkan validation\n"
                << "  --vsync             Use FIFO presentation\n"
                << "  --no-vsync          Prefer mailbox/immediate presentation\n"
                << "  -h, --help          Show this help\n\n"
                << "Controls: 1-9 scene shortcut, Tab next, Shift+Tab previous, Space pause,\n"
                << "R reset, . single step, -/+ speed, F1 panel, Ctrl+S save editor scene,\n"
                << "Ctrl+L load editor scene, Esc exit. Scenes 10-15 are clickable in the panel.\n";
        }

