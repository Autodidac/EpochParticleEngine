        void handle_event(const platform::WindowEvent& event)
        {
            if (!state_)
                return;

            switch (event.type)
            {
            case platform::WindowEventType::key_pressed:
                handle_key(decode_key(state_->driver, event.key), true, event.repeated);
                break;
            case platform::WindowEventType::key_released:
                handle_key(decode_key(state_->driver, event.key), false, false);
                break;
            case platform::WindowEventType::mouse_moved:
                handle_pointer_move({
                    static_cast<float>(event.x),
                    static_cast<float>(event.y)
                });
                break;
            case platform::WindowEventType::mouse_button_pressed:
            case platform::WindowEventType::mouse_button_released:
                handle_pointer_button(
                    translate_pointer_button(event.button),
                    event.type == platform::WindowEventType::mouse_button_pressed,
                    {
                        static_cast<float>(event.x),
                        static_cast<float>(event.y)
                    });
                break;
            case platform::WindowEventType::resized:
                if (event.width > 0 && event.height > 0
                    && !state_->renderer->drawable_bounds().valid())
                {
                    apply_framebuffer_size(*state_, {
                        static_cast<float>(event.width),
                        static_cast<float>(event.height)
                    });
                }
                break;
            case platform::WindowEventType::dpi_changed:
                if (event.width > 0)
                {
                    state_->ui.set_dpi_scale(std::clamp(
                        static_cast<float>(event.width) / 96.0F,
                        0.5F,
                        4.0F));
                    const Bounds framebuffer =
                        state_->renderer->drawable_bounds();
                    if (framebuffer.valid())
                        apply_framebuffer_size(*state_, framebuffer);
                }
                break;
            default:
                break;
            }
        }

#if defined(__APPLE__)
        void handle_cocoa_event(const CocoaInputEvent& event) noexcept
        {
            if (!state_)
                return;
            state_->shift_down = event.shift;
            state_->control_down = event.control;
            switch (event.type)
            {
            case CocoaInputType::key_pressed:
                handle_key(
                    decode_key(state_->driver, event.key, event.character),
                    true,
                    event.repeated);
                break;
            case CocoaInputType::key_released:
                handle_key(
                    decode_key(state_->driver, event.key, event.character),
                    false,
                    false);
                break;
            case CocoaInputType::pointer_moved:
                handle_pointer_move({ event.x, event.y });
                break;
            case CocoaInputType::pointer_pressed:
                handle_pointer_button(
                    translate_pointer_button(event.button),
                    true,
                    { event.x, event.y });
                break;
            case CocoaInputType::pointer_released:
                handle_pointer_button(
                    translate_pointer_button(event.button),
                    false,
                    { event.x, event.y });
                break;
            }
        }
#endif

        [[nodiscard]] PointerButton translate_pointer_button(
            std::int32_t button) const noexcept
        {
            if (!state_)
                return PointerButton::middle;
            if (button == 1)
                return PointerButton::primary;
            if (state_->driver == platform::DriverKind::x11)
                return button == 3
                    ? PointerButton::secondary
                    : PointerButton::middle;
            return button == 2
                ? PointerButton::secondary
                : PointerButton::middle;
        }

        void handle_key(LogicalKey key, bool pressed, bool repeated) noexcept
        {
            if (!state_)
                return;
            if (key == LogicalKey::shift)
            {
                state_->shift_down = pressed;
                return;
            }
            if (key == LogicalKey::control)
            {
                state_->control_down = pressed;
                return;
            }
            if (!pressed)
                return;

            const auto first_scene = std::to_underlying(LogicalKey::scene_1);
            const auto last_scene = std::to_underlying(LogicalKey::scene_9);
            const auto value = std::to_underlying(key);
            if (value >= first_scene && value <= last_scene)
            {
                state_->simulation.select_scene(
                    static_cast<std::size_t>(value - first_scene));
                return;
            }

            switch (key)
            {
            case LogicalKey::escape:
                if (runtime_ != nullptr)
                    runtime_->request_context_close(state_->context);
                break;
            case LogicalKey::tab:
                if (!repeated)
                {
                    if (state_->shift_down)
                        state_->simulation.previous_scene();
                    else
                        state_->simulation.next_scene();
                }
                break;
            case LogicalKey::space:
                if (!repeated)
                    state_->simulation.toggle_paused();
                break;
            case LogicalKey::reset:
                if (!repeated)
                    state_->simulation.reset();
                break;
            case LogicalKey::step:
                if (!repeated)
                    state_->simulation.step_once();
                break;
            case LogicalKey::slower:
                if (!repeated)
                {
                    state_->simulation.set_time_scale(
                        state_->simulation.time_scale() * 0.5);
                }
                break;
            case LogicalKey::faster:
                if (!repeated)
                {
                    state_->simulation.set_time_scale(
                        state_->simulation.time_scale() * 2.0);
                }
                break;
            case LogicalKey::panel:
                if (!repeated)
                {
                    state_->ui.toggle_panel();
                    state_->simulation.resize(
                        state_->ui.layout().scene_bounds());
                }
                break;
            default:
                break;
            }
        }

