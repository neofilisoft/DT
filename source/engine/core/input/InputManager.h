#pragma once

#include "core/input/InputAction.h"
#include "core/platform/Types.h"

#include <SDL3/SDL.h>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// InputManager.h
//
// Cross-platform input action mapping system. Translates raw SDL events from
// keyboard, gamepad, and touch into named semantic actions ("Jump", "Interact",
// "MoveLeft") that game/simulation code can poll without knowing what physical
// device the player is using.
//
// Thread model:
//   ProcessEvent() and BeginFrame()/EndFrame() are called from the render/
//   platform thread (same thread as SDL event polling). The state arrays are
//   written only from that thread, so no locking is needed for internal state.
//   External callers (Lua scripts running on the sim thread, game code on any
//   thread) read through IsActionPressed() / GetAxis() which are safe because:
//     - Writes happen atomically per-field (trivial types, cache-aligned).
//     - The sim thread reads state that may be one frame stale, which is
//       acceptable for gameplay (same tradeoff as any frame-latency input).
//
// Usage:
//   InputManager::Get().LoadBindings("engine/config/input.ini");
//   // in event loop:
//   InputManager::Get().ProcessEvent(sdlEvent);
//   // in game code:
//   if (InputManager::Get().IsActionPressed("Interact")) { ... }
//   float moveX = InputManager::Get().GetAxis("MoveX");
// ---------------------------------------------------------------------------

namespace dt
{
    // Maximum number of named actions supported. Sized to keep state arrays
    // small enough to fit in a cache line or two, while being large enough for
    // a full set of game bindings (move, camera, interact, UI, shortcuts...).
    static constexpr usize kMaxActions = 64;

    // Maximum simultaneous touch points tracked.
    static constexpr usize kMaxTouchPoints = 10;

    struct TouchPoint
    {
        SDL_FingerID id = 0;
        f32 x  = 0.0f;   // normalized [0, 1] screen position
        f32 y  = 0.0f;
        bool active = false;
    };

    class InputManager
    {
    public:
        static InputManager& Get();

        // Loads action definitions and bindings from a .ini file using
        // the same ConfigLayer parser as the rest of the engine.
        // [keyboard], [gamepad], [gamepad_axis], [touch] sections.
        // Safe to call multiple times (clears and reloads).
        void LoadBindings(const std::string& iniPath);

        // Register a single action definition at runtime (useful for engine
        // built-in actions like "Pause", "DebugToggle" that are not in the
        // user-facing input.ini).
        void RegisterAction(InputActionDef def);

        // Called once per platform frame, before SDL event processing.
        // Moves the "just pressed" / "just released" flags to the previous
        // frame's state so that IsActionPressed returns true for exactly one
        // frame after the key goes down.
        void BeginFrame();

        // Called once per platform frame, after all SDL events are processed.
        void EndFrame();

        // Feed a single SDL event. Call this for every event in the SDL
        // event loop. Handles SDL_EVENT_KEY_DOWN/UP, SDL_EVENT_GAMEPAD_*,
        // SDL_EVENT_FINGER_*, and SDL_EVENT_QUIT.
        void ProcessEvent(const SDL_Event& ev);

        // Opens all currently connected gamepads. Call once after SDL_Init.
        void OpenGamepads();

        // Closes all open gamepad handles. Call before SDL_Quit.
        void CloseGamepads();

        // --- Query API -------------------------------------------------------

        // Returns true for exactly one frame after the action transitions
        // from not-held to held.
        bool IsActionPressed(const std::string& name) const;

        // Returns true every frame the action is held.
        bool IsActionHeld(const std::string& name) const;

        // Returns true for exactly one frame after the action transitions
        // from held to not-held.
        bool IsActionReleased(const std::string& name) const;

        // Returns the current value of a named analog axis in [-1, 1].
        // For digital bindings mapped as axes (e.g. WASD), returns -1/0/+1.
        f32 GetAxis(const std::string& name) const;

        // Raw touch points (for custom touch UI that bypasses action mapping).
        const TouchPoint* GetTouchPoints() const { return m_touchPoints.data(); }
        usize             TouchPointCount()  const { return kMaxTouchPoints; }

        // Returns true if at least one gamepad is open.
        bool HasGamepad() const { return !m_gamepads.empty(); }

    private:
        InputManager() = default;

        // Looks up the action index for a name. Returns kMaxActions if not found.
        usize FindActionIndex(const std::string& name) const;

        // Sets an action's held state and manages pressed/released transitions.
        void SetActionHeld(usize idx, bool held);

        // Sets a named axis value directly (from gamepad analog sticks).
        void SetAxisValue(usize idx, f32 value);

        // Parse a key name string from .ini into an SDL keycode.
        static SDL_Keycode ParseKeycode(const std::string& name);

        // Parse a gamepad button name string from .ini.
        static SDL_GamepadButton ParseGamepadButton(const std::string& name);

        // Parse a gamepad axis name string from .ini.
        static SDL_GamepadAxis ParseGamepadAxis(const std::string& name);

        // --- State ---

        // Registered action definitions (max kMaxActions).
        std::vector<InputActionDef> m_actions;

        // Index lookup: action name -> slot index.
        std::unordered_map<std::string, usize> m_nameToIndex;

        // Current frame state (indexed by action slot).
        std::array<bool, kMaxActions> m_held        = {};
        std::array<bool, kMaxActions> m_pressed      = {};  // True for one frame
        std::array<bool, kMaxActions> m_released     = {};  // True for one frame
        std::array<f32,  kMaxActions> m_axisValues   = {};

        // Open gamepad handles (SDL3 uses SDL_Gamepad*).
        std::vector<SDL_Gamepad*> m_gamepads;

        // Touch state.
        std::array<TouchPoint, kMaxTouchPoints> m_touchPoints = {};
    };
}
