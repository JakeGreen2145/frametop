// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace ft::portal {
struct Vec3 {
    double x{}, y{}, z{};
};
// Row-major memory, column-vector multiplication; metres, right-handed coordinates.
struct Mat4 {
    std::array<double, 16> v{};
    static Mat4 identity();
};
struct Box {
    Vec3 center{}, size{1, 1, 1};
    std::array<double, 4> quaternion{0, 0, 0, 1};
};
struct Rectangle {
    Vec3 center, u, v;
};
struct alignas(16) Parameters {
    std::array<float, 4> q0{}, q1{}, q2{};
    float opacity{};
    std::uint32_t epoch_low{}, epoch_high{}, flags{};
};
static_assert(sizeof(Parameters) == 64);
static_assert(offsetof(Parameters, opacity) == 48 && offsetof(Parameters, flags) == 60);
struct Draw {
    Box box;
    Mat4 raw_to_standing = Mat4::identity();
    Mat4 raw_to_eye = Mat4::identity();
    Mat4 projection{};
    bool tracking_valid = false;
    bool one_sided = false;
    double minimum_plane_distance = 0.01; // numerical exclusion, not a comfort threshold
    double opacity = 1;
    std::uint64_t epoch = 0;
};
enum class Error {
    none,
    tracking,
    geometry,
    origin,
    eye,
    projection,
    near_plane,
    facing,
    opacity,
    numeric
};
struct Result {
    Error error = Error::geometry;
    Parameters parameters{};
    explicit operator bool() const { return error == Error::none; }
};
bool top_face(const Box &, Rectangle &) noexcept;
Result compute(const Draw &) noexcept;
Vec3 evaluate(const Parameters &, Vec3 raw_point) noexcept;
bool contains(Vec3 homogeneous_q) noexcept;
using NativeConstants = std::array<std::byte, 528>;
using PortalConstants = std::array<std::byte, 592>;
// Copy native bytes verbatim. Invalid results serialize a zero, invalid extension.
PortalConstants snapshot(const NativeConstants &, const Result &) noexcept;
} // namespace ft::portal
