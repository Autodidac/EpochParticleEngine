#if !defined(__APPLE__)
#error cocoa_input_bridge.mm must only be compiled on Apple platforms
#endif

#include "cocoa_input_bridge.hpp"

#import <Cocoa/Cocoa.h>

namespace epochengine::particle::demo
{
    namespace
    {
        [[nodiscard]] CocoaInputEvent pointer_event(
            const CocoaInputType type,
            NSEvent* event,
            NSWindow* window) noexcept
        {
            NSView* view = [window contentView];
            const NSPoint logical = [view convertPoint:[event locationInWindow] fromView:nil];
            const NSPoint backing = [view convertPointToBacking:logical];
            const NSRect backing_bounds = [view convertRectToBacking:[view bounds]];

            std::int32_t button = 3;
            if ([event buttonNumber] == 0)
                button = 1;
            else if ([event buttonNumber] == 1)
                button = 2;

            const NSEventModifierFlags modifiers = [event modifierFlags];
            return {
                .type = type,
                .button = button,
                .x = static_cast<float>(backing.x),
                .y = static_cast<float>(backing_bounds.size.height - backing.y),
                .shift = (modifiers & NSEventModifierFlagShift) != 0,
                .control = (modifiers & NSEventModifierFlagControl) != 0
            };
        }

        [[nodiscard]] CocoaInputEvent key_event(
            const CocoaInputType type,
            NSEvent* event) noexcept
        {
            char32_t character{};
            NSString* text = [event charactersIgnoringModifiers];
            if ([text length] != 0)
                character = static_cast<char32_t>([text characterAtIndex:0]);
            const NSEventModifierFlags modifiers = [event modifierFlags];
            return {
                .type = type,
                .key = static_cast<std::int32_t>([event keyCode]),
                .character = character,
                .repeated = [event isARepeat] == YES,
                .shift = (modifiers & NSEventModifierFlagShift) != 0,
                .control = (modifiers & NSEventModifierFlagControl) != 0
            };
        }
    }

    void* install_cocoa_input_bridge(
        void* native_window,
        void* user,
        CocoaInputCallback callback) noexcept
    {
        if (native_window == nullptr || callback == nullptr)
            return nullptr;

        @autoreleasepool
        {
            NSWindow* window = static_cast<NSWindow*>(native_window);
            const NSEventMask mask =
                NSEventMaskKeyDown |
                NSEventMaskKeyUp |
                NSEventMaskMouseMoved |
                NSEventMaskLeftMouseDragged |
                NSEventMaskRightMouseDragged |
                NSEventMaskOtherMouseDragged |
                NSEventMaskLeftMouseDown |
                NSEventMaskRightMouseDown |
                NSEventMaskOtherMouseDown |
                NSEventMaskLeftMouseUp |
                NSEventMaskRightMouseUp |
                NSEventMaskOtherMouseUp;

            id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask
                handler:^NSEvent* (NSEvent* event)
                {
                    if ([event window] != window)
                        return event;

                    CocoaInputEvent translated{};
                    switch ([event type])
                    {
                    case NSEventTypeKeyDown:
                        translated = key_event(CocoaInputType::key_pressed, event);
                        break;
                    case NSEventTypeKeyUp:
                        translated = key_event(CocoaInputType::key_released, event);
                        break;
                    case NSEventTypeMouseMoved:
                    case NSEventTypeLeftMouseDragged:
                    case NSEventTypeRightMouseDragged:
                    case NSEventTypeOtherMouseDragged:
                        translated = pointer_event(CocoaInputType::pointer_moved, event, window);
                        break;
                    case NSEventTypeLeftMouseDown:
                    case NSEventTypeRightMouseDown:
                    case NSEventTypeOtherMouseDown:
                        translated = pointer_event(CocoaInputType::pointer_pressed, event, window);
                        break;
                    case NSEventTypeLeftMouseUp:
                    case NSEventTypeRightMouseUp:
                    case NSEventTypeOtherMouseUp:
                        translated = pointer_event(CocoaInputType::pointer_released, event, window);
                        break;
                    default:
                        return event;
                    }
                    callback(user, translated);
                    return event;
                }];
            return monitor != nil ? [monitor retain] : nullptr;
        }
    }

    void uninstall_cocoa_input_bridge(void* token) noexcept
    {
        if (token == nullptr)
            return;
        @autoreleasepool
        {
            id monitor = static_cast<id>(token);
            [NSEvent removeMonitor:monitor];
            [monitor release];
        }
    }
}
