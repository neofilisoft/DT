#include "core/input/InputManager.h"
#include "core/config/Config.h"
#include "core/logging/Logger.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace dt
{
    // -------------------------------------------------------------------------
    // String helpers
    // -------------------------------------------------------------------------

    static std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // -------------------------------------------------------------------------
    // Singleton
    // -------------------------------------------------------------------------

    InputManager& InputManager::Get()
    {
        static InputManager instance;
        return instance;
    }

    // -------------------------------------------------------------------------
    // LoadBindings
    // -------------------------------------------------------------------------

    void InputManager::LoadBindings(const std::string& iniPath)
    {
        m_actions.clear();
        m_nameToIndex.clear();

        ConfigLayer layer;
        if (!layer.LoadFromFile(iniPath))
        {
            DT_LOG_WARN(LogCategory::Core,
                "InputManager: could not load bindings from '{}', using engine defaults", iniPath);
            // Register essential built-in fallbacks so the engine stays usable.
            RegisterAction({ "Pause",    { { InputDeviceType::Keyboard, InputBindingKind::Digital, SDLK_ESCAPE } } });
            RegisterAction({ "SpeedUp",  { { InputDeviceType::Keyboard, InputBindingKind::Digital, SDLK_RSHIFT } } });
            return;
        }

        // ---- [keyboard] section ----
        for (const auto& key : layer.GetKeysInSection("keyboard"))
        {
            const ConfigValue* val = layer.Find("keyboard", key);
            if (!val) continue;
            const std::string* strVal = std::get_if<std::string>(val);
            if (!strVal) continue;

            SDL_Keycode kc = ParseKeycode(*strVal);
            if (kc == SDLK_UNKNOWN)
            {
                DT_LOG_WARN(LogCategory::Core,
                    "InputManager: unknown key name '{}' for action '{}'", *strVal, key);
                continue;
            }

            usize idx = FindActionIndex(key);
            if (idx == kMaxActions)
            {
                RegisterAction({ key, {}, false });
                idx = m_actions.size() - 1;
            }
            m_actions[idx].bindings.push_back(
                { InputDeviceType::Keyboard, InputBindingKind::Digital, static_cast<i32>(kc) });
        }

        // ---- [gamepad] section (digital buttons) ----
        for (const auto& key : layer.GetKeysInSection("gamepad"))
        {
            const ConfigValue* val = layer.Find("gamepad", key);
            if (!val) continue;
            const std::string* strVal = std::get_if<std::string>(val);
            if (!strVal) continue;

            SDL_GamepadButton btn = ParseGamepadButton(*strVal);
            if (btn == SDL_GAMEPAD_BUTTON_INVALID)
            {
                DT_LOG_WARN(LogCategory::Core,
                    "InputManager: unknown gamepad button '{}' for action '{}'", *strVal, key);
                continue;
            }

            usize idx = FindActionIndex(key);
            if (idx == kMaxActions)
            {
                RegisterAction({ key, {}, false });
                idx = m_actions.size() - 1;
            }
            m_actions[idx].bindings.push_back(
                { InputDeviceType::Gamepad, InputBindingKind::Digital, static_cast<i32>(btn) });
        }

        // ---- [gamepad_axis] section (analog sticks mapped to axis actions) ----
        for (const auto& key : layer.GetKeysInSection("gamepad_axis"))
        {
            const ConfigValue* val = layer.Find("gamepad_axis", key);
            if (!val) continue;
            const std::string* strVal = std::get_if<std::string>(val);
            if (!strVal) continue;

            SDL_GamepadAxis axis = ParseGamepadAxis(*strVal);
            if (axis == SDL_GAMEPAD_AXIS_INVALID)
            {
                DT_LOG_WARN(LogCategory::Core,
                    "InputManager: unknown gamepad axis '{}' for action '{}'", *strVal, key);
                continue;
            }

            usize idx = FindActionIndex(key);
            if (idx == kMaxActions)
            {
                RegisterAction({ key, {}, true });
                idx = m_actions.size() - 1;
            }
            m_actions[idx].isAxis = true;
            m_actions[idx].bindings.push_back(
                { InputDeviceType::Gamepad, InputBindingKind::Axis, static_cast<i32>(axis), 1.0f });
        }

        DT_LOG_INFO(LogCategory::Core,
            "InputManager: loaded {} actions from '{}'", m_actions.size(), iniPath);
    }

    void InputManager::RegisterAction(InputActionDef def)
    {
        if (m_actions.size() >= kMaxActions)
        {
            DT_LOG_WARN(LogCategory::Core,
                "InputManager: action limit reached, cannot register '{}'", def.name);
            return;
        }
        m_nameToIndex[def.name] = m_actions.size();
        m_actions.push_back(std::move(def));
    }

    // -------------------------------------------------------------------------
    // Frame management
    // -------------------------------------------------------------------------

    void InputManager::BeginFrame()
    {
        m_pressed.fill(false);
        m_released.fill(false);
    }

    void InputManager::EndFrame()
    {
        // Nothing needed here currently; reserved for future per-frame cleanup
        // (e.g. clearing one-shot virtual touch events).
    }

    // -------------------------------------------------------------------------
    // Event processing
    // -------------------------------------------------------------------------

    void InputManager::ProcessEvent(const SDL_Event& ev)
    {
        switch (ev.type)
        {
            // --- Keyboard ---
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                if (ev.key.repeat) break; // Ignore key repeat events

                bool held = (ev.type == SDL_EVENT_KEY_DOWN);
                for (usize i = 0; i < m_actions.size(); ++i)
                {
                    for (const auto& b : m_actions[i].bindings)
                    {
                        if (b.device == InputDeviceType::Keyboard &&
                            b.kind   == InputBindingKind::Digital &&
                            b.code   == ev.key.key)
                        {
                            SetActionHeld(i, held);
                        }
                    }
                }
                break;
            }

            // --- Gamepad digital buttons ---
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
            {
                bool held = (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                for (usize i = 0; i < m_actions.size(); ++i)
                {
                    for (const auto& b : m_actions[i].bindings)
                    {
                        if (b.device == InputDeviceType::Gamepad &&
                            b.kind   == InputBindingKind::Digital &&
                            b.code   == static_cast<i32>(ev.gbutton.button))
                        {
                            SetActionHeld(i, held);
                        }
                    }
                }
                break;
            }

            // --- Gamepad analog axes ---
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            {
                for (usize i = 0; i < m_actions.size(); ++i)
                {
                    for (const auto& b : m_actions[i].bindings)
                    {
                        if (b.device == InputDeviceType::Gamepad &&
                            b.kind   == InputBindingKind::Axis &&
                            b.code   == static_cast<i32>(ev.gaxis.axis))
                        {
                            // SDL axis range is -32768 to 32767; normalize to [-1, 1]
                            f32 raw = static_cast<f32>(ev.gaxis.value) / 32767.0f;
                            // Apply deadzone
                            f32 val = (std::abs(raw) < b.deadzone) ? 0.0f : raw * b.axisScale;
                            SetAxisValue(i, val);
                        }
                    }
                }
                break;
            }

            // --- Gamepad connect/disconnect ---
            case SDL_EVENT_GAMEPAD_ADDED:
            {
                SDL_Gamepad* pad = SDL_OpenGamepad(ev.gdevice.which);
                if (pad)
                {
                    m_gamepads.push_back(pad);
                    DT_LOG_INFO(LogCategory::Core, "InputManager: gamepad connected ({})",
                        SDL_GetGamepadName(pad) ? SDL_GetGamepadName(pad) : "unknown");
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_REMOVED:
            {
                for (auto it = m_gamepads.begin(); it != m_gamepads.end(); ++it)
                {
                    if (SDL_GetGamepadID(*it) == ev.gdevice.which)
                    {
                        DT_LOG_INFO(LogCategory::Core, "InputManager: gamepad disconnected");
                        SDL_CloseGamepad(*it);
                        m_gamepads.erase(it);
                        break;
                    }
                }
                break;
            }

            // --- Touch / Virtual controls ---
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
            {
                bool active = (ev.type != SDL_EVENT_FINGER_UP);
                // Store raw touch point for VirtualControls to process.
                for (auto& tp : m_touchPoints)
                {
                    if (!tp.active || tp.id == ev.tfinger.fingerID)
                    {
                        tp.id     = ev.tfinger.fingerID;
                        tp.x      = ev.tfinger.x;
                        tp.y      = ev.tfinger.y;
                        tp.active = active;
                        break;
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    void InputManager::OpenGamepads()
    {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids)
        {
            for (int i = 0; i < count; ++i)
            {
                SDL_Gamepad* pad = SDL_OpenGamepad(ids[i]);
                if (pad)
                {
                    m_gamepads.push_back(pad);
                    DT_LOG_INFO(LogCategory::Core, "InputManager: opened gamepad '{}'",
                        SDL_GetGamepadName(pad) ? SDL_GetGamepadName(pad) : "unknown");
                }
            }
            SDL_free(ids);
        }
    }

    void InputManager::CloseGamepads()
    {
        for (SDL_Gamepad* pad : m_gamepads)
            SDL_CloseGamepad(pad);
        m_gamepads.clear();
    }

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    bool InputManager::IsActionPressed(const std::string& name) const
    {
        usize idx = FindActionIndex(name);
        return (idx < kMaxActions) ? m_pressed[idx] : false;
    }

    bool InputManager::IsActionHeld(const std::string& name) const
    {
        usize idx = FindActionIndex(name);
        return (idx < kMaxActions) ? m_held[idx] : false;
    }

    bool InputManager::IsActionReleased(const std::string& name) const
    {
        usize idx = FindActionIndex(name);
        return (idx < kMaxActions) ? m_released[idx] : false;
    }

    f32 InputManager::GetAxis(const std::string& name) const
    {
        usize idx = FindActionIndex(name);
        if (idx >= kMaxActions) return 0.0f;
        // If it's a digital action mapped as an axis, synthesize from held state.
        if (!m_actions[idx].isAxis)
            return m_held[idx] ? 1.0f : 0.0f;
        return m_axisValues[idx];
    }

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    usize InputManager::FindActionIndex(const std::string& name) const
    {
        auto it = m_nameToIndex.find(name);
        return (it != m_nameToIndex.end()) ? it->second : kMaxActions;
    }

    void InputManager::SetActionHeld(usize idx, bool held)
    {
        if (idx >= kMaxActions) return;
        if (held && !m_held[idx])
            m_pressed[idx] = true;
        if (!held && m_held[idx])
            m_released[idx] = true;
        m_held[idx] = held;
    }

    void InputManager::SetAxisValue(usize idx, f32 value)
    {
        if (idx < kMaxActions)
            m_axisValues[idx] = value;
    }

    // -------------------------------------------------------------------------
    // String -> SDL code parsers
    // -------------------------------------------------------------------------

    SDL_Keycode InputManager::ParseKeycode(const std::string& name)
    {
        // SDL3 provides SDL_GetKeyFromName() which handles all SDLK_ names.
        SDL_Keycode kc = SDL_GetKeyFromName(name.c_str());
        if (kc != SDLK_UNKNOWN) return kc;

        // Friendly aliases for common keys not recognized by SDL_GetKeyFromName.
        static const std::unordered_map<std::string, SDL_Keycode> kAliases = {
            { "space",    SDLK_SPACE     },
            { "enter",    SDLK_RETURN    },
            { "return",   SDLK_RETURN    },
            { "backspace",SDLK_BACKSPACE },
            { "tab",      SDLK_TAB       },
            { "escape",   SDLK_ESCAPE    },
            { "esc",      SDLK_ESCAPE    },
            { "shift",    SDLK_LSHIFT    },
            { "lshift",   SDLK_LSHIFT    },
            { "rshift",   SDLK_RSHIFT    },
            { "ctrl",     SDLK_LCTRL     },
            { "lctrl",    SDLK_LCTRL     },
            { "rctrl",    SDLK_RCTRL     },
            { "alt",      SDLK_LALT      },
            { "lalt",     SDLK_LALT      },
            { "ralt",     SDLK_RALT      },
            { "up",       SDLK_UP        },
            { "down",     SDLK_DOWN      },
            { "left",     SDLK_LEFT      },
            { "right",    SDLK_RIGHT     },
        };
        auto it = kAliases.find(ToLower(name));
        return (it != kAliases.end()) ? it->second : SDLK_UNKNOWN;
    }

    SDL_GamepadButton InputManager::ParseGamepadButton(const std::string& name)
    {
        static const std::unordered_map<std::string, SDL_GamepadButton> kMap = {
            { "buttonsouth",  SDL_GAMEPAD_BUTTON_SOUTH  },
            { "a",            SDL_GAMEPAD_BUTTON_SOUTH  },
            { "cross",        SDL_GAMEPAD_BUTTON_SOUTH  },
            { "buttoneast",   SDL_GAMEPAD_BUTTON_EAST   },
            { "b",            SDL_GAMEPAD_BUTTON_EAST   },
            { "circle",       SDL_GAMEPAD_BUTTON_EAST   },
            { "buttonwest",   SDL_GAMEPAD_BUTTON_WEST   },
            { "x",            SDL_GAMEPAD_BUTTON_WEST   },
            { "square",       SDL_GAMEPAD_BUTTON_WEST   },
            { "buttonnorth",  SDL_GAMEPAD_BUTTON_NORTH  },
            { "y",            SDL_GAMEPAD_BUTTON_NORTH  },
            { "triangle",     SDL_GAMEPAD_BUTTON_NORTH  },
            { "back",         SDL_GAMEPAD_BUTTON_BACK   },
            { "select",       SDL_GAMEPAD_BUTTON_BACK   },
            { "start",        SDL_GAMEPAD_BUTTON_START  },
            { "buttonstart",  SDL_GAMEPAD_BUTTON_START  },
            { "leftshoulder", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER  },
            { "lb",           SDL_GAMEPAD_BUTTON_LEFT_SHOULDER  },
            { "l1",           SDL_GAMEPAD_BUTTON_LEFT_SHOULDER  },
            { "rightshoulder",SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
            { "rb",           SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
            { "r1",           SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
            { "dpup",         SDL_GAMEPAD_BUTTON_DPAD_UP    },
            { "dpdown",       SDL_GAMEPAD_BUTTON_DPAD_DOWN  },
            { "dpleft",       SDL_GAMEPAD_BUTTON_DPAD_LEFT  },
            { "dpright",      SDL_GAMEPAD_BUTTON_DPAD_RIGHT },
        };
        auto it = kMap.find(ToLower(name));
        return (it != kMap.end()) ? it->second : SDL_GAMEPAD_BUTTON_INVALID;
    }

    SDL_GamepadAxis InputManager::ParseGamepadAxis(const std::string& name)
    {
        static const std::unordered_map<std::string, SDL_GamepadAxis> kMap = {
            { "leftstickx",    SDL_GAMEPAD_AXIS_LEFTX         },
            { "leftsticky",    SDL_GAMEPAD_AXIS_LEFTY         },
            { "rightstickx",   SDL_GAMEPAD_AXIS_RIGHTX        },
            { "rightsticky",   SDL_GAMEPAD_AXIS_RIGHTY        },
            { "lefttrigger",   SDL_GAMEPAD_AXIS_LEFT_TRIGGER  },
            { "lt",            SDL_GAMEPAD_AXIS_LEFT_TRIGGER  },
            { "l2",            SDL_GAMEPAD_AXIS_LEFT_TRIGGER  },
            { "righttrigger",  SDL_GAMEPAD_AXIS_RIGHT_TRIGGER },
            { "rt",            SDL_GAMEPAD_AXIS_RIGHT_TRIGGER },
            { "r2",            SDL_GAMEPAD_AXIS_RIGHT_TRIGGER },
        };
        auto it = kMap.find(ToLower(name));
        return (it != kMap.end()) ? it->second : SDL_GAMEPAD_AXIS_INVALID;
    }
}
