#pragma once

#include <string_view>

namespace epochengine::particle::vulkan::detail
{
    inline constexpr std::string_view vertex_shader = R"glsl(
#version 450

struct RenderItem
{
    vec2 center;
    vec2 half_extent;
    vec4 color;
    float rotation;
    float shape;
    uint payload_low;
    uint payload_high;
};

layout(std430, set = 0, binding = 0) readonly buffer RenderItems
{
    RenderItem items[];
};

layout(push_constant) uniform PushConstants
{
    vec2 viewport;
} push_constants;

layout(location = 0) out vec4 fragment_color;
layout(location = 1) out vec2 fragment_local;
layout(location = 2) flat out float fragment_shape;
layout(location = 3) flat out float fragment_corner_radius;
layout(location = 4) flat out vec2 fragment_half_extent;
layout(location = 5) flat out uvec2 fragment_glyph_bitmap;

const vec2 corners[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

void main()
{
    RenderItem item = items[gl_InstanceIndex];
    vec2 local = corners[gl_VertexIndex];

    float cosine = cos(item.rotation);
    float sine = sin(item.rotation);
    mat2 rotation = mat2(cosine, sine, -sine, cosine);
    vec2 pixel = item.center + rotation * (local * item.half_extent);

    // RenderFrame uses upper-left-origin framebuffer pixels with positive Y
    // downward. A positive-height Vulkan viewport maps NDC -1 to the top edge,
    // so Y uses the same conversion as X and must not be inverted here.
    vec2 safe_viewport = max(push_constants.viewport, vec2(1.0));
    vec2 normalized = vec2(
        pixel.x / safe_viewport.x * 2.0 - 1.0,
        pixel.y / safe_viewport.y * 2.0 - 1.0);

    gl_Position = vec4(normalized, 0.0, 1.0);
    fragment_color = item.color;
    fragment_local = local;
    fragment_shape = item.shape;
    fragment_corner_radius = uintBitsToFloat(item.payload_low);
    fragment_half_extent = item.half_extent;
    fragment_glyph_bitmap = uvec2(item.payload_low, item.payload_high);
}
)glsl";

    static_assert(vertex_shader.find("pixel.y / safe_viewport.y * 2.0 - 1.0")
        != std::string_view::npos);
    static_assert(vertex_shader.find("1.0 - pixel.y / safe_viewport.y * 2.0")
        == std::string_view::npos);

    inline constexpr std::string_view fragment_shader = R"glsl(
#version 450

layout(location = 0) in vec4 fragment_color;
layout(location = 1) in vec2 fragment_local;
layout(location = 2) flat in float fragment_shape;
layout(location = 3) flat in float fragment_corner_radius;
layout(location = 4) flat in vec2 fragment_half_extent;
layout(location = 5) flat in uvec2 fragment_glyph_bitmap;

layout(location = 0) out vec4 output_color;

void main()
{
    float coverage = 1.0;

    if (fragment_shape > 2.5)
    {
        vec2 glyph_uv = clamp(
            fragment_local * 0.5 + 0.5,
            vec2(0.0),
            vec2(0.999999));
        uvec2 cell = uvec2(floor(glyph_uv * vec2(5.0, 7.0)));
        uint bit_index = cell.y * 5u + cell.x;
        uint word = bit_index < 32u
            ? fragment_glyph_bitmap.x
            : fragment_glyph_bitmap.y;
        uint word_bit = bit_index < 32u ? bit_index : bit_index - 32u;
        if ((word & (1u << word_bit)) == 0u)
            discard;
    }
    else if (fragment_shape < 0.5)
    {
        float radius = length(fragment_local);
        float antialias = max(fwidth(radius) * 1.5, 0.002);
        coverage = 1.0 - smoothstep(1.0 - antialias, 1.0, radius);
        if (coverage <= 0.0)
            discard;
    }
    else if (fragment_shape > 1.5)
    {
        vec2 half_extent = max(fragment_half_extent, vec2(0.001));
        float radius = clamp(
            fragment_corner_radius,
            0.0,
            min(half_extent.x, half_extent.y));
        vec2 pixel = fragment_local * half_extent;
        vec2 q = abs(pixel) - half_extent + vec2(radius);
        float distance_to_edge =
            length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
        float antialias = max(fwidth(distance_to_edge) * 1.5, 0.5);
        coverage = 1.0 - smoothstep(-antialias, antialias, distance_to_edge);
        if (coverage <= 0.0)
            discard;
    }

    output_color = vec4(fragment_color.rgb, fragment_color.a * coverage);
}
)glsl";
}
