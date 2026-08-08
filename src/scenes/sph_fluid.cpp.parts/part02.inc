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
                result.metrics[3] = { "GPU COMPUTE", last_update_used_compute_ ? 1.0 : 0.0 };
                result.metric_count = 4;
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
            [[nodiscard]] bool update_compute(
                IComputeBackend& backend,
                float delta_seconds)
            {
                static constexpr std::string_view density_shader = R"glsl(
#version 450
layout(local_size_x = 64) in;
layout(std430, set = 0, binding = 0) buffer SphStorage
{
    uint words[];
} data;
float f(uint index) { return uintBitsToFloat(data.words[index]); }
void putf(uint index, float value) { data.words[index] = floatBitsToUint(value); }

void main()
{
    uint particle = gl_GlobalInvocationID.x;
    uint count = data.words[0];
    if (particle >= count)
        return;
    uint input_base = 64u;
    uint density_base = input_base + count * 4u;
    uint source = input_base + particle * 4u;
    vec2 position = vec2(f(source), f(source + 1u));
    float radius = f(4u);
    float density = f(5u);
    uint neighbors = 0u;
    for (uint other_index = 0u; other_index < count; ++other_index)
    {
        if (other_index == particle)
            continue;
        uint other = input_base + other_index * 4u;
        vec2 delta = vec2(f(other), f(other + 1u)) - position;
        float distance_squared = dot(delta, delta);
        if (distance_squared >= radius * radius)
            continue;
        float distance_value = sqrt(max(distance_squared, 0.0));
        float q = 1.0 - distance_value / radius;
        density += q * q;
        ++neighbors;
    }
    uint output_word = density_base + particle * 2u;
    putf(output_word, max(density, 0.1));
    data.words[output_word + 1u] = neighbors;
}
)glsl";

                static constexpr std::string_view integrate_shader = R"glsl(
#version 450
layout(local_size_x = 64) in;
layout(std430, set = 0, binding = 0) buffer SphStorage
{
    uint words[];
} data;
float f(uint index) { return uintBitsToFloat(data.words[index]); }
void putf(uint index, float value) { data.words[index] = floatBitsToUint(value); }
float pressure(float density)
{
    return clamp(
        (density - f(6u)) * f(7u),
        -f(8u),
        f(9u));
}

