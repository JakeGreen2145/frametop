// SPDX-License-Identifier: MIT
#include "ft_aperture.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace ft::portal {
namespace {
constexpr double tolerance = 1e-6;
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(Vec3 a, double b) { return {a.x * b, a.y * b, a.z * b}; }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(Vec3 a) { return std::hypot(a.x, a.y, a.z); }
bool finite(Vec3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
bool finite(const Mat4 &m) {
    return std::all_of(m.v.begin(), m.v.end(), [](double v) { return std::isfinite(v); });
}
bool affine(const Mat4 &m) {
    return finite(m) && std::abs(m.v[12]) <= tolerance && std::abs(m.v[13]) <= tolerance &&
           std::abs(m.v[14]) <= tolerance && std::abs(m.v[15] - 1) <= tolerance;
}
Vec3 transform(const Mat4 &m, Vec3 p, double w) {
    return {m.v[0] * p.x + m.v[1] * p.y + m.v[2] * p.z + m.v[3] * w,
            m.v[4] * p.x + m.v[5] * p.y + m.v[6] * p.z + m.v[7] * w,
            m.v[8] * p.x + m.v[9] * p.y + m.v[10] * p.z + m.v[11] * w};
}
// Scaled pivoting rejects singular/ill-conditioned inputs, not just rigid matrices.
bool inverse(const Mat4 &m, Mat4 &out) {
    if (!finite(m))
        return false;
    double a[4][8]{}, row_scale[4]{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            a[i][j] = m.v[i * 4 + j];
            row_scale[i] = std::max(row_scale[i], std::abs(a[i][j]));
        }
        a[i][i + 4] = 1;
        if (row_scale[i] == 0)
            return false;
    }
    for (int j = 0; j < 4; ++j) {
        int pivot = j;
        for (int i = j + 1; i < 4; ++i)
            if (std::abs(a[i][j]) / row_scale[i] > std::abs(a[pivot][j]) / row_scale[pivot])
                pivot = i;
        if (std::abs(a[pivot][j]) / row_scale[pivot] < 1e-12)
            return false;
        for (int k = 0; k < 8; ++k)
            std::swap(a[pivot][k], a[j][k]);
        std::swap(row_scale[pivot], row_scale[j]);
        double divisor = a[j][j];
        for (double &v : a[j])
            v /= divisor;
        for (int i = 0; i < 4; ++i)
            if (i != j) {
                double factor = a[i][j];
                for (int k = 0; k < 8; ++k)
                    a[i][k] -= factor * a[j][k];
            }
    }
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out.v[i * 4 + j] = a[i][j + 4];
    return finite(out);
}
bool rigid(const Mat4 &m) {
    if (!affine(m))
        return false;
    Vec3 a{m.v[0], m.v[4], m.v[8]}, b{m.v[1], m.v[5], m.v[9]}, c{m.v[2], m.v[6], m.v[10]};
    return std::abs(dot(a, a) - 1) <= tolerance && std::abs(dot(b, b) - 1) <= tolerance &&
           std::abs(dot(c, c) - 1) <= tolerance && std::abs(dot(a, b)) <= tolerance &&
           std::abs(dot(a, c)) <= tolerance && std::abs(dot(b, c)) <= tolerance &&
           std::abs(dot(cross(a, b), c) - 1) <= tolerance;
}
bool central_projection(const Mat4 &p) {
    if (!finite(p))
        return false;
    double magnitude = 0;
    for (double v : p.v)
        magnitude = std::max(magnitude, std::abs(v));
    if (magnitude == 0)
        return false;
    // Eye origin must map to (0,0,z,0), with nonzero z and full projective rank.
    if (std::abs(p.v[3]) > 1e-9 * magnitude || std::abs(p.v[7]) > 1e-9 * magnitude ||
        std::abs(p.v[15]) > 1e-9 * magnitude)
        return false;
    if (std::abs(p.v[11]) <= 1e-12 * magnitude ||
        length({p.v[12], p.v[13], p.v[14]}) <= 1e-12 * magnitude)
        return false;
    Mat4 ignored;
    return inverse(p, ignored);
}
Result fail(Error error) {
    Result r;
    r.error = error;
    return r;
}
} // namespace
Mat4 Mat4::identity() {
    Mat4 m;
    for (int i = 0; i < 4; ++i)
        m.v[i * 5] = 1;
    return m;
}
bool top_face(const Box &b, Rectangle &out) noexcept {
    out = {};
    if (!finite(b.center) || !finite(b.size) || b.size.x <= 0 || b.size.y <= 0 || b.size.z <= 0)
        return false;
    double norm = 0;
    for (double q : b.quaternion) {
        if (!std::isfinite(q))
            return false;
        norm += q * q;
    }
    if (std::abs(norm - 1) > tolerance)
        return false;
    const auto [x, y, z, w] = b.quaternion;
    Mat4 r = Mat4::identity();
    r.v = {1 - 2 * (y * y + z * z),
           2 * (x * y - z * w),
           2 * (x * z + y * w),
           0,
           2 * (x * y + z * w),
           1 - 2 * (x * x + z * z),
           2 * (y * z - x * w),
           0,
           2 * (x * z - y * w),
           2 * (y * z + x * w),
           1 - 2 * (x * x + y * y),
           0,
           0,
           0,
           0,
           1};
    out = {add(b.center, transform(r, {0, b.size.y / 2, 0}, 0)),
           transform(r, {b.size.x / 2, 0, 0}, 0), transform(r, {0, 0, -b.size.z / 2}, 0)};
    return finite(out.center) && finite(out.u) && finite(out.v);
}
Result compute(const Draw &d) noexcept {
    if (!d.tracking_valid)
        return fail(Error::tracking);
    if (!std::isfinite(d.opacity) || d.opacity < 0 || d.opacity > 1)
        return fail(Error::opacity);
    if (!std::isfinite(d.minimum_plane_distance) || d.minimum_plane_distance <= 0)
        return fail(Error::geometry);
    Rectangle s;
    if (!top_face(d.box, s))
        return fail(Error::geometry);
    Mat4 origin_inv, eye_inv;
    if (!rigid(d.raw_to_standing) || !inverse(d.raw_to_standing, origin_inv))
        return fail(Error::origin);
    if (!affine(d.raw_to_eye) || !inverse(d.raw_to_eye, eye_inv))
        return fail(Error::eye);
    if (!central_projection(d.projection))
        return fail(Error::projection);
    Vec3 c = transform(origin_inv, s.center, 1), u = transform(origin_inv, s.u, 0),
         v = transform(origin_inv, s.v, 0), eye = transform(eye_inv, {}, 1);
    Vec3 w = add(c, scale(eye, -1)), n = cross(u, v);
    double ul = length(u), vl = length(v), nl = length(n), det = dot(n, w);
    if (!finite(c) || !finite(u) || !finite(v) || !finite(eye) || !std::isfinite(det) ||
        !std::isfinite(nl) || ul <= 0 || vl <= 0 || nl <= 0 ||
        std::abs(dot(u, v)) > tolerance * ul * vl)
        return fail(Error::numeric);
    if (std::abs(det) / nl < d.minimum_plane_distance)
        return fail(Error::near_plane);
    if (d.one_sided && det >= 0)
        return fail(Error::facing);
    Vec3 rows[3] = {scale(cross(v, w), 1 / det), scale(cross(w, u), 1 / det), scale(n, 1 / det)};
    Result result;
    std::array<float, 4> *dest[3] = {&result.parameters.q0, &result.parameters.q1,
                                     &result.parameters.q2};
    for (int i = 0; i < 3; ++i) {
        double vals[4] = {rows[i].x, rows[i].y, rows[i].z, -dot(rows[i], eye)};
        for (int j = 0; j < 4; ++j) {
            if (!std::isfinite(vals[j]) || std::abs(vals[j]) > std::numeric_limits<float>::max())
                return fail(Error::numeric);
            // Native shader modes do not promise denormal retention on the GPU.
            if (vals[j] != 0 && std::abs(vals[j]) < std::numeric_limits<float>::min())
                return fail(Error::numeric);
            (*dest[i])[j] = static_cast<float>(vals[j]);
        }
    }
    result.parameters.opacity = static_cast<float>(d.opacity);
    result.parameters.epoch_low = static_cast<std::uint32_t>(d.epoch);
    result.parameters.epoch_high = static_cast<std::uint32_t>(d.epoch >> 32);
    result.parameters.flags = 1u | (d.one_sided ? 2u : 0u);
    result.error = Error::none;
    return result;
}
Vec3 evaluate(const Parameters &p, Vec3 x) noexcept {
    auto f = [&](const std::array<float, 4> &q) {
        return q[0] * x.x + q[1] * x.y + q[2] * x.z + q[3];
    };
    return {f(p.q0), f(p.q1), f(p.q2)};
}
bool contains(Vec3 q) noexcept {
    return finite(q) && q.z > 0 && std::abs(q.x) <= q.z && std::abs(q.y) <= q.z;
}
PortalConstants snapshot(const NativeConstants &native, const Result &r) noexcept {
    PortalConstants out{};
    std::memcpy(out.data(), native.data(), native.size());
    if (r)
        std::memcpy(out.data() + 528, &r.parameters, sizeof(r.parameters));
    return out;
}
} // namespace ft::portal
