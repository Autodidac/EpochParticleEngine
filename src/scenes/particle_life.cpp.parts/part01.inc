                                    force += direction * strength;
                                    ++neighbor_count;
                                });

                            if (pointer_active_)
                            {
                                Vec2 delta = detail::minimum_image(
                                    pointer_position_ - position,
                                    bounds_);
                                const float distance_squared = length_squared(delta);
                                if (distance_squared > 1.0F
                                    && distance_squared < pointer_radius_ * pointer_radius_)
                                {
                                    const float distance = std::sqrt(distance_squared);
                                    const float sign = pointer_repels_ ? -1.0F : 1.0F;
                                    force += delta / distance
                                        * sign * (1.0F - distance / pointer_radius_) * 5.0F;
                                }
                            }

                            accelerations_[particle] = force * acceleration_scale_;
                            neighbor_counts_[particle] = neighbor_count;
                        }
                    });

                auto mutable_positions = pool_.positions();
                auto mutable_velocities = pool_.velocities();
                context.tasks.parallel_for(
                    count,
                    128U,
                    [this, mutable_positions, mutable_velocities, delta_seconds](
                        std::size_t begin,
                        std::size_t end)
                    {
                        for (std::size_t particle = begin; particle < end; ++particle)
                        {
                            Vec2 velocity =
                                (mutable_velocities[particle]
                                    + accelerations_[particle] * delta_seconds)
                                * damping_;

                            const float speed_squared = length_squared(velocity);
                            if (speed_squared > maximum_speed_ * maximum_speed_)
                                velocity = normalized_or(velocity) * maximum_speed_;

                            mutable_velocities[particle] = velocity;
                            mutable_positions[particle] = detail::wrapped(
                                mutable_positions[particle] + velocity * delta_seconds,
                                bounds_);
                        }
                    });

                interaction_count_ = 0;
                average_speed_ = 0.0;
                for (std::size_t index = 0; index < count; ++index)
                {
                    interaction_count_ += neighbor_counts_[index];
                    average_speed_ += static_cast<double>(length(velocities[index]));
                }
                if (count != 0U)
                    average_speed_ /= static_cast<double>(count);
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                const auto positions = pool_.positions();
                const auto colors = pool_.colors();
                const auto species = pool_.species();

                for (std::size_t index = 0; index < pool_.size(); ++index)
                {
                    Color color = colors[index];
                    const float pulse = 0.15F
                        * std::sin(static_cast<float>(index % 97U) * 0.37F);
                    color.r = std::clamp(color.r + pulse, 0.0F, 1.0F);
                    color.g = std::clamp(color.g + pulse, 0.0F, 1.0F);
                    frame.circle(
                        positions[index],
                        2.0F + static_cast<float>(species[index] & 1U) * 0.45F,
                        color,
                        2);
                }

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        pointer_radius_,
                        pointer_repels_
                            ? Color{ 1.0F, 0.22F, 0.20F, 0.12F }
                            : Color{ 0.22F, 0.72F, 1.0F, 0.12F },
                        1);
                }
            }

            void pointer(const PointerEvent& event) override
            {
                pointer_position_ = event.position;
                pointer_active_ = event.primary_down || event.secondary_down;
                pointer_repels_ = event.secondary_down;
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                for (Vec2& position : pool_.positions())
                    detail::rescale_position(position, old_bounds, new_bounds);
                bounds_ = new_bounds;
                grid_.configure(bounds_, interaction_radius_);
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{
                    .particle_count = pool_.size(),
                    .active_cell_count = 0
                };
                result.metrics[0] = {
                    "INTERACTIONS",
                    static_cast<double>(interaction_count_)
                };
                result.metrics[1] = { "AVERAGE SPEED", average_speed_ };
                result.metrics[2] = {
                    "SPECIES",
                    static_cast<double>(species_count_)
                };
                result.metrics[3] = { "GPU COMPUTE", last_update_used_compute_ ? 1.0 : 0.0 };
                result.metric_count = 4;
                return result;
            }

