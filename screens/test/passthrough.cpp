// No SteamVR connection: exercise the desk volume's real geometry/control code.
#include <cstdio>
#include "../passthrough.h"
#include <cassert>
#include <limits>
#include <cmath>
#include <set>
#include <string>

void Geometry() {
    passthrough::Volume v;
    v.enabled = true;
    v.center = {2, 3, -4};
    v.size = {1, 2, 3};
    v.yaw = 90;
    const auto edges = passthrough::Edges(v);
    std::set<std::string> segments;
    int corners[8] = {};
    for (const auto &e : edges) {
        auto a = passthrough::ToLocal(v, e.a), b = passthrough::ToLocal(v, e.b);
        int changed = 0, ca = 0, cb = 0;
        for (int i = 0; i < 3; ++i) {
            assert(std::fabs(std::fabs(a[i]) - v.size[i] / 2) < 1e-9);
            assert(std::fabs(std::fabs(b[i]) - v.size[i] / 2) < 1e-9);
            changed += std::fabs(a[i] - b[i]) > 1e-9;
            ca |= (a[i] > 0) << i, cb |= (b[i] > 0) << i;
        }
        assert(changed == 1);
        ++corners[ca], ++corners[cb];
        segments.insert(std::to_string(std::min(ca, cb)) + ":" + std::to_string(std::max(ca, cb)));
    }
    assert(segments.size() == 12);
    for (int c : corners) assert(c == 3);
    const auto faces = passthrough::FaceCenters(v);
    for (int f = 0; f < 6; ++f) {
        const auto p = passthrough::ToLocal(v, faces[f]);
        for (int i = 0; i < 3; ++i)
            assert(std::fabs(p[i] - (i == f / 2 ? (f % 2 ? 1 : -1) * v.size[i] / 2 : 0)) < 1e-9);
    }
    std::puts("volume geometry: 12 unique yawed edges, 8 degree-3 corners, 6 face centers");
}

void ResizeFaces() {
    for (double yaw : {0., 37., 90., -155.})
        for (int face = 0; face < 6; ++face) {
            passthrough::Volume v;
            v.enabled = true;
            v.center = {2, 3, -4};
            v.size = {1, 2, 3};
            v.yaw = yaw;
            const auto before = v;
            const auto opposite = passthrough::FaceCenters(v)[face ^ 1];
            const double sign = face % 2 ? 1 : -1;
            assert(passthrough::ResizeFace(v, before, face, sign * 0.4));
            assert(std::fabs(v.size[face / 2] - before.size[face / 2] - 0.4) < 1e-9);
            const auto after = passthrough::FaceCenters(v)[face ^ 1];
            for (int i = 0; i < 3; ++i) assert(std::fabs(after[i] - opposite[i]) < 1e-9);
            assert(v.yaw == yaw && v.enabled);
            assert(passthrough::ResizeFace(v, before, face, sign * 100));
            assert(std::fabs(v.size[face / 2] - 5) < 1e-9 && passthrough::Valid(v));
            assert(passthrough::ResizeFace(v, before, face, -sign * 100));
            assert(std::fabs(v.size[face / 2] - 0.05) < 1e-9 && passthrough::Valid(v));
        }
    passthrough::Volume v;
    v.enabled = true;
    v.center = {99.9, 0, 0};
    const auto before = v;
    assert(passthrough::ResizeFace(v, before, 1, 5));
    assert(passthrough::Valid(v) && std::fabs(v.center[0] - 100) < 1e-9);
    assert(std::fabs(v.size[0] - 1.2) < 1e-9);
    const auto good = v;
    assert(!passthrough::ResizeFace(v, before, 6, 1));
    assert(!passthrough::ResizeFace(v, before, 0, std::numeric_limits<double>::quiet_NaN()));
    assert(v.center == good.center && v.size == good.size);
    std::puts("volume resize: all six faces/yaws anchored, dimension and center clamps, invalid edits atomic");
}

void DragFaces() {
    for (int face = 0; face < 6; ++face) {
        passthrough::Volume v;
        v.enabled = true;
        v.yaw = 37;
        const auto start = v;
        const auto handle = passthrough::FaceCenters(v)[face];
        passthrough::Point local{};
        local[face / 2] = 1;
        const auto axisPoint = passthrough::ToWorld(v, local);
        passthrough::Point axis{}, direction{}, origin{};
        // Oblique ray: each face can be resized by pointing along a plane containing its axis.
        for (int i = 0; i < 3; ++i) axis[i] = axisPoint[i] - v.center[i];
        local = {};
        local[(face / 2 + 1) % 3] = 1;
        const auto side = passthrough::ToWorld(v, local);
        for (int i = 0; i < 3; ++i) {
            direction[i] = (axis[i] + side[i] - v.center[i]) / std::sqrt(2.);
            origin[i] = handle[i] - direction[i];
        }
        passthrough::ResizeDrag drag;
        assert(drag.Begin(v, face, 7, {origin, direction}));
        assert(drag.Active() && drag.Face() == face && drag.Device() == 7);
        assert(drag.Update(v, {origin, direction}));
        assert(v.center == start.center && v.size == start.size); // no jump at grab
        const double delta = (face % 2 ? 1 : -1) * 0.4;
        for (int i = 0; i < 3; ++i) origin[i] += axis[i] * delta;
        assert(drag.Update(v, {origin, direction}));
        assert(std::fabs(v.size[face / 2] - start.size[face / 2] - 0.4) < 1e-9);
        assert(!drag.Finish(v, 8) && drag.Active()); // unrelated device cannot end it
        assert(drag.Finish(v, 7) && !drag.Active());
        assert(!drag.Finish(v, 7));
    }
    std::puts("volume drag: six yawed laser axes, stable grab offset, device-owned changed finish");
}

