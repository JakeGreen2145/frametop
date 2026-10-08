// Pure CPU translation and opt-in control geometry. No OpenVR headers or runtime.
#include "../passthrough.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

void Same(const passthrough::Volume &a, const passthrough::Volume &b) {
    assert(a.center == b.center && a.size == b.size && a.yaw == b.yaw && a.enabled == b.enabled);
}
int main() {
    using namespace passthrough;
    for (double yaw : {0., 37., 90., -155.}) {
        Volume v;
        v.yaw = yaw;
        const auto start = v;
        const auto bar = GrabCenter(v);
        const auto local = ToLocal(v, bar);
        assert(std::fabs(local[0]) < 1e-9);
        assert(std::fabs(local[1] - v.size[1] / 2) < 1e-9);
        assert(std::fabs(local[2] - v.size[2] / 2) < 1e-9);
        // Off-center press, non-unit ray, no snap to the bar's center.
        Ray ray{{bar[0] + .07, bar[1], bar[2] + 1}, {0, 0, -2}};
        TranslateDrag drag;
        assert(!drag.Begin(v, 4, ray)); // legacy disabled gate is retained
        assert(drag.Begin(v, 4, ray, true)); // controls can edit disabled alpha volume
        assert(drag.Active() && drag.Device() == 4);
        assert(!drag.Begin(v, 3, ray, true));
        assert(drag.Update(v, ray)); Same(v, start);
        ray.origin[0] += .2; ray.origin[1] -= .3; ray.origin[2] += .4;
        assert(drag.Update(v, ray));
        for (int k = 0; k < 3; ++k)
            assert(std::fabs(v.center[k] - start.center[k] - Point{.2, -.3, .4}[k]) < 1e-9);
        assert(v.size == start.size && v.yaw == yaw && !v.enabled);
        const auto moved = v;
        for (int i = 0; i < 100; ++i) assert(drag.Update(v, ray));
        Same(v, moved); // evaluate from press, not incremental drift
        assert(!drag.Finish(v, 3) && drag.Active());
        assert(drag.Finish(v, 4) && !drag.Active());
        assert(!drag.Finish(v, 4));
        assert(drag.Begin(v, 4, ray, true));
        ray.origin[2] -= .2;
        assert(drag.Update(v, ray));
        drag.Cancel(v); Same(v, moved);
        assert(!drag.Active());
    }
    Volume v; v.enabled = true;
    auto start = v;
    const auto bar = GrabCenter(v);
    Ray ray{{bar[0], bar[1], bar[2] + 1}, {0, 0, -1}};
    TranslateDrag drag;
    assert(drag.Begin(v, 4, ray));
    // Mouse angular motion follows a fixed-distance ray point, desk stays level.
    ray.direction = {.3, .4, -1};
    assert(drag.Update(v, ray));
    assert(v.center[0] > start.center[0] && v.center[1] > start.center[1]);
    assert(v.size == start.size && v.yaw == start.yaw);
    auto before = v, bad = v;
    auto invalid = ray; invalid.direction[0] = std::numeric_limits<double>::infinity();
    assert(!drag.Update(v, invalid)); Same(v, before);
    invalid.direction = {0, 0, 0};
    assert(!drag.Update(v, invalid)); Same(v, before);
    ray.origin = {1000, -1000, 1000};
    assert(drag.Update(v, ray));
    assert(Valid(v) && v.center == (Point{100, -100, 100}));
    drag.Cancel(v); Same(v, start);
    assert(drag.Begin(v, 4, ray)); // bar is ahead of this negative-Z ray
    char reply[256];
    assert(Command("passthrough state", v, reply, sizeof reply, nullptr, &drag) && drag.Active());
    assert(Command("passthrough set 1 0 0 0 0 1 1 0", v, reply, sizeof reply, nullptr, &drag) && drag.Active());
    assert(Command("passthrough set 0 1 2 3 .05 5 2 90", v, reply, sizeof reply, nullptr, &drag));
    assert(!drag.Active() && v.center == (Point{1, 2, 3}) && !v.enabled);
    assert(!drag.Update(v, ray));
    before = v;
    drag.Cancel(v); Same(v, before); // external edit cannot be rolled back
    bad = v; bad.center[0] = 101;
    assert(!drag.Begin(bad, 4, ray, true));
    bad = v; bad.size[0] = .049;
    assert(!drag.Begin(bad, 4, ray, true));
    bad = v; bad.size[1] = 5.001;
    assert(!drag.Begin(bad, 4, ray, true));
    bad = v; bad.yaw = std::numeric_limits<double>::quiet_NaN();
    assert(!drag.Begin(bad, 4, ray, true));
    ResizeDrag resize;
    const auto face = FaceCenters(v)[1];
    ray = {{face[0], face[1], face[2] + 1}, {0, 0, -1}};
    assert(!resize.Begin(v, 1, 4, ray));
    assert(resize.Begin(v, 1, 4, ray, true));
    assert(resize.Update(v, ray)); Same(v, before);
    std::puts("volume translation: near/top grab anchor, no jump/drift, push/pull, angular motion, limits, ownership, rollback, external set");
}
