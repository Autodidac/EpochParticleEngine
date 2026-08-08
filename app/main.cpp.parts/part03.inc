            auto renderer = vulkan::Renderer::create(native_surface, {
                .enable_validation = options_.validation,
                .vertical_sync = options_.vertical_sync,
                .initial_item_capacity = next_state->frame.maximum_items(),
                .clear_color = { 0.012F, 0.018F, 0.030F, 1.0F }
            });
            if (!renderer)
            {
                return platform_failure(
                    "Vulkan renderer initialization failed: " + renderer.error().message);
            }
            next_state->renderer = std::move(*renderer);

            Bounds framebuffer = next_state->renderer->drawable_bounds();
            if (!framebuffer.valid())
                framebuffer = fallback_bounds(context);
            apply_framebuffer_size(*next_state, framebuffer);

            if (options_.scene < next_state->simulation.scene_count())
                next_state->simulation.select_scene(options_.scene);

#if defined(__APPLE__)
            if (context.driver_kind == platform::DriverKind::cocoa)
            {
                next_state->cocoa_input_token = install_cocoa_input_bridge(
                    context.native.window,
                    this,
                    [](void* user, const CocoaInputEvent& event) noexcept
                    {
                        static_cast<ParticleLabApplication*>(user)->handle_cocoa_event(event);
                    });
            }
#endif

            std::cout << "Vulkan device: " << next_state->renderer->device_name() << '\n'
                      << "Platform driver: " << platform::to_string(context.driver_kind) << '\n'
                      << "Worker threads: " << next_state->simulation.worker_count() << '\n'
                      << "Seed: " << next_state->simulation.seed() << '\n';

            context_ = context.id;
            state_ = std::move(next_state);
            return {};
        }

        void shutdown_context() noexcept
        {
            if (!state_)
                return;
#if defined(__APPLE__)
            uninstall_cocoa_input_bridge(state_->cocoa_input_token);
            state_->cocoa_input_token = nullptr;
#endif
            state_.reset();
            context_ = {};
        }

        void apply_framebuffer_size(ApplicationState& state, Bounds framebuffer) noexcept
        {
            if (!framebuffer.valid())
                return;
            state.ui.resize(framebuffer);
            state.simulation.resize(state.ui.layout().scene_bounds());
        }

        [[nodiscard]] bool render_context(
            const platform::ContextSnapshot& context,
            std::chrono::nanoseconds elapsed)
        {
            if (!state_ || !state_->renderer)
                return false;

            const double frame_seconds = std::chrono::duration<double>(elapsed).count();
            if (frame_seconds > 0.0 && frame_seconds < 1.0)
            {
                state_->smoothed_frame_seconds +=
                    (frame_seconds - state_->smoothed_frame_seconds) * 0.08;
            }

            Bounds framebuffer = state_->renderer->drawable_bounds();
            if (!framebuffer.valid())
                return true;
            if (state_->ui.layout().framebuffer.width != framebuffer.width ||
                state_->ui.layout().framebuffer.height != framebuffer.height)
            {
                apply_framebuffer_size(*state_, framebuffer);
            }

            state_->simulation.advance(elapsed);
            state_->frame.begin(framebuffer);
            state_->simulation.render(state_->frame);
            state_->ui.render(
                state_->frame,
                state_->simulation,
                state_->smoothed_frame_seconds > 0.0
                    ? 1.0 / state_->smoothed_frame_seconds
                    : 0.0,
                state_->smoothed_frame_seconds * 1'000.0,
                state_->renderer->item_capacity(),
                state_->renderer->device_name());
            state_->frame.finalize();

            if (auto draw = state_->renderer->draw(state_->frame); !draw)
            {
                std::cerr << "Vulkan rendering failed: " << draw.error().message << '\n';
                fatal_.store(true, std::memory_order_release);
                return false;
            }

            const auto now = std::chrono::steady_clock::now();
            if (runtime_ != nullptr && now - state_->title_update >= std::chrono::milliseconds{ 500 })
            {
                state_->title_update = now;
                const SceneInfo info = state_->simulation.active_scene_info();
                const int frames_per_second = static_cast<int>(std::clamp(
                    state_->smoothed_frame_seconds > 0.0
                        ? 1.0 / state_->smoothed_frame_seconds
                        : 0.0,
                    0.0,
                    static_cast<double>(std::numeric_limits<int>::max())));
                std::string title{ "EpochParticleEngine | " };
                title.append(info.name);
                title.append(" | ");
                title.append(std::to_string(frames_per_second));
                title.append(" FPS | ");
                title.append(state_->renderer->device_name());
                if (auto status = runtime_->set_title(context.id, title); !status)
                    std::cerr << "Window title update failed: " << status.error().message << '\n';
            }
            return true;
        }