void DragSafety() {
    passthrough::Volume v;
    v.enabled = true;
    const auto start = v;
    const auto face = passthrough::FaceCenters(v)[5];
    // Axis-on grab stays finite; moving the controller/cursor pushes the face.
    passthrough::Ray ray{{face[0], face[1], face[2] + 1}, {0, 0, -2}};
    passthrough::ResizeDrag drag;
    assert(drag.Begin(v, 5, 7, ray));
    assert(!drag.Begin(v, 1, 8, ray)); // no hijack
    ray.origin[2] += 0.3;
    assert(drag.Update(v, ray));
    assert(std::fabs(v.size[2] - start.size[2] - 0.3) < 1e-9);
    const auto changed = v;
    auto bad = ray;
    bad.direction[0] = std::numeric_limits<double>::infinity();
    assert(!drag.Update(v, bad));
    assert(v.center == changed.center && v.size == changed.size);
    drag.Cancel(v); // tracking lost/cancelled, no persistence
    assert(!drag.Active() && v.center == start.center && v.size == start.size);
    assert(!drag.Finish(v, 7));
    assert(drag.Begin(v, 5, 7, ray));
    char reply[256];
    assert(passthrough::Command("passthrough state", v, reply, sizeof reply, &drag));
    assert(drag.Active());
    assert(passthrough::Command("passthrough set 1 0 0 0 0 0.3 1 0", v, reply, sizeof reply, &drag));
    assert(drag.Active()); // rejected command does not interfere
    assert(passthrough::Command("passthrough set 0 1 2 3 1 2 3 90", v, reply, sizeof reply, &drag));
    assert(!drag.Active() && !v.enabled && v.center == (passthrough::Point{1, 2, 3}));
    assert(!drag.Update(v, ray) && !drag.Begin(v, 5, 7, ray)); // disabled
    v.enabled = true;
    bad.direction = {0, 0, 0};
    assert(!drag.Begin(v, 5, 7, bad));
    std::puts("volume drag safety: axial push/pull, invalid rays, rollback, external set cancels, disabled");
}

void AxialMouse() {
    for (int face = 0; face < 6; ++face) {
        passthrough::Volume v;
        v.enabled = true;
        const auto start = v;
        const auto point = passthrough::FaceCenters(v)[face];
        passthrough::Point origin = point, direction{};
        origin[face / 2] += 1;
        direction[face / 2] = -1;
        passthrough::ResizeDrag drag;
        assert(drag.Begin(v, face, 4, {origin, direction}));
        // Looking straight down a resize axis must still respond to a 3D mouse's
        // angular/tangent motion. Drag up (sideways for the vertical axis) grows it.
        origin[face / 2 == 1 ? 0 : 1] += 0.2;
        assert(drag.Update(v, {origin, direction}));
        assert(std::fabs(v.size[face / 2] - start.size[face / 2] - 0.2) < 1e-9);
    }
    std::puts("volume axial mouse: tangential laser/cursor motion resizes all six end-on faces");
}

int main() {
    Geometry();
    ResizeFaces();
    DragFaces();
    DragSafety();
    AxialMouse();
    passthrough::Volume v;
    v.enabled = true;
    v.center = {0, 0, -1};
    v.size = {1, 0.5, 0.5};
    assert(passthrough::Occludes(v, {0, 0, 0}, {0, 0, -2}));
    v.enabled = false;
    assert(!passthrough::Occludes(v, {0, 0, 0}, {0, 0, -2}));
    v.enabled = true;
    assert(!passthrough::Occludes(v, {0, 0, 0}, {2, 0, -2}));
    assert(!passthrough::Occludes(v, {0, 0, 0}, {0, 2, -2}));
    assert(!passthrough::Occludes(v, {0, 0, 0}, {0, 0, -0.5}));  // behind the screen
    assert(!passthrough::Occludes(v, {0, 0, -2}, {0, 0, -3}));  // behind the eye
    assert(passthrough::Occludes(v, {0, 0, -1}, {0, 0, -2}));   // eye inside
    assert(passthrough::Occludes(v, {0, 0, 0}, {0, 0, -1}));    // screen inside
    v.size[0] = std::numeric_limits<double>::quiet_NaN();
    assert(!passthrough::Occludes(v, {0, 0, 0}, {0, 0, -2}));
    std::puts("passthrough: desk volume cuts a ray before its screen");
}
