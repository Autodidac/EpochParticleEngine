#pragma once

#include "export.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace epochengine::particle
{
    enum class PrimitiveShape : std::uint8_t
    {
        circle,
        rectangle,
        rounded_rectangle
    };

    struct RenderItem
    {
        Vec2 center{};
        Vec2 half_extent{ 1.0F, 1.0F };
        Color color{};
        float rotation{};
        PrimitiveShape shape{ PrimitiveShape::circle };
        float corner_radius{};
        std::int32_t layer{};
    };

    enum class TextAlign : std::uint8_t
    {
        left,
        center,
        right
    };

    class EPOCH_PARTICLE_API RenderFrame
    {
    public:
        explicit RenderFrame(std::size_t maximum_items = 262'144);

        void begin(Bounds extent);
        void reserve(std::size_t item_count);

        void add(RenderItem item);
        void circle(Vec2 center, float radius, Color color, std::int32_t layer = 0);
        void rectangle(
            Vec2 center,
            Vec2 half_extent,
            Color color,
            std::int32_t layer = 0,
            float rotation = 0.0F);
        void rounded_rectangle(
            Vec2 center,
            Vec2 half_extent,
            float radius,
            Color color,
            std::int32_t layer = 0);
        void line(
            Vec2 from,
            Vec2 to,
            float thickness,
            Color color,
            std::int32_t layer = 0);
        void text(
            Vec2 position,
            std::string_view value,
            float scale,
            Color color,
            std::int32_t layer = 100,
            TextAlign alignment = TextAlign::left);

        void finalize();

        [[nodiscard]] std::span<const RenderItem> items() const noexcept;
        [[nodiscard]] Bounds extent() const noexcept;
        [[nodiscard]] std::size_t dropped_items() const noexcept;
        [[nodiscard]] std::size_t maximum_items() const noexcept;

    private:
        [[nodiscard]] float text_width(std::string_view value, float scale) const noexcept;

        Bounds extent_{};
        std::vector<RenderItem> items_;
        std::size_t maximum_items_{};
        std::size_t dropped_items_{};
        bool finalized_{};
    };
}
