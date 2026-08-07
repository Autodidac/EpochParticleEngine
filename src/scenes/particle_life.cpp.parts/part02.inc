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
        };
    }

    std::unique_ptr<IScene> make_particle_life_scene()
    {
        return std::make_unique<ParticleLifeScene>();
    }
}
