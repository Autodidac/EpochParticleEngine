#include <epochengine/particle/epoch_particle_engine.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using namespace epochengine::particle;

    class TestFailure final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    void require(bool condition, std::string_view message)
    {
        if (!condition)
            throw TestFailure(std::string{ message });
    }

    template<class Exception, class Function>
    void require_throws(Function&& function, std::string_view message)
    {
        try
        {
            std::forward<Function>(function)();
        }
        catch (const Exception&)
        {
            return;
        }
        catch (...)
        {
            throw TestFailure(std::string{ message } + " (wrong exception type)");
        }
        throw TestFailure(std::string{ message });
    }

    class CountingScene final : public IScene
    {
    public:
        [[nodiscard]] SceneInfo info() const noexcept override
        {
            return { "counting", "Counting", "Simulation scheduler test scene" };
        }

        void reset(const SceneResetContext&) override
        {
            updates_ = 0;
        }

        void update(const SceneUpdateContext& context) override
        {
            ++updates_;
            context.events.push_back({
                .type = SimulationEventType::particle_emitted,
                .position = {},
                .intensity = 1.0F,
                .source_id = updates_
            });
        }

        void render(RenderFrame& frame, Bounds) const override
        {
            frame.circle({ 1.0F, 1.0F }, 1.0F, {});
        }

        void pointer(const PointerEvent&) override {}
        void resize(Bounds, Bounds) override {}

        [[nodiscard]] SceneStats stats() const noexcept override
        {
            return { .particle_count = updates_ };
        }

        [[nodiscard]] std::uint64_t state_hash() const noexcept override
        {
            return updates_;
        }

    private:
        std::uint64_t updates_{};
    };

    [[nodiscard]] std::string test_ticks_environment()
    {
#if defined(_WIN32)
        char* buffer = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&buffer, &length, "EPOCH_PARTICLE_TEST_TICKS") != 0
            || buffer == nullptr)
        {
            return {};
        }
        std::string result{ buffer };
        std::free(buffer);
        return result;
#else
        const char* value = std::getenv("EPOCH_PARTICLE_TEST_TICKS");
        return value != nullptr ? std::string{ value } : std::string{};
#endif
    }

    [[nodiscard]] int configured_scene_ticks() noexcept
    {
        constexpr int default_ticks = 180;
        const std::string value = test_ticks_environment();
        if (value.empty())
            return default_ticks;

        int parsed = default_ticks;
        const std::string_view text{ value };
        const auto [end, error] = std::from_chars(
            text.data(),
            text.data() + text.size(),
            parsed);
        if (error != std::errc{} || end != text.data() + text.size())
            return default_ticks;
        return std::clamp(parsed, 1, 2'000);
    }

    void test_fixed_point()
    {
        constexpr Fixed32 one = Fixed32::from_integer(1);
        constexpr Fixed32 half = Fixed32::from_ratio(1, 2);
        static_assert((one + half).raw() == 98'304);
        static_assert((half * half).raw() == 16'384);
        static_assert((one / Fixed32::from_integer(2)).raw() == half.raw());
        static_assert(Fixed32::from_ratio(-3, 2).floor_to_integer() == -2);
        static_assert(Fixed32::from_ratio(3, 2).floor_to_integer() == 1);
        static_assert((Fixed32::from_ratio(-1, 3)
            * Fixed32::from_ratio(1, 2)).raw() == -10'923);
        static_assert((Fixed32::from_integer(-3)
            / Fixed32::from_integer(2)).raw() == -98'304);

        require(
            std::abs(Fixed32::from_float(3.25F).to_float() - 3.25F)
                < 0.00002F,
            "Fixed32 float conversion failed");
        require(
            Fixed32::from_integer(
                std::numeric_limits<std::int32_t>::max()).raw()
                == std::numeric_limits<std::int32_t>::max(),
            "Fixed32 positive saturation failed");
        require(
            (-Fixed32::from_raw(
                std::numeric_limits<std::int32_t>::min())).raw()
                == std::numeric_limits<std::int32_t>::max(),
            "Fixed32 negation saturation failed");

        require(Bounds{ 100.0F, 50.0F }.valid(),
            "Finite Bounds should be valid");
        require(!Bounds{
                std::numeric_limits<float>::infinity(),
                50.0F
            }.valid(),
            "Infinite Bounds should be invalid");
        require(!Bounds{
                std::numeric_limits<float>::quiet_NaN(),
                50.0F
            }.valid(),
            "NaN Bounds should be invalid");
        require(!Bounds{ 100.0F, 50.0F }.contains({
                std::numeric_limits<float>::quiet_NaN(),
                5.0F
            }),
            "Bounds accepted a NaN point");
    }

    void test_text_metrics()
    {
        constexpr TextSize size{
            .logical_height = 14.0F,
            .dpi_scale = 1.5F
        };
        constexpr BitmapTextMetrics metrics = make_bitmap_text_metrics(
            size,
            1.0F,
            2.0F);
        static_assert(metrics.pixel_height == 21.0F);
        static_assert(metrics.cell_size == 3.0F);
        static_assert(metrics.glyph_width == 15.0F);
        static_assert(metrics.advance == 19.0F);
        static_assert(metrics.line_advance == 29.0F);

        require(
            resolved_text_pixel_height({ 16.0F, 2.0F }) == 32.0F,
            "TextSize did not resolve logical pixels through DPI scale");
        require(
            make_bitmap_text_metrics({ -1.0F, -1.0F }).pixel_height
                == default_text_logical_height,
            "Text metric defaults are invalid");

        RenderFrame frame{ 128 };
        frame.begin({ 320.0F, 200.0F });
        frame.text({ 10.0F, 20.0F }, "I", size, {}, 1);
        frame.finalize();
        require(!frame.items().empty(),
            "DPI-aware text emitted no glyph geometry");
        require(frame.items().front().half_extent.x > 1.0F,
            "DPI-aware text retained the legacy one-pixel cell size");
    }

    void test_random_repeatability()
    {
        Pcg32 first{ 42, 17 };
        Pcg32 second{ 42, 17 };
        Pcg32 different{ 43, 17 };

        bool differs = false;
        for (int index = 0; index < 2'000; ++index)
        {
            const std::uint32_t lhs = first.next_u32();
            const std::uint32_t rhs = second.next_u32();
            require(lhs == rhs, "PCG32 repeated seed diverged");
            differs = differs || lhs != different.next_u32();
        }
        require(differs,
            "PCG32 distinct seeds produced an identical sequence");

        Pcg32 bounded{ 99 };
        for (int index = 0; index < 2'000; ++index)
        {
            require(
                bounded.bounded(7) < 7,
                "PCG32 bounded result escaped its range");
        }

        Pcg32 full_range{ 123 };
        for (int index = 0; index < 2'000; ++index)
        {
            const std::int32_t value = full_range.range(
                std::numeric_limits<std::int32_t>::min(),
                std::numeric_limits<std::int32_t>::max());
            require(
                value >= std::numeric_limits<std::int32_t>::min()
                    && value < std::numeric_limits<std::int32_t>::max(),
                "PCG32 signed range overflowed");
        }
        require(full_range.range(12, 12) == 12,
            "PCG32 degenerate integer range failed");
        require(
            full_range.range(
                std::numeric_limits<float>::quiet_NaN(),
                5.0F) == 0.0F,
            "PCG32 accepted a NaN float minimum");
        require(
            full_range.range(
                3.0F,
                std::numeric_limits<float>::infinity()) == 3.0F,
            "PCG32 accepted an infinite float maximum");
    }

