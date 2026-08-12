#pragma once

#include "core/platform/Types.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// InputAction.h
//
// POD types describing how a named semantic action (e.g. "Jump", "Interact")
// maps to one or more physical input sources. A single action can have
// bindings from multiple device types simultaneously - pressing Space on a
// keyboard and pressing the South button on a controller both trigger the
// same "Jump" action without any game-side branching.
//
// Device coverage:
//   Keyboard  - SDL SDLK_* keycodes
//   Gamepad   - SDL_GAMEPAD_BUTTON_* (digital) or SDL_GAMEPAD_AXIS_* (analog)
//   Touch     - virtual control regions mapped to an action name in input.ini
//               (a VirtualControlLayout converts touch coords -> action name,
//               the resolved name is then fed back into InputManager as a
//               synthetic "digital" press, keeping the polling API identical
//               across all device types)
// ---------------------------------------------------------------------------

namespace dt
{
    enum class InputDeviceType : u8
    {
        Keyboard = 0,
        Gamepad  = 1,
        Touch    = 2,
    };

    enum class InputBindingKind : u8
    {
        Digital = 0,   // Boolean: pressed or not
        Axis    = 1,   // f32 in [-1, 1], maps to a named axis
    };

    // A single physical source that contributes to one logical action.
    struct InputBinding
    {
        InputDeviceType device = InputDeviceType::Keyboard;
        InputBindingKind kind  = InputBindingKind::Digital;

        // For Keyboard:  SDL keycode (SDLK_*)
        // For Gamepad:   SDL_GamepadButton or SDL_GamepadAxis cast to i32
        // For Touch:     virtual control slot index (0 = DPad, 1+ = buttons)
        i32 code = 0;

        // For axis bindings: the SDL axis value is multiplied by this scale
        // before being stored (use -1.0 to invert, 1.0 for normal direction).
        f32 axisScale = 1.0f;

        // Axis deadzone - values within [-deadzone, +deadzone] are snapped to 0.
        f32 deadzone = 0.15f;
    };

    // One named logical action and all its physical bindings.
    struct InputActionDef
    {
        std::string              name;     // "Jump", "MoveLeft", "Interact"
        std::vector<InputBinding> bindings;
        bool                     isAxis = false; // true -> GetAxis(), false -> IsActionPressed()
    };
}
