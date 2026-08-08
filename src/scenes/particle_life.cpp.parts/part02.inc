            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                StableHasher hasher;
                hasher.append_u64(seed_);
                hasher.append_u64(pool_.state_hash());
                for (const float value : attraction_)
                    hasher.append_float(value);
                return hasher.value();
            }

        private:
            [[nodiscard]] bool update_compute(
                IComputeBackend& backend,
                float delta_seconds)
            {
                static constexpr std::string_view shader = R"glsl(
#version 450
layout(local_size_x = 64) in;
layout(std430, set = 0, binding = 0) buffer ParticleStorage
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
    uint output_base = input_base + count * 5u;
    uint source = input_base + particle * 5u;
    vec2 position = vec2(f(source), f(source + 1u));
    vec2 velocity = vec2(f(source + 2u), f(source + 3u));
    uint own_species = data.words[source + 4u];

    vec2 bounds = vec2(f(2u), f(3u));
    float interaction_radius = f(5u);
    float repulsion_radius = f(6u);
    vec2 force = vec2(0.0);
    uint neighbors = 0u;
    for (uint neighbor = 0u; neighbor < count; ++neighbor)
    {
        if (neighbor == particle)
            continue;
        uint other = input_base + neighbor * 5u;
        vec2 delta = vec2(f(other), f(other + 1u)) - position;
        if (delta.x > bounds.x * 0.5) delta.x -= bounds.x;
        if (delta.x < -bounds.x * 0.5) delta.x += bounds.x;
        if (delta.y > bounds.y * 0.5) delta.y -= bounds.y;
        if (delta.y < -bounds.y * 0.5) delta.y += bounds.y;
        float distance_squared = dot(delta, delta);
        if (distance_squared <= 1.0e-8
            || distance_squared >= interaction_radius * interaction_radius)
            continue;

        float distance = sqrt(distance_squared);
        float strength;
        if (distance < repulsion_radius)
        {
            strength = -2.4 * (1.0 - distance / repulsion_radius);
        }
        else
        {
            float phase = (distance - repulsion_radius)
                / (interaction_radius - repulsion_radius);
            float envelope = sin(phase * 3.14159265358979323846);
            uint other_species = data.words[other + 4u];
            strength = f(16u + own_species * data.words[1] + other_species)
                * envelope;
        }
        force += delta / distance * strength;
        ++neighbors;
    }

    if (data.words[13] != 0u)
    {
        vec2 delta = vec2(f(11u), f(12u)) - position;
        if (delta.x > bounds.x * 0.5) delta.x -= bounds.x;
        if (delta.x < -bounds.x * 0.5) delta.x += bounds.x;
        if (delta.y > bounds.y * 0.5) delta.y -= bounds.y;
        if (delta.y < -bounds.y * 0.5) delta.y += bounds.y;
        float distance_squared = dot(delta, delta);
        float pointer_radius = f(7u);
        if (distance_squared > 1.0
            && distance_squared < pointer_radius * pointer_radius)
        {
            float distance = sqrt(distance_squared);
            float sign_value = data.words[14] != 0u ? -1.0 : 1.0;
            force += delta / distance * sign_value
                * (1.0 - distance / pointer_radius) * 5.0;
        }
    }

    velocity = (velocity + force * f(8u) * f(4u)) * f(10u);
    float speed = length(velocity);
    if (speed > f(9u))
        velocity *= f(9u) / speed;
    position += velocity * f(4u);
    position = mod(mod(position, bounds) + bounds, bounds);

    uint output_word = output_base + particle * 6u;
    putf(output_word, position.x);
    putf(output_word + 1u, position.y);
    putf(output_word + 2u, velocity.x);
    putf(output_word + 3u, velocity.y);
    data.words[output_word + 4u] = own_species;
    data.words[output_word + 5u] = neighbors;
}
)glsl";

                constexpr std::size_t header_words = 64U;
                constexpr std::size_t input_stride = 5U;
                constexpr std::size_t output_stride = 6U;
                const std::size_t count = pool_.size();
                const std::size_t output_base =
                    header_words + count * input_stride;
                compute_storage_.assign(
                    output_base + count * output_stride, 0U);
                const auto put_float = [this](std::size_t word, float value)
                {
                    compute_storage_[word] = std::bit_cast<std::uint32_t>(value);
                };
                compute_storage_[0] = static_cast<std::uint32_t>(count);
                compute_storage_[1] = static_cast<std::uint32_t>(species_count_);
                put_float(2, bounds_.width);
                put_float(3, bounds_.height);
                put_float(4, delta_seconds);
                put_float(5, interaction_radius_);
                put_float(6, repulsion_radius_);
                put_float(7, pointer_radius_);
                put_float(8, acceleration_scale_);
                put_float(9, maximum_speed_);
                put_float(10, damping_);
                put_float(11, pointer_position_.x);
                put_float(12, pointer_position_.y);
                compute_storage_[13] = pointer_active_ ? 1U : 0U;
                compute_storage_[14] = pointer_repels_ ? 1U : 0U;
                for (std::size_t index = 0; index < attraction_.size(); ++index)
                    put_float(16U + index, attraction_[index]);

                const auto positions = pool_.positions();
                const auto velocities = pool_.velocities();
                const auto species = pool_.species();
                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    const std::size_t word =
                        header_words + particle * input_stride;
                    put_float(word, positions[particle].x);
                    put_float(word + 1U, positions[particle].y);
                    put_float(word + 2U, velocities[particle].x);
                    put_float(word + 3U, velocities[particle].y);
                    compute_storage_[word + 4U] = species[particle];
                }

                std::span<std::uint32_t> words{ compute_storage_ };
                const auto result = backend.dispatch({
                    .program_id = "epoch.particle-life.v1",
                    .shader_source = shader,
                    .storage = std::as_writable_bytes(words),
                    .push_constants = {},
                    .workgroup_count_x = static_cast<std::uint32_t>(
                        (count + 63U) / 64U)
                });
                if (!result)
                {
                    compute_disabled_ = true;
                    std::clog << "Particle Life compute fallback: " << result.error() << '\n';
                    return false;
                }

                auto mutable_positions = pool_.positions();
                auto mutable_velocities = pool_.velocities();
                interaction_count_ = 0;
                average_speed_ = 0.0;
                for (std::size_t particle = 0; particle < count; ++particle)
                {
                    const std::size_t word =
                        output_base + particle * output_stride;
                    mutable_positions[particle] = {
                        std::bit_cast<float>(compute_storage_[word]),
                        std::bit_cast<float>(compute_storage_[word + 1U])
                    };
                    mutable_velocities[particle] = {
                        std::bit_cast<float>(compute_storage_[word + 2U]),
                        std::bit_cast<float>(compute_storage_[word + 3U])
                    };
                    neighbor_counts_[particle] = compute_storage_[word + 5U];
                    interaction_count_ += neighbor_counts_[particle];
                    average_speed_ += static_cast<double>(
                        length(mutable_velocities[particle]));
                }
                if (count != 0)
                    average_speed_ /= static_cast<double>(count);
                last_update_used_compute_ = true;
                return true;
            }

            static constexpr std::size_t species_count_ = 6;
            static constexpr std::size_t particle_count_ = 3'600;
            static constexpr float interaction_radius_ = 46.0F;
            static constexpr float repulsion_radius_ = 8.5F;
            static constexpr float pointer_radius_ = 115.0F;
            static constexpr float acceleration_scale_ = 72.0F;
            static constexpr float maximum_speed_ = 145.0F;
            static constexpr float damping_ = 0.986F;

            Bounds bounds_{};
            std::uint64_t seed_{};
            ParticlePool pool_{ particle_count_ };
            UniformGridIndex grid_{};
            std::array<float, species_count_ * species_count_> attraction_{};
            std::vector<Vec2> accelerations_;
            std::vector<std::uint32_t> neighbor_counts_;
            Vec2 pointer_position_{};
            std::uint64_t interaction_count_{};
            double average_speed_{};
            bool pointer_active_{};
            bool pointer_repels_{};
            std::vector<std::uint32_t> compute_storage_;
            bool compute_disabled_{};
            bool last_update_used_compute_{};
        };
    }

    std::unique_ptr<IScene> make_particle_life_scene()
    {
        return std::make_unique<ParticleLifeScene>();
    }
}
