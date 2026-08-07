                            if (position.y < margin)
                            {
                                position.y = margin;
                                velocity.y = std::abs(velocity.y) * boundary_restitution_;
                                velocity.x *= boundary_friction_;
                            }
                            else if (position.y > bounds_.height - margin)
                            {
                                position.y = bounds_.height - margin;
                                velocity.y = -std::abs(velocity.y) * boundary_restitution_;
                                velocity.x *= boundary_friction_;
                            }

                            mutable_positions[particle] = position;
                            mutable_velocities[particle] = velocity;
                        }
                    });

                average_density_ = 0.0;
                maximum_density_observed_ = 0.0;
                average_neighbors_ = 0.0;
                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    average_density_ += static_cast<double>(densities_[particle]);
                    maximum_density_observed_ = std::max(
                        maximum_density_observed_,
                        static_cast<double>(densities_[particle]));
                    average_neighbors_ += static_cast<double>(neighbor_counts_[particle]);
                }
                if (count != 0U)
                {
                    average_density_ /= static_cast<double>(count);
                    average_neighbors_ /= static_cast<double>(count);
                }
            }

            void render(RenderFrame& frame, Bounds) const override
            {
                const auto positions = pool_.positions();
                for (std::size_t particle = 0; particle < pool_.size(); ++particle)
                {
                    const float density_fraction = std::clamp(
                        (densities_[particle] - 2.0F) / 5.0F,
                        0.0F,
                        1.0F);
                    const Color color = lerp(
                        Color{ 0.08F, 0.36F, 0.96F, 0.84F },
                        Color{ 0.32F, 0.92F, 1.0F, 0.98F },
                        density_fraction);
                    frame.circle(
                        positions[particle],
                        particle_radius_,
                        color,
                        2);
                }

                if (pointer_active_)
                {
                    frame.circle(
                        pointer_position_,
                        pointer_radius_,
                        pointer_repels_
                            ? Color{ 1.0F, 0.24F, 0.22F, 0.10F }
                            : Color{ 0.22F, 0.70F, 1.0F, 0.10F },
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
                grid_.configure(bounds_, smoothing_radius_);
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                SceneStats result{
                    .particle_count = pool_.size(),
                    .active_cell_count = 0
                };
                result.metrics[0] = { "AVERAGE DENSITY", average_density_ };
                result.metrics[1] = { "MAX DENSITY", maximum_density_observed_ };
                result.metrics[2] = { "AVERAGE NEIGHBORS", average_neighbors_ };
                result.metric_count = 3;
                return result;
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(pool_.state_hash());
                for (const float density : densities_)
                    hasher.append_float(density);
                return hasher.value();
            }

        private:
            void resize_working_sets()
            {
                densities_.assign(pool_.size(), self_density_);
                pressures_.assign(pool_.size(), 0.0F);
                forces_.assign(pool_.size(), {});
                neighbor_counts_.assign(pool_.size(), 0U);
            }

            static constexpr std::size_t maximum_particles_ = 1'400;
            static constexpr float particle_radius_ = 3.7F;
            static constexpr float smoothing_radius_ = 22.0F;
            static constexpr float self_density_ = 1.0F;
            static constexpr float rest_density_ = 4.1F;
            static constexpr float pressure_stiffness_ = 115.0F;
            static constexpr float minimum_pressure_ = 30.0F;
            static constexpr float maximum_pressure_ = 450.0F;
            static constexpr float pressure_force_scale_ = 52.0F;
            static constexpr float viscosity_ = 8.5F;
            static constexpr float cohesion_ = 0.18F;
            static constexpr float gravity_ = 470.0F;
            static constexpr float maximum_acceleration_ = 2'400.0F;
            static constexpr float boundary_restitution_ = 0.28F;
            static constexpr float boundary_friction_ = 0.90F;
            static constexpr float pointer_radius_ = 105.0F;
            static constexpr float pointer_force_ = 5'800.0F;

