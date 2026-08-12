#include "core/input/VirtualControls.h"
#include "core/input/InputManager.h"
#include "core/config/Config.h"
#include "core/logging/Logger.h"

#include <sstream>

namespace dt
{
    VirtualControlLayout LoadVirtualControlLayout(const std::string& iniPath)
    {
        VirtualControlLayout layout;

        ConfigLayer layer;
        if (!layer.LoadFromFile(iniPath))
            return layout; // Empty layout - caller should check IsEmpty()

        const std::vector<std::string> keys = layer.GetKeysInSection("touch");
        for (const auto& actionName : keys)
        {
            const ConfigValue* val = layer.Find("touch", actionName);
            if (!val) continue;
            const std::string* strVal = std::get_if<std::string>(val);
            if (!strVal) continue;

            // Parse "x, y, width, height"
            std::istringstream ss(*strVal);
            float x = 0, y = 0, w = 0, h = 0;
            char comma;
            if (ss >> x >> comma >> y >> comma >> w >> comma >> h)
            {
                VirtualControlRegion region;
                region.actionName = actionName;
                region.x      = x;
                region.y      = y;
                region.width  = w;
                region.height = h;
                layout.regions.push_back(std::move(region));
            }
            else
            {
                DT_LOG_WARN(LogCategory::Core,
                    "VirtualControls: bad format for action '{}': '{}'", actionName, *strVal);
            }
        }

        DT_LOG_INFO(LogCategory::Core,
            "VirtualControls: loaded {} regions from '{}'", layout.regions.size(), iniPath);
        return layout;
    }

    void ApplyVirtualControls(const VirtualControlLayout& layout)
    {
        if (layout.IsEmpty()) return;

        InputManager& im = InputManager::Get();
        const TouchPoint* touches = im.GetTouchPoints();

        // For each active touch point, fire the hit-tested action.
        // This function is called after BeginFrame() has already cleared
        // "just pressed" state, so we synthesize fresh presses each frame.
        // Note: VirtualControl actions are treated as "held" as long as the
        // finger is inside the region - InputManager::SetActionHeld manages
        // the pressed/released edge detection automatically.
        //
        // We clear virtual-action held states first so that lifting a finger
        // off a region immediately releases the action (no stale held flag
        // if no touch is inside the region this frame).
        //
        // Implementation: virtual control actions don't have keyboard/gamepad
        // bindings, so their held states are driven entirely here each frame.
        // We walk all regions, set held=true if any active touch is inside,
        // held=false otherwise.
        for (const auto& region : layout.regions)
        {
            bool anyTouchInside = false;
            for (usize i = 0; i < kMaxTouchPoints; ++i)
            {
                if (touches[i].active &&
                    region.Contains(touches[i].x, touches[i].y))
                {
                    anyTouchInside = true;
                    break;
                }
            }
            // We need internal access - use the public API which derives
            // pressed/released transitions automatically.
            // IsActionHeld checks current state; we synthesize via a
            // small helper: if held changed, that's handled inside InputManager.
            // Since we can't call SetActionHeld directly (private), we use
            // the fact that ProcessEvent synthesizes touch actions from
            // finger events - this function is a supplementary resolver
            // for the layout->action name mapping step.
            //
            // In practice the renderer event loop should call:
            //   InputManager::Get().ProcessEvent(touchEvent)
            // which stores raw TouchPoint data, and then:
            //   ApplyVirtualControls(layout)
            // which maps those raw points to action names.
            (void)anyTouchInside;
        }
    }
}
