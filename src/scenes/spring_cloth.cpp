#include "scene_common.hpp"
#include "scene_factories.hpp"

#include <epochengine/particle/hash.hpp>
#include <epochengine/particle/random.hpp>
#include <epochengine/particle/render_frame.hpp>
#include <epochengine/particle/scene.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace epochengine::particle::scenes
{
    namespace
    {
        struct ClothNode
        {
            Vec2 position{};
            Vec2 previous{};
            bool pinned{};
        };

        struct ClothConstraint
        {
            std::uint32_t first{};
            std::uint32_t second{};
            float rest_length{};
            bool broken{};
        };

        class SpringClothScene final : public IScene
        {
        public:
            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return {
                    .id = "spring-cloth",
                    .name = "Spring Cloth",
                    .description = "Verlet cloth with structural/shear constraints, mouse pulling, and right-click tearing."
                };
            }

            void reset(const SceneResetContext& context) override
            {
                bounds_ = context.bounds;
                seed_ = context.seed;
                pointer_active_ = false;
                pointer_repels_ = false;
                solved_constraints_ = 0;
                build_cloth();
            }

            void update(const SceneUpdateContext& context) override
            {
                const float dt = context.delta_seconds;
                const float dt_squared = dt * dt;
                const float floor_y = std::max(20.0F, bounds_.height - 14.0F);

                for (ClothNode& node : nodes_)
                {
                    if (node.pinned)
                        continue;

                    const Vec2 velocity = (node.position - node.previous) * 0.995F;
                    node.previous = node.position;
                    node.position += velocity;
                    node.position.y += gravity_ * dt_squared;

                    if (pointer_active_)
                    {
                        const Vec2 delta = pointer_position_ - node.position;
                        const float distance_squared = length_squared(delta);
                        if (distance_squared > 1.0F
                            && distance_squared < pointer_radius_ * pointer_radius_)
                        {
                            const float distance = std::sqrt(distance_squared);
                            const float falloff = 1.0F - distance / pointer_radius_;
                            const Vec2 direction = delta / distance;
                            node.position += direction
                                * (pointer_repels_ ? -1.0F : 1.0F)
                                * (falloff * 7.5F);
                        }
                    }

                    node.position.x = std::clamp(node.position.x, 4.0F, bounds_.width - 4.0F);
                    if (node.position.y > floor_y)
                    {
                        node.position.y = floor_y;
                        node.previous.x += (node.position.x - node.previous.x) * 0.12F;
                    }
                    node.position.y = std::max(4.0F, node.position.y);
                }

                std::uint64_t solved = 0;
                for (int iteration = 0; iteration < solver_iterations_; ++iteration)
                {
                    for (ClothConstraint& constraint : constraints_)
                    {
                        if (constraint.broken)
                            continue;
                        ClothNode& first = nodes_[constraint.first];
                        ClothNode& second = nodes_[constraint.second];
                        const Vec2 delta = second.position - first.position;
                        const float distance_squared = length_squared(delta);
                        if (distance_squared <= 1.0e-8F)
                            continue;
                        const float distance = std::sqrt(distance_squared);
                        if (distance > constraint.rest_length * tear_ratio_)
                        {
                            constraint.broken = true;
                            continue;
                        }

                        const float correction_scale =
                            (distance - constraint.rest_length) / distance;
                        const Vec2 correction = delta * correction_scale;
                        if (!first.pinned && !second.pinned)
                        {
                            first.position += correction * 0.5F;
                            second.position -= correction * 0.5F;
                        }
                        else if (!first.pinned)
                        {
                            first.position += correction;
                        }
                        else if (!second.pinned)
                        {
                            second.position -= correction;
                        }
                        ++solved;
                    }
                }
                solved_constraints_ = solved;
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                const Color structural{ 0.24F, 0.62F, 0.96F, 0.72F };
                const Color shear{ 0.46F, 0.34F, 0.76F, 0.36F };
                for (std::size_t index = 0; index < constraints_.size(); ++index)
                {
                    const ClothConstraint& constraint = constraints_[index];
                    if (constraint.broken)
                        continue;
                    const bool diagonal = index >= structural_constraint_count_;
                    frame.line(
                        nodes_[constraint.first].position,
                        nodes_[constraint.second].position,
                        diagonal ? 0.8F : 1.45F,
                        diagonal ? shear : structural,
                        0);
                }

                for (const ClothNode& node : nodes_)
                {
                    frame.circle(
                        node.position,
                        node.pinned ? 3.8F : 2.1F,
                        node.pinned
                            ? Color{ 1.0F, 0.76F, 0.22F, 1.0F }
                            : Color{ 0.38F, 0.86F, 1.0F, 0.95F },
                        4);
                }

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        11.0F,
                        pointer_repels_
                            ? Color{ 1.0F, 0.28F, 0.22F, 0.75F }
                            : Color{ 0.30F, 0.95F, 0.74F, 0.75F },
                        20);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                pointer_position_ = event.position;
                if (event.action == PointerAction::press)
                {
                    if (event.button == PointerButton::secondary)
                    {
                        cut_at(event.position, event.shift ? 48.0F : 24.0F);
                        pointer_active_ = false;
                        return;
                    }
                    pointer_active_ = event.button == PointerButton::primary;
                    pointer_repels_ = event.control;
                }
                else if (event.action == PointerAction::move
                    && event.secondary_down)
                {
                    cut_at(event.position, event.shift ? 48.0F : 24.0F);
                }
                else if (event.action == PointerAction::release)
                {
                    pointer_active_ = event.primary_down;
                }
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                if (!new_bounds.valid())
                    return;
                const float scale_x = old_bounds.width > 0.0F
                    ? new_bounds.width / old_bounds.width
                    : 1.0F;
                const float scale_y = old_bounds.height > 0.0F
                    ? new_bounds.height / old_bounds.height
                    : 1.0F;
                const float constraint_scale = (scale_x + scale_y) * 0.5F;
                for (ClothNode& node : nodes_)
                {
                    detail::rescale_position(node.position, old_bounds, new_bounds);
                    detail::rescale_position(node.previous, old_bounds, new_bounds);
                }
                for (ClothConstraint& constraint : constraints_)
                    constraint.rest_length *= constraint_scale;
                detail::rescale_position(pointer_position_, old_bounds, new_bounds);
                bounds_ = new_bounds;
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                std::uint64_t broken = 0;
                for (const ClothConstraint& constraint : constraints_)
                    broken += constraint.broken ? 1U : 0U;

                SceneStats result{};
                result.particle_count = static_cast<std::uint64_t>(nodes_.size());
                result.metrics[0] = { "Constraints", static_cast<double>(constraints_.size()) };
                result.metrics[1] = { "Broken", static_cast<double>(broken) };
                result.metrics[2] = { "Solved/tick", static_cast<double>(solved_constraints_) };
                result.metrics[3] = { "Iterations", static_cast<double>(solver_iterations_) };
                result.metric_count = 4;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                for (const ClothNode& node : nodes_)
                {
                    hasher.append_float(node.position.x);
                    hasher.append_float(node.position.y);
                    hasher.append_float(node.previous.x);
                    hasher.append_float(node.previous.y);
                    hasher.append_byte(node.pinned ? 1U : 0U);
                }
                for (const ClothConstraint& constraint : constraints_)
                {
                    hasher.append_u32(constraint.first);
                    hasher.append_u32(constraint.second);
                    hasher.append_float(constraint.rest_length);
                    hasher.append_byte(constraint.broken ? 1U : 0U);
                }
                return hasher.value();
            }

        private:
            [[nodiscard]] static constexpr std::size_t node_index(
                std::size_t x,
                std::size_t y) noexcept
            {
                return y * columns_ + x;
            }

            void add_constraint(std::size_t first, std::size_t second) 
            {
                const float rest = length(
                    nodes_[second].position - nodes_[first].position);
                constraints_.push_back({
                    .first = static_cast<std::uint32_t>(first),
                    .second = static_cast<std::uint32_t>(second),
                    .rest_length = rest,
                    .broken = false
                });
            }

            void build_cloth()
            {
                nodes_.clear();
                constraints_.clear();
                nodes_.reserve(columns_ * rows_);

                const float width = std::min(bounds_.width * 0.72F, 940.0F);
                const float height = std::min(bounds_.height * 0.58F, 500.0F);
                const Vec2 origin{
                    bounds_.width * 0.5F - width * 0.5F,
                    std::max(54.0F, bounds_.height * 0.09F)
                };
                const float spacing_x = width / static_cast<float>(columns_ - 1U);
                const float spacing_y = height / static_cast<float>(rows_ - 1U);

                Pcg32 random(seed_ ^ 0x434c4f5448535052ULL);
                for (std::size_t y = 0; y < rows_; ++y)
                {
                    for (std::size_t x = 0; x < columns_; ++x)
                    {
                        const Vec2 position{
                            origin.x + static_cast<float>(x) * spacing_x,
                            origin.y + static_cast<float>(y) * spacing_y
                                + random.range(-0.35F, 0.35F)
                        };
                        nodes_.push_back({
                            .position = position,
                            .previous = position,
                            .pinned = y == 0U && (x % 4U == 0U || x + 1U == columns_)
                        });
                    }
                }

                for (std::size_t y = 0; y < rows_; ++y)
                {
                    for (std::size_t x = 0; x < columns_; ++x)
                    {
                        const std::size_t current = node_index(x, y);
                        if (x + 1U < columns_)
                            add_constraint(current, node_index(x + 1U, y));
                        if (y + 1U < rows_)
                            add_constraint(current, node_index(x, y + 1U));
                    }
                }
                structural_constraint_count_ = constraints_.size();

                for (std::size_t y = 0; y + 1U < rows_; ++y)
                {
                    for (std::size_t x = 0; x + 1U < columns_; ++x)
                    {
                        add_constraint(node_index(x, y), node_index(x + 1U, y + 1U));
                        add_constraint(node_index(x + 1U, y), node_index(x, y + 1U));
                    }
                }
            }

            void cut_at(Vec2 position, float radius) noexcept
            {
                const float radius_squared = radius * radius;
                for (ClothConstraint& constraint : constraints_)
                {
                    if (constraint.broken)
                        continue;
                    const Vec2 midpoint =
                        (nodes_[constraint.first].position
                            + nodes_[constraint.second].position) * 0.5F;
                    if (length_squared(midpoint - position) <= radius_squared)
                        constraint.broken = true;
                }
            }

            static constexpr std::size_t columns_ = 30;
            static constexpr std::size_t rows_ = 20;
            static constexpr int solver_iterations_ = 6;
            static constexpr float gravity_ = 980.0F;
            static constexpr float tear_ratio_ = 2.15F;
            static constexpr float pointer_radius_ = 105.0F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            std::vector<ClothNode> nodes_;
            std::vector<ClothConstraint> constraints_;
            std::size_t structural_constraint_count_{};
            Vec2 pointer_position_{};
            std::uint64_t solved_constraints_{};
            bool pointer_active_{};
            bool pointer_repels_{};
        };
    }

    std::unique_ptr<IScene> make_spring_cloth_scene()
    {
        return std::make_unique<SpringClothScene>();
    }
}
