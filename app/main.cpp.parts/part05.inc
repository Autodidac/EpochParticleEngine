        void handle_pointer_move(Vec2 position) noexcept
        {
            if (!state_)
                return;
            state_->cursor = position;
            if (!state_->ui.layout().point_in_scene(position))
                return;
            state_->simulation.pointer({
                .action = PointerAction::move,
                .button = PointerButton::primary,
                .position = position,
                .primary_down = state_->primary_down,
                .secondary_down = state_->secondary_down,
                .shift = state_->shift_down,
                .control = state_->control_down
            });
        }

        void handle_pointer_button(
            PointerButton button,
            bool pressed,
            Vec2 position) noexcept
        {
            if (!state_)
                return;
            state_->cursor = position;
            if (button == PointerButton::primary)
                state_->primary_down = pressed;
            else if (button == PointerButton::secondary)
                state_->secondary_down = pressed;

            if (button == PointerButton::primary && pressed)
            {
                if (const auto scene = state_->ui.scene_at(position, state_->simulation))
                {
                    state_->simulation.select_scene(*scene);
                    return;
                }
            }

            const bool inside_scene = state_->ui.layout().point_in_scene(position);
            if (!inside_scene && pressed)
                return;
            const Bounds scene_bounds = state_->ui.layout().scene_bounds();
            state_->simulation.pointer({
                .action = pressed ? PointerAction::press : PointerAction::release,
                .button = button,
                .position = scene_bounds.clamp_point(position),
                .primary_down = state_->primary_down,
                .secondary_down = state_->secondary_down,
                .shift = state_->shift_down,
                .control = state_->control_down
            });
        }

        LaunchOptions options_{};
        platform::Runtime* runtime_{};
        platform::ContextId context_{};
        std::unique_ptr<ApplicationState> state_;
        std::atomic_bool fatal_{};
    };

    namespace
    {
        platform::IApplication* EPOCH_PLATFORM_CALL create_application(
            const platform::ApplicationLaunchInfo&) noexcept
        {
            return new (std::nothrow) ParticleLabApplication();
        }

        void EPOCH_PLATFORM_CALL destroy_application(platform::IApplication* application) noexcept
        {
            delete application;
        }
    }
}

extern "C" void EPOCH_PLATFORM_CALL epoch_platform_application_factory(
    epochengine::platform::ApplicationFactory* factory) noexcept
{
    if (factory == nullptr)
        return;
    *factory = {
        .create = &epochengine::particle::demo::create_application,
        .destroy = &epochengine::particle::demo::destroy_application
    };
}
