#pragma once
#include <chrono>
#include <initializer_list>
// Include the pinned public OpenVR header before this file. The native owner and
// controls should share this policy. Read-only queries, never Steam mutations.
namespace portal {
class SteamMenuVisibility {
    using Clock = std::chrono::steady_clock;
    Clock::time_point next{};
    bool visible = false;
public:
    bool active(vr::IVROverlay *api) {
        const auto now = Clock::now();
        if (now < next) return visible;
        next = now + std::chrono::milliseconds(50);
        visible = api->IsDashboardVisible();
        // Frame's standalone Steam menu is Basic, not Dashboard_Main.
        // system.systemui and gamepadui.bar remain visible with the menu shut;
        // deliberately never use those as suppression signals.
        // The observed Frame menu key must be rechecked after SteamOS updates.
        for (const char *key : {"valve.steam.gamepadui.main",
                               "valve.steam.gamepadui.frame.menu.55880005"}) {
            vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
            if (api->FindOverlay(key, &handle) == vr::VROverlayError_None &&
                handle != vr::k_ulOverlayHandleInvalid && api->IsOverlayVisible(handle)) visible = true;
        }
        return visible;
    }
};
}
