            Bounds bounds_{};
            std::uint64_t seed_{};
            ParticlePool pool_{ maximum_particles_ };
            UniformGridIndex grid_{};
            std::vector<float> densities_;
            std::vector<float> pressures_;
            std::vector<Vec2> forces_;
            std::vector<std::uint32_t> neighbor_counts_;
            Vec2 pointer_position_{};
            double average_density_{};
            double maximum_density_observed_{};
            double average_neighbors_{};
            bool pointer_active_{};
            bool pointer_repels_{};
            std::vector<std::uint32_t> compute_storage_;
            bool compute_disabled_{};
            bool last_update_used_compute_{};
        };
    }

    std::unique_ptr<IScene> make_sph_fluid_scene()
    {
        return std::make_unique<SphFluidScene>();
    }
}
