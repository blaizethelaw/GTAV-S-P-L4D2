#include "portal_math.hpp"
#include "../generated/portal_math.hpp"

#include <cmath>

namespace lsa::portal {
namespace {
constexpr double epsilon = sheets::portal_math::rigid_transform::epsilon;
bool finite(Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
std::optional<Vec3> normalized(Vec3 v) noexcept {
    if (!finite(v)) return std::nullopt;
    const double magnitude = length(v);
    if (!std::isfinite(magnitude) || magnitude <= epsilon) return std::nullopt;
    return v * (1.0 / magnitude);
}
Vec3 rotateUnchecked(const Frame& from, const Frame& to, Vec3 v) noexcept {
    // Local half-turn about portal up reverses right and normal, retaining a
    // proper rotation (determinant +1), hence no speed scaling or reflection.
    return to.right * -dot(v, from.right) + to.up * dot(v, from.up)
         + to.normal * -dot(v, from.normal);
}
}
Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vec3 operator*(Vec3 a, double s) noexcept { return {a.x*s, a.y*s, a.z*s}; }
double dot(Vec3 a, Vec3 b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Vec3 v) noexcept { return std::hypot(v.x, v.y, v.z); }

bool validFrame(const Frame& f) noexcept {
    return finite(f.center) && finite(f.right) && finite(f.up) && finite(f.normal)
        && std::abs(length(f.right)-1.0) <= epsilon
        && std::abs(length(f.up)-1.0) <= epsilon
        && std::abs(length(f.normal)-1.0) <= epsilon
        && std::abs(dot(f.right,f.up)) <= epsilon
        && std::abs(dot(f.right,f.normal)) <= epsilon
        && std::abs(dot(f.up,f.normal)) <= epsilon
        && length(cross(f.right,f.up)-f.normal) <= epsilon;
}

std::optional<Frame> makeFrame(Vec3 center, Vec3 normal, Vec3 upHint) noexcept {
    if (!finite(center)) return std::nullopt;
    auto n = normalized(normal);
    auto hint = normalized(upHint);
    if (!n || !hint) return std::nullopt;
    auto right = normalized(cross(*hint, *n));
    if (!right) return std::nullopt; // Caller selects a nonparallel up hint.
    Frame frame{center, *right, cross(*n,*right), *n};
    return validFrame(frame) ? std::optional<Frame>{frame} : std::nullopt;
}

std::optional<Vec3> rotateVector(const Frame& from, const Frame& to, Vec3 v) noexcept {
    if (!validFrame(from) || !validFrame(to) || !finite(v)) return std::nullopt;
    Vec3 result = rotateUnchecked(from, to, v);
    return finite(result) ? std::optional<Vec3>{result} : std::nullopt;
}

std::optional<Motion> transformMotion(const Frame& from, const Frame& to,
                                      const Motion& motion, double clearance) noexcept {
    if (!validFrame(from) || !validFrame(to) || !finite(motion.position)
        || !finite(motion.velocity) || !finite(motion.forward) || !finite(motion.up)
        || length(motion.forward) <= epsilon || length(motion.up) <= epsilon
        || !std::isfinite(clearance) || clearance < 0.0) return std::nullopt;
    Motion result{
        to.center + rotateUnchecked(from,to,motion.position-from.center) + to.normal*clearance,
        rotateUnchecked(from,to,motion.velocity),
        rotateUnchecked(from,to,motion.forward),
        rotateUnchecked(from,to,motion.up)};
    if (!finite(result.position) || !finite(result.velocity)
        || !finite(result.forward) || !finite(result.up)) return std::nullopt;
    return result;
}

std::optional<Crossing> crossing(const Frame& frame, Aperture aperture,
                                Vec3 previous, Vec3 current, double radius) noexcept {
    if (!validFrame(frame) || !finite(previous) || !finite(current)
        || !std::isfinite(aperture.halfWidth) || !std::isfinite(aperture.halfHeight)
        || !std::isfinite(radius) || radius < 0.0
        || aperture.halfWidth <= radius || aperture.halfHeight <= radius) return std::nullopt;
    const double before = dot(previous-frame.center, frame.normal);
    const double after = dot(current-frame.center, frame.normal);
    if (!std::isfinite(before) || !std::isfinite(after)
        || before <= sheets::portal_math::crossing::epsilon || after > 0.0) return std::nullopt;
    const double t = before / (before-after);
    if (!std::isfinite(t) || t < 0.0 || t > 1.0) return std::nullopt;
    Vec3 hit = previous+(current-previous)*t;
    if (!finite(hit)) return std::nullopt;
    const Vec3 local = hit-frame.center;
    const double x = dot(local,frame.right)/(aperture.halfWidth-radius);
    const double y = dot(local,frame.up)/(aperture.halfHeight-radius);
    if (x*x+y*y > 1.0) return std::nullopt; // Conservative inset ellipse.
    return Crossing{t,hit};
}
} // namespace lsa::portal
