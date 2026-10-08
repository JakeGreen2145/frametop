// SPDX-License-Identifier: MIT
#include "ft_aperture.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
using namespace ft::portal;
static unsigned checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                   \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)
static bool close(double a, double b, double eps = 2e-5) { return std::abs(a - b) < eps; }
static Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 mul(Vec3 a, double b) { return {a.x * b, a.y * b, a.z * b}; }
static Vec3 point(const Mat4 &m, Vec3 p) {
    return {m.v[0] * p.x + m.v[1] * p.y + m.v[2] * p.z + m.v[3],
            m.v[4] * p.x + m.v[5] * p.y + m.v[6] * p.z + m.v[7],
            m.v[8] * p.x + m.v[9] * p.y + m.v[10] * p.z + m.v[11]};
}
static Mat4 product(const Mat4 &a, const Mat4 &b) {
    Mat4 c;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                c.v[i * 4 + j] += a.v[i * 4 + k] * b.v[k * 4 + j];
    return c;
}
static Draw base() {
    Draw d;
    d.tracking_valid = true;
    d.box.center = {0, -2.5, 0};
    d.box.size = {2, 1, 2};
    d.projection.v = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -.1, 0, 0, -1, 0};
    return d;
}
int main() {
    Draw d = base();
    auto r = compute(d);
    CHECK(r);
    CHECK(r.parameters.flags == 1);
    Rectangle face{};
    CHECK(top_face(d.box, face));
    CHECK(close(face.center.y, -2));
    CHECK(close(face.u.x, 1));
    CHECK(close(face.v.z, -1));
    CHECK(contains(evaluate(r.parameters, {0, -2, 0})));
    CHECK(!contains(evaluate(r.parameters, {3, -2, 0})));
    CHECK(!contains(evaluate(r.parameters, {0, 2, 0})));
    const double nan = std::numeric_limits<double>::quiet_NaN(),
                 inf = std::numeric_limits<double>::infinity();
    auto bad = d;
    bad.tracking_valid = false;
    CHECK(compute(bad).error == Error::tracking);
    for (double value : {0., -1., nan, inf}) {
        bad = d;
        bad.box.size.x = value;
        CHECK(!compute(bad));
    }
    bad = d;
    bad.box.center.y = -1e100;
    CHECK(compute(bad).error == Error::numeric); // Q2 cannot be represented by float32
    bad.box.center.y = -1e40;
    CHECK(compute(bad).error == Error::numeric); // native shaders do not promise denormal retention
    bad = d;
    bad.box.size.x = 1e-200;
    CHECK(!compute(bad));
    bad = d;
    bad.box.center.y = nan;
    CHECK(!compute(bad));
    bad = d;
    bad.box.quaternion = {0, 0, 0, 2};
    CHECK(!compute(bad));
    bad = d;
    bad.raw_to_standing.v[0] = 2;
    CHECK(compute(bad).error == Error::origin);
    bad = d;
    bad.raw_to_standing.v[0] = -1;
    CHECK(!compute(bad));
    bad = d;
    bad.raw_to_standing.v[15] = 2;
    CHECK(!compute(bad));
    bad = d;
    bad.raw_to_standing.v[12] = .1;
    CHECK(!compute(bad));
    bad = d;
    bad.raw_to_eye.v[12] = .1;
    CHECK(compute(bad).error == Error::eye);
    bad = d;
    bad.raw_to_eye.v[0] = 0;
    CHECK(!compute(bad));
    bad = d;
    bad.raw_to_eye.v[3] = nan;
    CHECK(!compute(bad));
    bad = d;
    bad.raw_to_eye.v[0] = 2;
    CHECK(compute(bad)); // affine eye need not be orthonormal
    for (int index : {3, 7, 15}) {
        bad = d;
        bad.projection.v[index] = .1;
        CHECK(compute(bad).error == Error::projection);
    }
    bad = d;
    bad.projection = Mat4::identity();
    CHECK(!compute(bad));
    bad = d;
    bad.projection.v[0] = 0;
    CHECK(!compute(bad));
    bad = d;
    bad.projection.v[10] = inf;
    CHECK(!compute(bad));
    for (double value : {0., -1., nan, inf}) {
        bad = d;
        bad.minimum_plane_distance = value;
        CHECK(!compute(bad));
    }
    bad = d;
    bad.box.center.y = -.505;
    CHECK(compute(bad).error == Error::near_plane);
    bad = d;
    bad.box.center.y = -.5;
    CHECK(!compute(bad));
    for (double value : {-.1, 1.1, nan, inf}) {
        bad = d;
        bad.opacity = value;
        CHECK(compute(bad).error == Error::opacity);
    }
    CHECK(!contains({nan, 0, 1}));
    CHECK(!contains({0, inf, 1}));
    CHECK(!contains({0, 0, inf}));
    CHECK(!contains({0, 0, 0}));
    CHECK(contains({1, -1, 1}));
    bad = d;
    bad.one_sided = true;
    CHECK(compute(bad));
    bad.box.center.y = 2;
    CHECK(compute(bad).error == Error::facing);
    // Rotated authoritative box top, full nontrivial quaternion, never center plane.
    const double angle = .8;
    d.box.quaternion = {std::sin(angle / 2), 0, 0, std::cos(angle / 2)};
    CHECK(top_face(d.box, face));
    CHECK(close(face.center.y, -2.5 + .5 * std::cos(angle)));
    CHECK(close(face.center.z, .5 * std::sin(angle)));
    CHECK(close(face.v.y, std::sin(angle)));
    CHECK(close(face.v.z, -std::cos(angle)));
    r = compute(d);
    CHECK(r);
    std::mt19937 gen(80417);
    std::uniform_real_distribution<double> uv(-2, 2), positive(.02, 20);
    // Same Standing geometry represented through a rotated/translated raw origin.
    Mat4 t = Mat4::identity();
    const double a = .47, c = std::cos(a), s = std::sin(a);
    t.v = {c, 0, s, .4, 0, 1, 0, 1.2, -s, 0, c, -.7, 0, 0, 0, 1};
    Mat4 inv = Mat4::identity();
    inv.v = {c, 0, -s, -c * .4 - s * .7, 0, 1, 0, -1.2, s, 0, c, -s * .4 + c * .7, 0, 0, 0, 1};
    unsigned rays = 0;
    for (double eye_x : {-.032, .032}) {
        Draw standing = d;
        standing.raw_to_eye.v[3] = -eye_x;
        auto sr = compute(standing);
        CHECK(sr);
        Draw raw = standing;
        raw.raw_to_standing = t;
        raw.raw_to_eye = product(standing.raw_to_eye, t);
        auto rr = compute(raw);
        CHECK(rr);
        Vec3 eye{eye_x, 0, 0};
        for (int i = 0; i < 10000; ++i) {
            double u = uv(gen), v = uv(gen), scale = positive(gen) * (i % 2 ? -1 : 1);
            Vec3 target = add(face.center, add(mul(face.u, u), mul(face.v, v)));
            Vec3 x = add(eye, mul(add(target, mul(eye, -1)), scale));
            auto q = evaluate(sr.parameters, x), qr = evaluate(rr.parameters, point(inv, x));
            bool exact = scale > 0 && std::abs(u) <= 1 && std::abs(v) <= 1;
            CHECK(contains(q) == exact);
            CHECK(contains(qr) == exact);
            CHECK(close(q.x, qr.x, 1e-4) && close(q.y, qr.y, 1e-4) && close(q.z, qr.z, 1e-4));
            ++rays;
        }
    }
    // Perspective interpolation, including unequal clip w. UV-before-interpolation is wrong.
    Vec3 xs[3] = {{-.9, -1, .2}, {1, -4, -.4}, {.1, -2, .7}};
    double weights[3] = {.2, .3, .5}, cw[3] = {.2, 1.7, 4.1}, sum = 0;
    for (int i = 0; i < 3; ++i) {
        weights[i] /= cw[i];
        sum += weights[i];
    }
    Vec3 x{}, q{};
    for (int i = 0; i < 3; ++i) {
        weights[i] /= sum;
        x = add(x, mul(xs[i], weights[i]));
        q = add(q, mul(evaluate(r.parameters, xs[i]), weights[i]));
    }
    auto expected = evaluate(r.parameters, x);
    CHECK(close(q.x, expected.x) && close(q.y, expected.y) && close(q.z, expected.z));
    NativeConstants original;
    for (size_t i = 0; i < original.size(); ++i)
        original[i] = std::byte(i % 251);
    d.epoch = 0xfedcba9876543210ULL;
    d.opacity = .375;
    r = compute(d);
    CHECK(r);
    CHECK(r.parameters.epoch_low == 0x76543210 && r.parameters.epoch_high == 0xfedcba98);
    auto bytes = snapshot(original, r);
    CHECK(std::memcmp(bytes.data(), original.data(), 528) == 0);
    float opacity;
    std::memcpy(&opacity, bytes.data() + 576, 4);
    CHECK(opacity == .375f);
    auto invalid = snapshot(original, Result{});
    CHECK(std::memcmp(invalid.data(), original.data(), 528) == 0);
    for (size_t i = 528; i < 592; ++i)
        CHECK(invalid[i] == std::byte{0});
    std::printf("PASS aperture checks=%u rays=%u stereo origin invariance, top-face, refusals, "
                "perspective, ABI\n",
                checks, rays);
}
