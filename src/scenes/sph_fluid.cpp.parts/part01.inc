                context.tasks.parallel_for(
                    count,
                    64U,
                    [this, positions, velocities](std::size_t begin, std::size_t end)
                    {
                        for (std::size_t particle = begin; particle < end; ++particle)
                        {
                            Vec2 force{ 0.0F, gravity_ * densities_[particle] };
                            grid_.for_each_neighbor(
                                positions[particle],
                                smoothing_radius_,
                                false,
                                [&](std::uint32_t neighbor)
                                {
                                    if (neighbor == particle)
                                        return;

                                    const Vec2 delta =
                                        positions[neighbor] - positions[particle];
                                    const float distance_squared = length_squared(delta);
                                    if (distance_squared <= 1.0e-6F
                                        || distance_squared >= smoothing_radius_
                                            * smoothing_radius_)
                                    {
                                        return;
                                    }

                                    const float distance = std::sqrt(distance_squared);
                                    const Vec2 direction = delta / distance;
                                    const float q = 1.0F - distance / smoothing_radius_;
                                    const float average_pressure =
                                        (pressures_[particle] + pressures_[neighbor]) * 0.5F;
                                    force -= direction
                                        * (average_pressure * q * pressure_force_scale_
                                            / std::max(densities_[neighbor], 0.1F));
                                    force += (velocities[neighbor] - velocities[particle])
                                        * (viscosity_ * q);
                                    force += delta * (cohesion_ * q * q);
                                });

                            if (pointer_active_)
                            {
                                const Vec2 delta =
                                    pointer_position_ - positions[particle];
                                const float distance_squared = length_squared(delta);
                                if (distance_squared > 1.0F
                                    && distance_squared < pointer_radius_ * pointer_radius_)
                                {
                                    const float distance = std::sqrt(distance_squared);
                                    const float sign = pointer_repels_ ? -1.0F : 1.0F;
                                    force += delta / distance
                                        * sign * pointer_force_
                                        * (1.0F - distance / pointer_radius_);
                                }
                            }

                            Vec2 acceleration = force / densities_[particle];
                            const float acceleration_squared = length_squared(acceleration);
                            if (acceleration_squared
                                > maximum_acceleration_ * maximum_acceleration_)
                            {
                                acceleration =
                                    normalized_or(acceleration) * maximum_acceleration_;
                            }
                            forces_[particle] = acceleration;
                        }
                    });

                auto mutable_positions = pool_.positions();
                auto mutable_velocities = pool_.velocities();
                const float delta_seconds = context.delta_seconds;
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
                                mutable_velocities[particle]
                                + forces_[particle] * delta_seconds;
                            velocity *= 0.999F;
                            Vec2 position =
                                mutable_positions[particle] + velocity * delta_seconds;

                            const float margin = particle_radius_ + 3.0F;
                            if (position.x < margin)
                            {
                                position.x = margin;
                                velocity.x = std::abs(velocity.x) * boundary_restitution_;
                                velocity.y *= boundary_friction_;
                            }
                            else if (position.x > bounds_.width - margin)
                            {
                                position.x = bounds_.width - margin;
                                velocity.x = -std::abs(velocity.x) * boundary_restitution_;
                                velocity.y *= boundary_friction_;
                            }

