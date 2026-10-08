// A desk volume, fixed in OpenVR's standing space (metres, +Y up, -Z forward).
// Pure placement/manipulation only. enabled is legacy data, never camera intent.
#pragma once

#include <array>

namespace passthrough {
using Point = std::array<double, 3>;
struct Volume {
    bool enabled = false; // preserved legacy field; never camera On intent
    Point center{0, 0.75, -0.65};
    Point size{1, 0.35, 0.65};  // width, height, depth
    // Standing_from_volume = T(center) Ry(yaw) Rx(pitch) Rz(roll), degrees.
    double yaw = 0, pitch = 0, roll = 0;
};

using Pose = std::array<std::array<double, 4>, 3>;
Pose VolumePose(const Volume &v);
// Pure geometry for a future native owner. Not camera readiness or a renderer.
// Top center C + R*(0,h/2,0), basis R*[X,-Z,Y], width=x, length=z.
struct Plane { Pose pose; double width, length; };
Plane TopFace(const Volume &v);
bool ValidPose(const Pose &pose);
bool Valid(const Volume &v);
Point ToLocal(const Volume &v, const Point &p);
Point ToWorld(const Volume &v, const Point &p);
struct Edge { Point a, b; };
std::array<Edge, 12> Edges(const Volume &v);
// Faces: -X, +X, -Y, +Y, -Z, +Z, in the box's local frame.
std::array<Point, 6> FaceCenters(const Volume &v);
// Near-user (+local Z) edge, centered across width, on the top plane.
Point GrabCenter(const Volume &v);
// Delta is metres along the positive local axis from the initial dragged face.
// Opposite face stays fixed; dimensions and standing center limits clamp together.
bool ResizeFace(Volume &out, const Volume &start, int face, double delta);
struct Ray { Point origin, direction; };
// One device owns a drag. Cancellation rolls back; an external set simply discards it.
class ResizeDrag {
public:
    // Explicit opt-in for independent controls; legacy callers still require enabled.
    bool Begin(const Volume &v, int face, unsigned device, const Ray &ray, bool allowDisabled = false);
    bool Update(Volume &v, const Ray &ray);
    bool Finish(const Volume &v, unsigned device);
    void Cancel(Volume &v);
    void Reset() { face_ = -1; }
    bool Active() const { return face_ >= 0; }
    int Face() const { return face_; }
    unsigned Device() const { return device_; }
private:
    bool Coordinate(const Ray &ray, double &value) const;
    Volume start_;
    Point axis_{}, point_{}, normal_{}, gesture_{};
    double distance_ = 0, grab_ = 0;
    int face_ = -1;
    unsigned device_ = 0;
};
// Drag ownership/rollback plus legacy ray translation. Native grab uses UpdatePose
// with the desktop carry transform device_now * inverse(device_press) * volume_press.
class TranslateDrag {
public:
    bool UpdatePose(Volume &v, const Pose &pose);
    bool Begin(const Volume &v, unsigned device, const Ray &ray, bool allowDisabled = false);
    bool Update(Volume &v, const Ray &ray);
    bool Finish(const Volume &v, unsigned device);
    void Cancel(Volume &v);
    void Reset() { active_ = false; }
    bool Active() const { return active_; }
    unsigned Device() const { return device_; }
private:
    bool Anchor(const Ray &ray, Point &point) const;
    Volume start_;
    Point grab_{};
    double distance_ = 0;
    unsigned device_ = 0;
    bool active_ = false;
};
// Legacy pure ray/box query only. No screen renderer calls it in the native port.
bool Occludes(const Volume &v, const Point &eye, const Point &screen);
// Returns false for another subsystem's command. State is safe to query with --no-vr.
bool Command(const char *cmd, Volume &v, char *reply, int size, ResizeDrag *drag = nullptr,
             TranslateDrag *translation = nullptr);
}  // namespace passthrough
