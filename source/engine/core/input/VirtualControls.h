#pragma once

#include "core/platform/Types.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// VirtualControls.h
//
// Screenspace virtual button/DPad layout for touch input on mobile (Android,
// iOS) or any platform where touch is the primary input device.
//
// A VirtualControlLayout defines a set of rectangular regions on the screen.
// When a touch point falls inside a region, the associated action name is
// synthesized as a pressed action in InputManager, making it transparent to
// all game logic (scripts poll IsActionPressed("Jump") the same way whether
// the player pressed Space, a gamepad button, or a virtual button on screen).
//
// Layout values are in normalized screen coordinates [0, 1] where:
//   (0, 0) = top-left corner
//   (1, 1) = bottom-right corner
//
// These are loaded from [touch] in input.ini. Example:
//   [touch]
//   ; action = x, y, width, height  (all normalized 0-1)
//   Jump     = 0.80, 0.75, 0.10, 0.18
//   Interact = 0.90, 0.55, 0.10, 0.18
//   DPadUp   = 0.12, 0.60, 0.08, 0.10
//   DPadDown = 0.12, 0.78, 0.08, 0.10
//   DPadLeft = 0.04, 0.70, 0.08, 0.10
//   DPadRight= 0.20, 0.70, 0.08, 0.10
// ---------------------------------------------------------------------------

namespace dt
{
    // One on-screen control region mapped to a named action.
    struct VirtualControlRegion
    {
        std::string actionName;   // Must match a registered InputManager action
        f32 x      = 0.0f;       // Normalized left edge [0, 1]
        f32 y      = 0.0f;       // Normalized top edge  [0, 1]
        f32 width  = 0.0f;
        f32 height = 0.0f;

        bool Contains(f32 nx, f32 ny) const
        {
            return nx >= x && nx <= (x + width)
                && ny >= y && ny <= (y + height);
        }
    };

    // Full layout of all virtual control regions for one platform/orientation.
    struct VirtualControlLayout
    {
        std::vector<VirtualControlRegion> regions;

        // Returns the action name for a normalized touch coordinate,
        // or an empty string if the touch is outside all regions.
        std::string HitTest(f32 nx, f32 ny) const
        {
            for (const auto& r : regions)
            {
                if (r.Contains(nx, ny))
                    return r.actionName;
            }
            return {};
        }

        bool IsEmpty() const { return regions.empty(); }
    };

    // Loads a VirtualControlLayout from the [touch] section of an ini file.
    // Returns an empty layout (no regions) if the section is absent.
    VirtualControlLayout LoadVirtualControlLayout(const std::string& iniPath);

    // Updates InputManager's action states based on current touch points and
    // the provided layout. Call once per frame after InputManager::BeginFrame()
    // and SDL event processing.
    void ApplyVirtualControls(const VirtualControlLayout& layout);
}
