    }

    struct ApplicationState final
    {
        explicit ApplicationState(const LaunchOptions& options)
            : simulation({
                .fixed_step = std::chrono::nanoseconds{ 16'666'667 },
                .maximum_catch_up_steps = 8,
                .seed = options.seed,
                .worker_count = options.workers
            })
            , frame(524'288)
        {
        }

        Simulation simulation;
        RenderFrame frame;
        UiOverlay ui;
        std::unique_ptr<vulkan::Renderer> renderer;
        platform::ContextId context{};
        platform::DriverKind driver{ platform::DriverKind::automatic };
        Vec2 cursor{};
        bool primary_down{};
        bool secondary_down{};
        bool control_down{};
        bool shift_down{};
        double smoothed_frame_seconds{ 1.0 / 60.0 };
        std::chrono::steady_clock::time_point title_update{
            std::chrono::steady_clock::now()
        };
#if defined(__APPLE__)
        void* cocoa_input_token{};
#endif
    };

    [[nodiscard]] Bounds fallback_bounds(
        const platform::ContextSnapshot& context) noexcept
    {
        return {
            static_cast<float>(std::max(context.width, 1)),
            static_cast<float>(std::max(context.height, 1))
        };
    }

    class ParticleLabApplication final : public platform::IApplication
    {
    public:
        [[nodiscard]] platform::ApplicationConfig configure(
            const platform::ApplicationLaunchInfo&) override
        {
            return {
                .runtime = {
                    .dedicated_driver_threads = true,
                    .auto_register_builtin_drivers = true,
                    .idle_wait = std::chrono::milliseconds{ 1 }
                },
                .quit_when_no_active_contexts = true,
                .main_loop_idle_wait = std::chrono::milliseconds{ 1 }
            };
        }

        [[nodiscard]] platform::Status on_start(
            platform::Runtime& runtime,
            const platform::ApplicationLaunchInfo& launch) override
        {
            runtime_ = &runtime;
            const std::string_view executable =
                launch.argc > 0 && launch.argv != nullptr && launch.argv[0] != nullptr
                    ? std::string_view{ launch.argv[0] }
                    : std::string_view{ "EpochParticleStudio" };

            const auto parsed = parse_arguments(launch.argc, launch.argv);
            if (!parsed)
            {
                print_usage(executable);
                return platform_failure("Invalid command-line arguments");
            }
            options_ = *parsed;
            if (options_.show_help)
            {
                print_usage(executable);
                runtime.request_stop();
                return {};
            }

            platform::ContextCallbacks callbacks{
                .initialize = [this](const platform::ContextSnapshot& context)
                {
                    return initialize_context(context);
                },
                .frame = [this](
                    const platform::ContextSnapshot& context,
                    std::chrono::nanoseconds delta)
                {
                    return render_context(context, delta);
                },
                .shutdown = [this](const platform::ContextSnapshot&)
                {
                    shutdown_context();
                },
                .event = [this](const platform::WindowEvent& event)
                {
                    handle_event(event);
                }
            };

            auto context = runtime.create_context({
                .driver = platform::DriverKind::automatic,
                .type = platform::ContextType::vulkan,
                .thread_policy = platform::ThreadPolicy::automatic,
                .window = {
                    .title = "EpochParticleEngine - Particle Studio",
                    .width = options_.width,
                    .height = options_.height,
                    .mode = platform::WindowMode::top_level,
                    .visible = true,
                    .resizable = true,
                    .decorated = true,
                    .high_dpi = true
                },
                .route = "epoch.particle.studio"
            }, std::move(callbacks));

            if (!context)
                return std::unexpected(context.error());
            context_ = *context;
            return {};
        }

        [[nodiscard]] bool on_frame(
            platform::Runtime& runtime,
            std::chrono::nanoseconds) override
        {
            return !fatal_.load(std::memory_order_acquire)
                && runtime.active_context_count() != 0;
        }

        void on_stop(platform::Runtime&) noexcept override
        {
            runtime_ = nullptr;
        }

    private:
        [[nodiscard]] platform::Status initialize_context(
            const platform::ContextSnapshot& context)
        {
            const vulkan::NativeSurface native_surface =
                make_native_surface(context.native);
            if (!native_surface.valid())
            {
                return platform_failure(
                    "EpochPlatformEngine did not provide a supported native window handle");
            }

            auto next_state = std::make_unique<ApplicationState>(options_);
            next_state->context = context.id;
            next_state->driver = context.driver_kind;