void main()
{
    uint particle = gl_GlobalInvocationID.x;
    uint count = data.words[0];
    if (particle >= count)
        return;
    uint input_base = 64u;
    uint density_base = input_base + count * 4u;
    uint output_base = density_base + count * 2u;
    uint source = input_base + particle * 4u;
    vec2 position = vec2(f(source), f(source + 1u));
    vec2 velocity = vec2(f(source + 2u), f(source + 3u));
    float own_density = f(density_base + particle * 2u);
    float own_pressure = pressure(own_density);
    float radius = f(4u);
    vec2 force = vec2(0.0, f(13u) * own_density);

    for (uint other_index = 0u; other_index < count; ++other_index)
    {
        if (other_index == particle)
            continue;
        uint other = input_base + other_index * 4u;
        vec2 delta = vec2(f(other), f(other + 1u)) - position;
        float distance_squared = dot(delta, delta);
        if (distance_squared <= 1.0e-6
            || distance_squared >= radius * radius)
            continue;
        float distance_value = sqrt(distance_squared);
        vec2 direction = delta / distance_value;
        float q = 1.0 - distance_value / radius;
        float other_density = f(density_base + other_index * 2u);
        float average_pressure =
            (own_pressure + pressure(other_density)) * 0.5;
        force -= direction * (
            average_pressure * q * f(10u) / max(other_density, 0.1));
        vec2 other_velocity = vec2(f(other + 2u), f(other + 3u));
        force += (other_velocity - velocity) * (f(11u) * q);
        force += delta * (f(12u) * q * q);
    }

    if (data.words[22] != 0u)
    {
        vec2 delta = vec2(f(20u), f(21u)) - position;
        float distance_squared = dot(delta, delta);
        if (distance_squared > 1.0
            && distance_squared < f(18u) * f(18u))
        {
            float distance_value = sqrt(distance_squared);
            float sign_value = data.words[23] != 0u ? -1.0 : 1.0;
            force += delta / distance_value * sign_value * f(19u)
                * (1.0 - distance_value / f(18u));
        }
    }

    vec2 acceleration = force / own_density;
    float acceleration_squared = dot(acceleration, acceleration);
    if (acceleration_squared > f(14u) * f(14u))
        acceleration *= f(14u) * inversesqrt(acceleration_squared);
    velocity = (velocity + acceleration * f(3u)) * 0.999;
    position += velocity * f(3u);

    vec2 bounds = vec2(f(1u), f(2u));
    float margin = f(15u) + 3.0;
    if (position.x < margin)
    {
        position.x = margin;
        velocity.x = abs(velocity.x) * f(16u);
        velocity.y *= f(17u);
    }
    else if (position.x > bounds.x - margin)
    {
        position.x = bounds.x - margin;
        velocity.x = -abs(velocity.x) * f(16u);
        velocity.y *= f(17u);
    }
    if (position.y < margin)
    {
        position.y = margin;
        velocity.y = abs(velocity.y) * f(16u);
        velocity.x *= f(17u);
    }
    else if (position.y > bounds.y - margin)
    {
        position.y = bounds.y - margin;
        velocity.y = -abs(velocity.y) * f(16u);
        velocity.x *= f(17u);
    }

    uint output_word = output_base + particle * 4u;
    putf(output_word, position.x);
    putf(output_word + 1u, position.y);
    putf(output_word + 2u, velocity.x);
    putf(output_word + 3u, velocity.y);
}
)glsl";

                constexpr std::size_t header_words = 64U;
                constexpr std::size_t particle_stride = 4U;
                constexpr std::size_t density_stride = 2U;
                const std::size_t count = pool_.size();
                const std::size_t density_base =
                    header_words + count * particle_stride;
                const std::size_t output_base =
                    density_base + count * density_stride;
                compute_storage_.assign(
                    output_base + count * particle_stride, 0U);
                const auto put_float = [this](std::size_t word, float value)
                {
                    compute_storage_[word] = std::bit_cast<std::uint32_t>(value);
                };
                compute_storage_[0] = static_cast<std::uint32_t>(count);
                put_float(1, bounds_.width);
                put_float(2, bounds_.height);
                put_float(3, delta_seconds);
                put_float(4, smoothing_radius_);
                put_float(5, self_density_);
                put_float(6, rest_density_);
                put_float(7, pressure_stiffness_);
                put_float(8, minimum_pressure_);
                put_float(9, maximum_pressure_);
                put_float(10, pressure_force_scale_);
                put_float(11, viscosity_);
                put_float(12, cohesion_);
                put_float(13, gravity_);
                put_float(14, maximum_acceleration_);
                put_float(15, particle_radius_);
                put_float(16, boundary_restitution_);
                put_float(17, boundary_friction_);
                put_float(18, pointer_radius_);
                put_float(19, pointer_force_);
                put_float(20, pointer_position_.x);
                put_float(21, pointer_position_.y);
                compute_storage_[22] = pointer_active_ ? 1U : 0U;
                compute_storage_[23] = pointer_repels_ ? 1U : 0U;

                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    const std::size_t word =
                        header_words + particle * particle_stride;
                    put_float(word, positions[particle].x);
                    put_float(word + 1U, positions[particle].y);
                    put_float(word + 2U, velocities[particle].x);
                    put_float(word + 3U, velocities[particle].y);
                }

                std::span<std::uint32_t> words{ compute_storage_ };
                const std::uint32_t groups = static_cast<std::uint32_t>(
                    (count + 63U) / 64U);
                auto density_result = backend.dispatch({
                    .program_id = "epoch.sph.density.v1",
                    .shader_source = density_shader,
                    .storage = std::as_writable_bytes(words),
                    .push_constants = {},
                    .workgroup_count_x = groups
                });
                if (!density_result)
                {
                    compute_disabled_ = true;
                    std::clog << "SPH density compute fallback: " << density_result.error() << '\n';
                    return false;
                }
                auto integrate_result = backend.dispatch({
                    .program_id = "epoch.sph.integrate.v1",
                    .shader_source = integrate_shader,
                    .storage = std::as_writable_bytes(words),
                    .push_constants = {},
                    .workgroup_count_x = groups
                });
                if (!integrate_result)
                {
                    compute_disabled_ = true;
                    std::clog << "SPH integration compute fallback: " << integrate_result.error() << '\n';
                    return false;
                }

                auto mutable_positions = pool_.positions();
                auto mutable_velocities = pool_.velocities();
                average_density_ = 0.0;
                maximum_density_observed_ = 0.0;
                average_neighbors_ = 0.0;
                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    const std::size_t density_word =
                        density_base + particle * density_stride;
                    densities_[particle] =
                        std::bit_cast<float>(compute_storage_[density_word]);
                    neighbor_counts_[particle] =
                        compute_storage_[density_word + 1U];
                    const std::size_t output =
                        output_base + particle * particle_stride;
                    mutable_positions[particle] = {
                        std::bit_cast<float>(compute_storage_[output]),
                        std::bit_cast<float>(compute_storage_[output + 1U])
                    };
                    mutable_velocities[particle] = {
                        std::bit_cast<float>(compute_storage_[output + 2U]),
                        std::bit_cast<float>(compute_storage_[output + 3U])
                    };
                    average_density_ +=
                        static_cast<double>(densities_[particle]);
                    maximum_density_observed_ = std::max(
                        maximum_density_observed_,
                        static_cast<double>(densities_[particle]));
                    average_neighbors_ += neighbor_counts_[particle];
                }
                if (count != 0)
                {
                    average_density_ /= static_cast<double>(count);
                    average_neighbors_ /= static_cast<double>(count);
                }
                last_update_used_compute_ = true;
                return true;
            }

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

