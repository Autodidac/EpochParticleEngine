        [[nodiscard]] platform::Status platform_failure(std::string message)
        {
            return std::unexpected(platform::Error{
                .code = platform::ErrorCode::initialization_failed,
                .message = std::move(message)
            });
        }

        [[nodiscard]] vulkan::NativeSurface make_native_surface(
            const platform::NativeWindowHandle& native) noexcept
        {
            switch (native.kind)
            {
            case platform::NativeHandleKind::win32:
                return {
                    .kind = vulkan::NativeSurfaceKind::win32,
                    .window = native.window,
                    .value = native.value
                };
            case platform::NativeHandleKind::x11:
                return {
                    .kind = vulkan::NativeSurfaceKind::xlib,
                    .display = native.display,
                    .value = native.value
                };
            case platform::NativeHandleKind::cocoa:
                return {
                    .kind = vulkan::NativeSurfaceKind::metal,
                    .window = native.window,
                    .value = native.value
                };
            case platform::NativeHandleKind::none:
            case platform::NativeHandleKind::headless:
            case platform::NativeHandleKind::custom:
            default:
                return {};
            }
        }

        enum class LogicalKey : std::uint8_t
        {
            none,
            escape,
            tab,
            space,
            reset,
            step,
            slower,
            faster,
            panel,
            shift,
            control,
            scene_1,
            scene_2,
            scene_3,
            scene_4,
            scene_5,
            scene_6,
            scene_7,
            scene_8,
            scene_9
        };

        [[nodiscard]] LogicalKey scene_key(char32_t character) noexcept
        {
            if (character >= U'1' && character <= U'9')
            {
                return static_cast<LogicalKey>(
                    std::to_underlying(LogicalKey::scene_1) +
                    static_cast<std::uint8_t>(character - U'1'));
            }
            return LogicalKey::none;
        }

        [[nodiscard]] LogicalKey decode_key(
            platform::DriverKind driver,
            std::int32_t key,
            char32_t character = {}) noexcept
        {
            if (const LogicalKey scene = scene_key(character); scene != LogicalKey::none)
                return scene;

            switch (character)
            {
            case U'\x1b': return LogicalKey::escape;
            case U'\t': return LogicalKey::tab;
            case U' ': return LogicalKey::space;
            case U'r':
            case U'R': return LogicalKey::reset;
            case U'.': return LogicalKey::step;
            case U'-': return LogicalKey::slower;
            case U'=':
            case U'+': return LogicalKey::faster;
            default: break;
            }

            if (driver == platform::DriverKind::win32)
            {
                if (key >= 0x31 && key <= 0x39)
                    return scene_key(static_cast<char32_t>(key));
                switch (key)
                {
                case 0x1B: return LogicalKey::escape;
                case 0x09: return LogicalKey::tab;
                case 0x20: return LogicalKey::space;
                case 0x52: return LogicalKey::reset;
                case 0xBE: return LogicalKey::step;
                case 0xBD:
                case 0x6D: return LogicalKey::slower;
                case 0xBB:
                case 0x6B: return LogicalKey::faster;
                case 0x70: return LogicalKey::panel;
                case 0x10:
                case 0xA0:
                case 0xA1: return LogicalKey::shift;
                case 0x11:
                case 0xA2:
                case 0xA3: return LogicalKey::control;
                default: return LogicalKey::none;
                }
            }

            if (driver == platform::DriverKind::x11)
            {
                if (key >= 0x31 && key <= 0x39)
                    return scene_key(static_cast<char32_t>(key));
                switch (key)
                {
                case 0xFF1B: return LogicalKey::escape;
                case 0xFF09: return LogicalKey::tab;
                case 0x20: return LogicalKey::space;
                case 0x72:
                case 0x52: return LogicalKey::reset;
                case 0x2E: return LogicalKey::step;
                case 0x2D: return LogicalKey::slower;
                case 0x3D:
                case 0x2B: return LogicalKey::faster;
                case 0xFFBE: return LogicalKey::panel;
                case 0xFFE1:
                case 0xFFE2: return LogicalKey::shift;
                case 0xFFE3:
                case 0xFFE4: return LogicalKey::control;
                default: return LogicalKey::none;
                }
            }

            if (driver == platform::DriverKind::cocoa)
            {
                switch (key)
                {
                case 53: return LogicalKey::escape;
                case 48: return LogicalKey::tab;
                case 49: return LogicalKey::space;
                case 122: return LogicalKey::panel;
                case 56:
                case 60: return LogicalKey::shift;
                case 59:
                case 62: return LogicalKey::control;
                default: return LogicalKey::none;
                }
            }
            return LogicalKey::none;
        }

